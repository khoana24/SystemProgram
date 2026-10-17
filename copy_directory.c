#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/**
 * Hàm hỗ trợ sao chép file thông thường (Regular File).
 * (Nếu nhóm bạn có thành viên khác phụ trách hàm copy_file riêng,
 *  bạn có thể thay thế phần thân hàm này bằng hàm của bạn đó).
 */
int copy_file(const char *src_path, const char *dest_path) {
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        perror("Lỗi mở file nguồn");
        return -1;
    }

    // Lấy thông tin quyền (mode) của file nguồn để áp dụng cho file đích
    struct stat st;
    mode_t mode = 0644;
    if (fstat(src_fd, &st) == 0) {
        mode = st.st_mode & 0777;
    }

    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dest_fd < 0) {
        perror("Lỗi tạo file đích");
        close(src_fd);
        return -1;
    }

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t n = write(dest_fd, buffer + bytes_written, bytes_read - bytes_written);
            if (n < 0) {
                perror("Lỗi ghi dữ liệu vào file đích");
                close(src_fd);
                close(dest_fd);
                return -1;
            }
            bytes_written += n;
        }
    }

    if (bytes_read < 0) {
        perror("Lỗi đọc file nguồn");
        close(src_fd);
        close(dest_fd);
        return -1;
    }

    close(src_fd);
    close(dest_fd);
    return 0;
}

/**
 * Hàm đệ quy duyệt và sao chép cấu trúc thư mục từ src_dir sang dest_dir.
 * Sử dụng các hàm POSIX: opendir(), readdir(), closedir(), mkdir().
 *
 * Trả về: 0 nếu thành công, -1 nếu có lỗi.
 */
int copy_directory(const char *src_dir, const char *dest_dir) {
    // 1. Lấy thông tin thư mục nguồn để giữ nguyên quyền (permissions)
    struct stat src_stat;
    mode_t dir_mode = 0755; // Quyền mặc định rwxr-xr-x nếu không lấy được stat
    if (stat(src_dir, &src_stat) == 0) {
        dir_mode = src_stat.st_mode & 0777;
    }

    // 2. Tạo thư mục đích bằng mkdir()
    // Nếu thư mục đã tồn tại (EEXIST) thì vẫn tiếp tục duyệt bên trong
    if (mkdir(dest_dir, dir_mode) == -1) {
        if (errno != EEXIST) {
            fprintf(stderr, "Lỗi tạo thư mục đích '%s': %s\n", dest_dir, strerror(errno));
            return -1;
        }
    } else {
        printf("[MKDIR] Đã tạo thư mục: %s\n", dest_dir);
    }

    // 3. Mở thư mục nguồn bằng opendir()
    DIR *dir = opendir(src_dir);
    if (dir == NULL) {
        fprintf(stderr, "Lỗi mở thư mục nguồn '%s': %s\n", src_dir, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    char src_path[PATH_MAX];
    char dest_path[PATH_MAX];

    // 4. Duyệt qua từng mục (entry) trong thư mục bằng readdir()
    while ((entry = readdir(dir)) != NULL) {
        // LƯU Ý TỬ HUYỆT: Bỏ qua thư mục hiện tại "." và thư mục cha ".."
        // để tránh đệ quy vô tận gây tràn bộ nhớ ngăn xếp (Stack Overflow).
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 5. Xử lý đường dẫn (Path Manipulation) an toàn với snprintf()
        // Kiểm tra giá trị trả về để chống tràn bộ đệm (Buffer Overflow)
        int src_len = snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        int dest_len = snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

        if (src_len < 0 || (size_t)src_len >= sizeof(src_path) ||
            dest_len < 0 || (size_t)dest_len >= sizeof(dest_path)) {
            fprintf(stderr, "Lỗi: Đường dẫn quá dài (vượt quá %d ký tự) tại '%s'\n",
                    PATH_MAX, entry->d_name);
            continue;
        }

        // 6. Kiểm tra loại mục hiện tại (thư mục con hay file)
        struct stat entry_stat;
        if (stat(src_path, &entry_stat) == -1) {
            fprintf(stderr, "Lỗi đọc thông tin '%s': %s\n", src_path, strerror(errno));
            continue;
        }

        if (S_ISDIR(entry_stat.st_mode)) {
            // ĐỆ QUY: Nếu là thư mục con, gọi lại hàm copy_directory()
            if (copy_directory(src_path, dest_path) == -1) {
                fprintf(stderr, "Cảnh báo: Không thể sao chép toàn bộ thư mục con '%s'\n", src_path);
            }
        } else if (S_ISREG(entry_stat.st_mode)) {
            // Nếu là file thông thường, gọi hàm copy_file()
            if (copy_file(src_path, dest_path) == 0) {
                printf("[FILE]  Đã sao chép: %s -> %s\n", src_path, dest_path);
            }
        }
    }

    // 7. Đóng thư mục sau khi duyệt xong bằng closedir()
    if (closedir(dir) == -1) {
        fprintf(stderr, "Lỗi đóng thư mục '%s': %s\n", src_dir, strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Cách dùng: %s <thư_mục_nguồn> <thư_mục_đích>\n", argv[0]);
        fprintf(stderr, "Ví dụ:     %s ./folderA ./folderB\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *src_dir = argv[1];
    const char *dest_dir = argv[2];

    printf("Bắt đầu sao chép từ '%s' sang '%s'...\n", src_dir, dest_dir);
    if (copy_directory(src_dir, dest_dir) == 0) {
        printf("Hoàn tất sao chép thư mục thành công!\n");
        return EXIT_SUCCESS;
    } else {
        fprintf(stderr, "Quá trình sao chép gặp lỗi.\n");
        return EXIT_FAILURE;
    }
}

