#define _XOPEN_SOURCE 700
#include "app_support.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *error_hint(int code)
{
    switch (code) {
        case ENOENT: return "Đường dẫn chưa tồn tại. Kiểm tra tên hoặc dùng menu 5 để tạo dữ liệu mẫu.";
        case EACCES: case EPERM: return "Không đủ quyền. Kiểm tra quyền đọc nguồn và ghi thư mục đích.";
        case EISDIR: return "Nguồn là thư mục. Dùng chức năng Folder hoặc tùy chọn -r.";
        case ENOTDIR: return "Một thành phần phải là thư mục nhưng đang là file. Kiểm tra lại đường dẫn.";
        case EINVAL: return "Kiểm tra loại file, nguồn trùng đích hoặc hai cây thư mục chồng lấn.";
        case ELOOP: return "Không theo symlink đích. Chọn một file đích thông thường.";
        case ENOSPC: return "Ổ đĩa hết chỗ. File đích có thể đang chép dở.";
        case EROFS: return "Vị trí đích chỉ đọc. Chọn thư mục có thể ghi.";
        case ENOTSUP: return "Chỉ hỗ trợ file thường, thư mục và symlink; không chép FIFO/socket/device.";
        case EEXIST: return "Trùng tên nhưng khác loại file. Đổi tên đích hoặc dùng chế độ bỏ qua.";
        case ECANCELED: return "Đã nhận yêu cầu dừng. File đang sao chép có thể còn dở.";
        case EBADMSG: return "Nội dung hai file khác nhau; chưa xác nhận bản sao hợp lệ.";
        case EMFILE: case ENFILE: return "Quá nhiều file đang mở; cây thư mục có thể quá sâu.";
        default: return "Xem mã lỗi hệ thống phía trên và kiểm tra nguồn, đích, thiết bị lưu trữ.";
    }
}
static char *path_join(const char *a, const char *b)
{
    size_t n = strlen(a), m = strlen(b);
    if (n > (size_t)-1 - m - 2) { errno = ENAMETOOLONG; return NULL; }
    char *path = malloc(n + m + 2);
    if (path) snprintf(path, n + m + 2, "%s/%s", a, b);
    return path;
}
static int sample_file(const char *root, const char *name, size_t bytes, const char *text)
{
    char *path = path_join(root, name);
    if (!path) return -1;
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    int saved = errno;
    free(path);
    if (fd == -1) { errno = saved; return -1; }
    unsigned char data[4096];
    size_t done = 0;
    if (text) bytes = strlen(text);
    while (done < bytes) {
        size_t n = bytes - done < sizeof data ? bytes - done : sizeof data;
        if (text) memcpy(data, text + done, n);
        else for (size_t i = 0; i < n; ++i) data[i] = (unsigned char)((done + i) % 256);
        size_t offset = 0;
        while (offset < n) {
            ssize_t written = write(fd, data + offset, n - offset);
            if (written == -1 && errno == EINTR) continue;
            if (written <= 0) {
                saved = written == 0 ? EIO : errno;
                close(fd); errno = saved; return -1;
            }
            offset += (size_t)written;
        }
        done += n;
    }
    return close(fd);
}
char *create_demo_source(char **destination)
{
    if (!destination) { errno = EINVAL; return NULL; }
    *destination = NULL;
    char temporary[] = "copyflow-demo-XXXXXX";
    if (!mkdtemp(temporary)) return NULL;
    char *root = realpath(temporary, NULL);
    if (!root) return NULL;
    char *source = path_join(root, "source");
    char *target = path_join(root, "backup");
    char *docs = source ? path_join(source, "docs") : NULL;
    char *empty = source ? path_join(source, "empty") : NULL;
    if (!source || !target || !docs || !empty) { errno = ENOMEM; goto fail; }
    if (mkdir(source, 0700) || mkdir(docs, 0700) || mkdir(empty, 0700)) goto fail;
    if (sample_file(source, "readme.txt", 0, "Xin chào CopyFlow 4.0!\nĐây là dữ liệu mẫu được tạo ngay trên Lubuntu.\n") ||
        sample_file(source, "docs/lesson.txt", 0, "open → read → write → close\n") ||
        sample_file(source, "binary.bin", 262147, NULL) ||
        sample_file(source, "notes.tmp", 0, "Có thể loại trừ file này bằng -x '*.tmp'.\n")) goto fail;
    free(root); free(docs); free(empty);
    *destination = target;
    return source;
fail:
    {
        int saved = errno;
        fprintf(stderr, "  Dữ liệu mẫu chưa hoàn tất, xem thư mục: %s\n", root);
        free(root); free(source); free(target); free(docs); free(empty);
        errno = saved;
        return NULL;
    }
}
