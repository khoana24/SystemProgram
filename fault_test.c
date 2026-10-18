#define _POSIX_C_SOURCE 200809L
#include "copy_file.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ssize_t __real_read(int, void *, size_t);
ssize_t __real_write(int, const void *, size_t);
int __real_close(int);
static int mode, reads, writes, closes;
/* 1: short IO + EINTR; 2: write error; 3: zero write;
 * 4: read error; 5: close error after actual close. */
ssize_t __wrap_read(int fd, void *buf, size_t n)
{
    if (mode == 1) {
        if (++reads == 1) { errno = EINTR; return -1; }
        if (n > 13) n = 13;
    }
    if (mode == 4) { errno = EIO; return -1; }
    return __real_read(fd, buf, n);
}
ssize_t __wrap_write(int fd, const void *buf, size_t n)
{
    if (mode == 1) {
        if (++writes == 1) { errno = EINTR; return -1; }
        if (n > 3) n = 3;
    }
    if (mode == 2) { errno = ENOSPC; return -1; }
    if (mode == 3) return 0;
    return __real_write(fd, buf, n);
}
int __wrap_close(int fd)
{
    int result = __real_close(fd);
    if (mode == 5 && ++closes == 1) { errno = EIO; return -1; }
    return result;
}
static int fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");
    assert(dir);
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)))
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    assert(closedir(dir) == 0);
    return count;
}
int main(void)
{
    char src[] = "/tmp/fileio-src-XXXXXX";
    char dst[] = "/tmp/fileio-dst-XXXXXX";
    int a = mkstemp(src), b = mkstemp(dst);
    assert(a >= 0 && b >= 0);
    unsigned char data[10003];
    for (size_t i = 0; i < sizeof data; ++i) data[i] = (unsigned char)(i % 256);
    assert(__real_write(a, data, sizeof data) == (ssize_t)sizeof data);
    assert(__real_close(a) == 0 && __real_close(b) == 0);
    int initial = fd_count();
    mode = 1;
    assert(copy_file(src, dst) == 0);
    mode = 0;
    FILE *file = fopen(dst, "rb");
    unsigned char actual[10003];
    assert(file);
    assert(fread(actual, 1, sizeof actual, file) == sizeof actual);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    assert(memcmp(data, actual, sizeof data) == 0);
    assert(fd_count() == initial);
    puts("PASS forced short read/write + EINTR + byte equality + no fd leak");
    assert(verify_file(src, dst) == 0);
    file = fopen(dst, "ab");
    assert(file && fputc(42, file) != EOF && fclose(file) == 0);
    assert(verify_file(src, dst) == -1 && errno == EBADMSG);
    assert(fd_count() == initial);
    puts("PASS verify detects extra destination byte and releases descriptors");
    volatile sig_atomic_t cancelled = 1;
    CopyOptions options = {0};
    options.cancelled = &cancelled;
    assert(copy_file_ex(src, dst, &options) == -1 && errno == ECANCELED);
    assert(fd_count() == initial);
    puts("PASS cooperative cancellation, no fd leak");
    for (int m = 2; m <= 5; ++m) {
        for (int i = 0; i < 100; ++i) {
            mode = m; reads = writes = closes = 0;
            assert(copy_file(src, dst) == -1);
            assert(errno == (m == 2 ? ENOSPC : EIO));
            mode = 0;
            assert(fd_count() == initial);
        }
        printf("PASS fault mode %d: 100 attempts, correct errno, no fd leak\n", m);
    }
    assert(copy_file(NULL, dst) == -1 && errno == EINVAL);
    assert(copy_file(src, NULL) == -1 && errno == EINVAL);
    assert(fd_count() == initial);
    assert(unlink(src) == 0 && unlink(dst) == 0);
    puts("PASS NULL arguments; all fault tests passed");
    return 0;
}
