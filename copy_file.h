#ifndef COPY_FILE_H
#define COPY_FILE_H
#include <signal.h>
#include <sys/types.h>

typedef struct {
    int verify;
    int preserve; /* rwx bits + original atime/mtime, not ownership/ACL */
    volatile sig_atomic_t *cancelled;
    void (*progress)(off_t done, off_t total, void *user);
    void *user;
} CopyOptions;
/* 0 success, -1 failure with errno. Source must stay unchanged.
 * Path namespace must stay stable; failure may leave a partial destination. */
int copy_file(const char *src, const char *dest);
int copy_file_ex(const char *src, const char *dest, const CopyOptions *options);
/* Verification mismatch: errno=EBADMSG. Other errno values are I/O errors. */
int verify_file(const char *src, const char *dest);
int verify_file_ex(const char *src, const char *dest, volatile sig_atomic_t *cancelled);
#endif
