#include "mycp.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int apply_metadata(const char *src, const char *dest,
                   const struct stat *src_stat, int is_symlink)
{
    int result = 0;

    if (is_symlink) {
        if (lchown(dest, src_stat->st_uid, src_stat->st_gid) == -1 &&
            errno != EPERM) {
            perror("lchown");
            result = -1;
        }
    } else {
        if (chown(dest, src_stat->st_uid, src_stat->st_gid) == -1 &&
            errno != EPERM) {
            perror("chown");
            result = -1;
        }

        if (chmod(dest, src_stat->st_mode & 07777) == -1) {
            perror("chmod");
            result = -1;
        }
    }

    struct timespec times[2] = {
        src_stat->st_atim,
        src_stat->st_mtim
    };

    int flags = is_symlink ? AT_SYMLINK_NOFOLLOW : 0;

    if (utimensat(AT_FDCWD, dest, times, flags) == -1) {
        if (!(is_symlink &&
              (errno == ENOTSUP || errno == EOPNOTSUPP ||
               errno == ENOSYS || errno == EINVAL))) {
            perror("utimensat");
            result = -1;
        }
    }

    (void)src;
    return result;
}

static int copy_symlink(const char *src, const char *dest,
                        const struct stat *src_stat)
{
    size_t capacity = src_stat->st_size > 0
                    ? (size_t)src_stat->st_size + 1
                    : 256;

    char *target = NULL;
    ssize_t length;

    while (1) {
        char *new_target = realloc(target, capacity + 1);
        if (new_target == NULL) {
            perror("realloc");
            free(target);
            return -1;
        }

        target = new_target;
        length = readlink(src, target, capacity);

        if (length == -1) {
            perror("readlink");
            free(target);
            return -1;
        }

        if ((size_t)length < capacity) {
            target[length] = '\0';
            break;
        }

        if (capacity > (size_t)-1 / 2) {
            fprintf(stderr, "Symbolic link target is too long\n");
            free(target);
            return -1;
        }

        capacity *= 2;
    }

    struct stat dest_stat;

    if (lstat(dest, &dest_stat) == 0) {
        if (!S_ISLNK(dest_stat.st_mode)) {
            fprintf(stderr, "Refusing to replace non-symlink: %s\n", dest);
            free(target);
            return -1;
        }

        if (unlink(dest) == -1) {
            perror("unlink");
            free(target);
            return -1;
        }
    } else if (errno != ENOENT) {
        perror(dest);
        free(target);
        return -1;
    }

    if (symlink(target, dest) == -1) {
        perror("symlink");
        free(target);
        return -1;
    }

    free(target);
    return apply_metadata(src, dest, src_stat, 1);
}

int copy_path(const char *src, const char *dest)
{
    struct stat src_stat;

    if (lstat(src, &src_stat) == -1) {
        perror(src);
        return -1;
    }

    if (S_ISREG(src_stat.st_mode)) {
        struct stat dest_stat;

        if (stat(dest, &dest_stat) == 0 &&
            src_stat.st_dev == dest_stat.st_dev &&
            src_stat.st_ino == dest_stat.st_ino) {
            fprintf(stderr, "Source and destination are the same file\n");
            return -1;
        }

        if (copy_file_data(src, dest) == -1) {
            return -1;
        }

        return apply_metadata(src, dest, &src_stat, 0);
    }

    if (S_ISDIR(src_stat.st_mode)) {
        if (copy_directory(src, dest) == -1) {
            return -1;
        }

        return apply_metadata(src, dest, &src_stat, 0);
    }

    if (S_ISLNK(src_stat.st_mode)) {
        return copy_symlink(src, dest, &src_stat);
    }

    fprintf(stderr, "Unsupported file type: %s\n", src);
    return -1;
}
