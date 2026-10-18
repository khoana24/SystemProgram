#define _XOPEN_SOURCE 700
#include "copy_tree.h"
#include "app_support.h"
#include <fnmatch.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *join(const char *a, const char *b)
{
    size_t n = strlen(a), m = strlen(b);
    if (n > (size_t)-1 - m - 2) { errno = ENAMETOOLONG; return NULL; }
    char *result = malloc(n + m + 2);
    if (result) snprintf(result, n + m + 2, "%s/%s", a, b);
    return result;
}
static int report(CopyJob *job, const char *path)
{
    int saved = errno;
    ++job->errors;
    fprintf(stderr, "  [LOI] %s: %s\n", path, strerror(saved));
    fprintf(stderr, "        Gợi ý: %s\n", error_hint(saved));
    errno = saved;
    return -1;
}
static int contains(const char *root, const char *path)
{
    size_t n = strlen(root);
    return !strcmp(root, path) || (!strncmp(root, path, n) && (n == 1 || path[n] == '/'));
}
/* Resolve a not-yet-existing destination using its existing parent.
 * The CLI requires the destination's parent to exist. */
static char *destination_real(const char *path)
{
    struct stat st;
    if (lstat(path, &st) == 0) return realpath(path, NULL);
    if (errno != ENOENT) return NULL;
    char *tmp = strdup(path);
    if (!tmp) return NULL;
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') tmp[--len] = 0;
    char *slash = strrchr(tmp, '/');
    char *name = strdup(slash ? slash + 1 : tmp);
    if (!name) { free(tmp); return NULL; }
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) {
        free(name); free(tmp); errno = EINVAL; return NULL;
    }
    if (slash) { if (slash == tmp) slash[1] = 0; else *slash = 0; }
    char *parent = realpath(slash ? tmp : ".", NULL);
    char *result = parent ? join(parent, name) : NULL;
    int saved = errno;
    free(parent); free(name); free(tmp);
    errno = saved;
    return result;
}
static int visit(const char *src, const char *dest, CopyJob *job, size_t depth)
{
    if (job->cancelled && *job->cancelled) { errno = ECANCELED; return -1; }
    if (depth > 0) {
        const char *name = strrchr(src, '/');
        name = name ? name + 1 : src;
        for (size_t i = 0; i < job->exclude_count; ++i) {
            if (fnmatch(job->excludes[i], name, 0) == 0) {
                ++job->excluded;
                if (!job->quiet) printf("  [LOAI TRU] %s\n", src);
                return 0;
            }
        }
    }
    struct stat source, target;
    if (lstat(src, &source) == -1) return report(job, src);
    int exists = lstat(dest, &target) == 0;
    if (!exists && errno != ENOENT) return report(job, dest);
    if (exists && source.st_dev == target.st_dev && source.st_ino == target.st_ino) {
        errno = EINVAL; return report(job, dest);
    }
    if (S_ISDIR(source.st_mode)) {
        if (!job->recursive) { errno = EISDIR; return report(job, src); }
        if (exists && !S_ISDIR(target.st_mode)) { errno = ENOTDIR; return report(job, dest); }
        if (!exists && !job->dry_run && mkdir(dest, 0777) == -1) return report(job, dest);
        ++job->folders;
        if (!job->quiet) printf("  [%s] %s\n", job->dry_run ? "DU KIEN / THU MUC" : "THU MUC", dest);
        DIR *dir = opendir(src);
        if (!dir) return report(job, src);
        int failed = 0;
        for (;;) {
            if (job->cancelled && *job->cancelled) { failed = 1; break; }
            errno = 0;
            struct dirent *entry = readdir(dir);
            if (!entry) { if (errno) { report(job, src); failed = 1; } break; }
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
            char *s = join(src, entry->d_name), *d = join(dest, entry->d_name);
            if (!s || !d) { errno = ENOMEM; report(job, src); failed = 1; }
            else if (visit(s, d, job, depth + 1) == -1) failed = 1;
            free(s); free(d);
        }
        if (closedir(dir) == -1) { report(job, src); failed = 1; }
        if (!job->dry_run && job->preserve && !failed) {
            struct timespec times[2] = {source.st_atim, source.st_mtim};
            if (chmod(dest, source.st_mode & 0777) == -1 ||
                utimensat(AT_FDCWD, dest, times, AT_SYMLINK_NOFOLLOW) == -1) {
                report(job, dest); failed = 1;
            }
        }
        return failed ? -1 : 0;
    }
    if (!S_ISREG(source.st_mode) && !S_ISLNK(source.st_mode)) {
        errno = ENOTSUP; return report(job, src);
    }
    if (exists && job->skip_existing) {
        ++job->skipped;
        if (!job->quiet) printf("  [BO QUA] %s\n", dest);
        return 0;
    }
    if (S_ISLNK(source.st_mode)) {
        /* Copy the link itself, never follow it into another tree. */
        size_t size = 256;
        char *text = NULL;
        for (;;) {
            char *next = realloc(text, size + 1);
            if (!next) { free(text); errno = ENOMEM; return report(job, src); }
            text = next;
            ssize_t n = readlink(src, text, size);
            if (n == -1) { int e = errno; free(text); errno = e; return report(job, src); }
            if ((size_t)n < size) { text[n] = 0; break; }
            if (size > (size_t)-1 / 2 - 1) {
                free(text); errno = ENAMETOOLONG; return report(job, src);
            }
            size *= 2;
        }
        if (exists && !S_ISLNK(target.st_mode)) {
            free(text); errno = EEXIST; return report(job, dest);
        }
        if (job->dry_run) {
            ++job->links;
            if (!job->quiet) printf("  [DU KIEN / LIEN KET] %s -> %s\n", dest, text);
            free(text);
            return 0;
        }
        if (exists && unlink(dest) == -1) {
            int e = errno; free(text); errno = e; return report(job, dest);
        }
        int result = symlink(text, dest), saved = errno;
        free(text);
        if (result == -1) { errno = saved; return report(job, dest); }
        ++job->links;
        if (!job->quiet) printf("  [LIEN KET] %s\n", dest);
        return 0;
    }
    if (exists && !S_ISREG(target.st_mode)) { errno = EINVAL; return report(job, dest); }
    if (job->dry_run) {
        ++job->files;
        job->bytes += (uintmax_t)source.st_size;
        if (!job->quiet) printf("  [DU KIEN / %s] %s (%ju byte)\n",
                               exists ? "GHI DE" : "TAO MOI", dest, (uintmax_t)source.st_size);
        return 0;
    }
    if (!job->quiet) printf("  [DANG CHEP] %s\n", src);
    CopyOptions options = {.verify = job->verify, .preserve = job->preserve,
                           .cancelled = job->cancelled, .progress = job->progress,
                           .user = job->user};
    if (copy_file_ex(src, dest, &options) == -1) return report(job, dest);
    ++job->files;
    job->bytes += (uintmax_t)source.st_size;
    if (!job->quiet) printf("  [OK%s] %s\n", job->verify ? " + KIEM TRA" : "", dest);
    return 0;
}
int copy_path(const char *src, const char *dest, CopyJob *job)
{
    if (!src || !dest || !job) { errno = EINVAL; return -1; }
    if (job->exclude_count > 16) { errno = EINVAL; return report(job, src); }
    for (size_t i = 0; i < job->exclude_count; ++i)
        if (!job->excludes[i] || !*job->excludes[i]) { errno = EINVAL; return report(job, src); }
    struct stat info;
    if (lstat(src, &info) == -1) return report(job, src);
    if (S_ISDIR(info.st_mode)) {
        if (!job->recursive) { errno = EISDIR; return report(job, src); }
        char *s = realpath(src, NULL), *d = destination_real(dest);
        if (!s || !d) {
            int e = errno; free(s); free(d); errno = e; return report(job, dest);
        }
        /* Reject same path and both ancestor directions, avoiding self-copy. */
        int overlap = contains(s, d) || contains(d, s);
        free(s); free(d);
        if (overlap) { errno = EINVAL; return report(job, dest); }
    }
    return visit(src, dest, job, 0);
}
