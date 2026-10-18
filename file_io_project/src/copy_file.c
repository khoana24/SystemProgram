#define _POSIX_C_SOURCE 200809L
#include "copy_file.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
static int open_retry(const char *path, int flags, mode_t permissions)
{
    int fd;
    do { fd = open(path, flags, permissions); } while (fd == -1 && errno == EINTR);
    return fd;
}
static ssize_t read_retry(int fd, void *data, size_t n)
{
    ssize_t result;
    do { result = read(fd, data, n); } while (result == -1 && errno == EINTR);
    return result;
}
static int verify_internal(const char *src, const char *dest,
                           volatile sig_atomic_t *cancelled)
{
    int a = -1, b = -1, saved = 0;
    unsigned char x[BUFFER_SIZE], y[BUFFER_SIZE];
    a = open_retry(src, O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0);
    if (a == -1) return -1;
    b = open_retry(dest, O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0);
    if (b == -1) goto fail;
    struct stat first, second;
    if (fstat(a, &first) == -1 || fstat(b, &second) == -1) goto fail;
    if (!S_ISREG(first.st_mode) || !S_ISREG(second.st_mode)) {
        errno = EINVAL; goto fail;
    }
    for (;;) {
        if (cancelled && *cancelled) { errno = ECANCELED; goto fail; }
        ssize_t n = read_retry(a, x, sizeof x);
        if (n < 0) goto fail;
        if (!n) {
            ssize_t extra = read_retry(b, y, 1);
            if (extra < 0) goto fail;
            if (extra) { errno = EBADMSG; goto fail; }
            break;
        }
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t m = read_retry(b, y + offset, (size_t)(n - offset));
            if (m < 0) goto fail;
            if (!m) { errno = EBADMSG; goto fail; }
            offset += m;
        }
        for (ssize_t i = 0; i < n; ++i)
            if (x[i] != y[i]) { errno = EBADMSG; goto fail; }
    }
    if (close(b) == -1) saved = errno;
    b = -1;
    if (close(a) == -1 && !saved) saved = errno;
    if (saved) { errno = saved; return -1; }
    return 0;
fail:
    saved = errno;
    if (b != -1) close(b);
    if (a != -1) close(a);
    errno = saved;
    return -1;
}
int copy_file_ex(const char *src, const char *dest, const CopyOptions *options)
{
    int in = -1, out = -1, saved = 0;
    struct stat source, target;
    unsigned char buffer[BUFFER_SIZE];
    off_t done = 0;
    if (!src || !dest) { errno = EINVAL; return -1; }
    in = open_retry(src, O_RDONLY | O_NONBLOCK | O_NOFOLLOW, 0);
    if (in == -1) return -1;
    if (fstat(in, &source) == -1) goto fail;
    if (!S_ISREG(source.st_mode)) { errno = EINVAL; goto fail; }
    /* Check aliases before O_TRUNC. Stable paths are a precondition. */
    if (lstat(dest, &target) == 0) {
        if (!S_ISREG(target.st_mode)) { errno = EINVAL; goto fail; }
        if (source.st_dev == target.st_dev && source.st_ino == target.st_ino) {
            errno = EINVAL; goto fail;
        }
    } else if (errno != ENOENT) goto fail;
    if (options && options->cancelled && *options->cancelled) {
        errno = ECANCELED; goto fail;
    }
    out = open_retry(dest, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0666);
    if (out == -1) goto fail;
    if (options && options->progress) options->progress(0, source.st_size, options->user);
    for (;;) {
        if (options && options->cancelled && *options->cancelled) {
            errno = ECANCELED; goto fail;
        }
        ssize_t count = read(in, buffer, sizeof buffer);
        if (count == -1) { if (errno == EINTR) continue; goto fail; }
        if (count == 0) break;
        ssize_t offset = 0;
        while (offset < count) {
            if (options && options->cancelled && *options->cancelled) {
                errno = ECANCELED; goto fail;
            }
            ssize_t written = write(out, buffer + offset, (size_t)(count - offset));
            if (written == -1) { if (errno == EINTR) continue; goto fail; }
            if (!written) { errno = EIO; goto fail; }
            offset += written;
        }
        done += count;
        if (options && options->progress) options->progress(done, source.st_size, options->user);
    }
    /* Verify before restoring metadata: verification reads can change atime. */
    if (options && options->verify && verify_internal(src, dest, options->cancelled) == -1)
        goto fail;
    if (options && options->preserve) {
        struct timespec times[2] = {source.st_atim, source.st_mtim};
        if (fchmod(out, source.st_mode & 0777) == -1) goto fail;
        if (futimens(out, times) == -1) goto fail;
    }
    /* Linux: never retry close, even on EINTR. */
    if (close(out) == -1) saved = errno;
    out = -1;
    if (close(in) == -1 && !saved) saved = errno;
    in = -1;
    if (saved) { errno = saved; return -1; }
    return 0;
fail:
    saved = errno;
    if (out != -1) close(out);
    if (in != -1) close(in);
    errno = saved;
    return -1;
}
int copy_file(const char *src, const char *dest)
{
    return copy_file_ex(src, dest, NULL);
}

int verify_file(const char *src, const char *dest)
{
    return verify_file_ex(src, dest, NULL);
}

int verify_file_ex(const char *src, const char *dest, volatile sig_atomic_t *cancelled)
{
    if (!src || !dest) { errno = EINVAL; return -1; }
    return verify_internal(src, dest, cancelled);
}
