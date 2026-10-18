#include "mycp.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

int copy_file_data(const char *src, const char *dest)
{
    int src_fd = -1;
    int dest_fd = -1;
    int result = -1;
    char buffer[4096];
    struct stat src_stat;
    struct stat dest_stat;

    src_fd = open(src, O_RDONLY | O_NOFOLLOW);
    if (src_fd == -1) {
        perror(src);
        goto cleanup;
    }

    if (fstat(src_fd, &src_stat) == -1) {
        perror("fstat source");
        goto cleanup;
    }

    if (!S_ISREG(src_stat.st_mode)) {
        fprintf(stderr, "Source is not a regular file: %s\n", src);
        goto cleanup;
    }

    dest_fd = open(dest, O_WRONLY | O_CREAT | O_NOFOLLOW, 0600);
    if (dest_fd == -1) {
        perror(dest);
        goto cleanup;
    }

    if (fstat(dest_fd, &dest_stat) == -1) {
        perror("fstat destination");
        goto cleanup;
    }

    if (src_stat.st_dev == dest_stat.st_dev &&
        src_stat.st_ino == dest_stat.st_ino) {
        fprintf(stderr, "Source and destination are the same file\n");
        goto cleanup;
    }

    if (ftruncate(dest_fd, 0) == -1) {
        perror("ftruncate");
        goto cleanup;
    }

    while (1) {
        ssize_t bytes_read = read(src_fd, buffer, sizeof(buffer));

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("read");
            goto cleanup;
        }

        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(
                dest_fd,
                buffer + total_written,
                (size_t)(bytes_read - total_written)
            );

            if (bytes_written == -1) {
                if (errno == EINTR) {
                    continue;
                }

                perror("write");
                goto cleanup;
            }

            if (bytes_written == 0) {
                fprintf(stderr, "write: no progress\n");
                goto cleanup;
            }

            total_written += bytes_written;
        }
    }

    result = 0;

cleanup:
    if (src_fd != -1 && close(src_fd) == -1) {
        perror("close source");
        result = -1;
    }

    if (dest_fd != -1 && close(dest_fd) == -1) {
        perror("close destination");
        result = -1;
    }

    return result;
}
