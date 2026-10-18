#define _POSIX_C_SOURCE 200809L
#include "copy_tree.h"
#include "app_support.h"
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t interrupted;
static int color, terminal, last_percent = -1, bar_open;
static void on_signal(int signal_number) { (void)signal_number; interrupted = 1; }
static const char *paint(const char *s) { return color ? s : ""; }
static void banner(void)
{
    printf("%s\n  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n", paint("\033[1;36m"));
    puts("  ┃  COPYFLOW  4.0                                    ┃");
    puts("  ┃  FILE & FOLDER COPY  /  POSIX C                    ┃");
    printf("  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛%s\n", paint("\033[0m"));
    puts("  LUBUNTU / LINUX    •    BYTE-LEVEL I/O    •    4 KB\n");
}
static void progress(off_t done, off_t total, void *unused)
{
    (void)unused;
    int percent = total > 0 ? (int)((long double)done * 100 / total) : 100;
    if (percent > 100) percent = 100;
    if (done == 0) last_percent = -1;
    if (percent == last_percent) return;
    last_percent = percent;
    printf("\r  %s[", paint("\033[36m"));
    for (int i = 0; i < 30; ++i) fputs(i < percent * 30 / 100 ? "━" : "─", stdout);
    printf("]%s %3d%%  %.2Lf / %.2Lf MiB", paint("\033[0m"), percent,
           (long double)done / 1048576, (long double)total / 1048576);
    bar_open = 1;
    if (percent == 100) { putchar('\n'); bar_open = 0; }
    fflush(stdout);
}
static void help(void)
{
    puts("Cách dùng:");
    puts("  ./filecopy                             Mở menu tương tác");
    puts("  ./filecopy [tùy chọn] NGUỒN ĐÍCH        Sao chép trực tiếp");
    puts("\nTùy chọn:");
    puts("  -r, --recursive       Sao chép thư mục và các thư mục con");
    puts("  -n, --skip-existing   Giữ lại file đích đã tồn tại");
    puts("  -V, --verify          Đọc lại và so sánh từng byte sau khi chép");
    puts("  -p, --preserve        Giữ quyền rwx và atime/mtime của file/folder");
    puts("      --dry-run         Xem kế hoạch, không tạo/ghi/sửa đích");
    puts("  -x, --exclude MẪU     Bỏ tên khớp glob trong cây, tối đa 16 mẫu");
    puts("      --check           Chỉ so sánh 2 file, không sao chép");
    puts("      --demo            Tạo dữ liệu mẫu mới và chạy copy + verify");
    puts("  -q, --quiet           Chỉ in thông tin tác vụ, thống kê và lỗi");
    puts("      --no-color        Tắt màu ANSI");
    puts("  -h, --help            Hướng dẫn sử dụng");
    puts("      --version         Phiên bản chương trình");
    puts("\nVí dụ:");
    puts("  ./filecopy -V photo.jpg photo-copy.jpg");
    puts("  ./filecopy -rpV project backup");
    puts("  ./filecopy -r --dry-run project backup");
    puts("  ./filecopy -rn project backup");
    puts("  ./filecopy -rV -x '*.tmp' -x '.git' project backup");
    puts("  ./filecopy --check source.bin backup.bin");
    puts("  ./filecopy --demo");
    puts("\nĐích là đường dẫn chính xác. project/a.txt -> backup/a.txt.");
    puts("Thư mục cha của đích phải có sẵn. Thư mục đích được tạo nếu thiếu.");
    puts("Mặc định ghi đè regular file có sẵn; dùng -n để bỏ qua.");
    puts("Symlink nguồn được sao chép nguyên liên kết, không đi theo liên kết.");
    puts("Không ghi xuyên qua symlink đích; loại file đặc biệt được báo lỗi.");
    puts("Lỗi/hủy có thể để lại file chép dở. Ctrl+C để yêu cầu dừng.\n");
}
static char *prompt(const char *label)
{
    char *line = NULL;
    size_t size = 0;
    printf("  %s›%s %s", paint("\033[36m"), paint("\033[0m"), label);
    fflush(stdout);
    if (getline(&line, &size, stdin) == -1) { free(line); return NULL; }
    size_t n = strlen(line);
    while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
    if (n >= 2 && ((line[0] == '"' && line[n-1] == '"') ||
                   (line[0] == '\'' && line[n-1] == '\''))) {
        memmove(line, line + 1, n - 2); line[n-2] = 0;
    }
    return line;
}
/* Path prompts do not execute shell syntax. Expand only ~ and ~/... . */
static char *path_prompt(const char *label)
{
    char *path = prompt(label);
    const char *home = getenv("HOME");
    if (path && home && path[0] == '~' && (!path[1] || path[1] == '/')) {
        size_t n = strlen(home), m = strlen(path + 1);
        if (n > (size_t)-1 - m - 1) { free(path); errno = ENAMETOOLONG; return NULL; }
        char *expanded = malloc(n + m + 1);
        if (!expanded) { free(path); return NULL; }
        memcpy(expanded, home, n);
        memcpy(expanded + n, path + 1, m + 1);
        free(path); path = expanded;
    }
    return path;
}
static char *source_prompt(int kind)
{
    for (;;) {
        char *path = path_prompt("Đường dẫn nguồn (trống: về menu): ");
        if (!path || !*path) return path;
        struct stat st;
        if (lstat(path, &st) == -1) {
            int saved = errno;
            printf("  [LOI] Nguồn '%s': %s\n  %s\n", path, strerror(saved), error_hint(saved));
            free(path); continue;
        }
        if ((kind == 1 && S_ISDIR(st.st_mode)) ||
            ((kind == 2 || kind == 7) && !S_ISDIR(st.st_mode)) ||
            (kind == 6 && !S_ISREG(st.st_mode))) {
            puts("  [LOI] Loại nguồn chưa đúng chức năng. File: chọn 1; thư mục: chọn 2; so sánh: 2 file thường.");
            free(path); continue;
        }
        return path;
    }
}
static char *destination_prompt(int kind)
{
    for (;;) {
        char *path = path_prompt(kind == 6 ? "File thứ hai (trống: về menu): " : "Đường dẫn đích (gồm tên file/folder, trống: về menu): ");
        if (!path || !*path) return path;
        struct stat st;
        if (lstat(path, &st) == 0) {
            if ((kind == 6 && !S_ISREG(st.st_mode)) ||
                (kind == 1 && S_ISDIR(st.st_mode)) ||
                ((kind == 2 || kind == 7) && !S_ISDIR(st.st_mode))) {
                puts("  [LOI] Loại đích không phù hợp. Với file, nhập cả tên bản sao; với folder, nhập tên thư mục đích.");
                free(path); continue;
            }
            return path;
        }
        int saved = errno;
        if (saved != ENOENT || kind == 6) {
            printf("  [LOI] %s: %s\n  %s\n", path, strerror(saved), error_hint(saved));
            free(path); continue;
        }
        char *parent = strdup(path);
        if (!parent) { free(path); return NULL; }
        size_t length = strlen(parent);
        while (length > 1 && parent[length-1] == '/') parent[--length] = 0;
        char *slash = strrchr(parent, '/');
        if (slash) { if (slash == parent) slash[1] = 0; else *slash = 0; }
        else { parent[0] = '.'; parent[1] = 0; }
        int valid = stat(parent, &st) == 0 && S_ISDIR(st.st_mode);
        if (!valid) printf("  [LOI] Thư mục cha '%s' chưa có hoặc không phải folder. Tạo thư mục cha trước, rồi nhập lại.\n", parent);
        free(parent);
        if (valid) return path;
        free(path);
    }
}
static double seconds(struct timespec a, struct timespec b)
{
    return (double)(b.tv_sec - a.tv_sec) + (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}
static int execute(const char *src, const char *dest, CopyJob *job)
{
    interrupted = 0;
    job->cancelled = &interrupted;
    job->progress = terminal && !job->quiet ? progress : NULL;
    job->files = job->folders = job->links = job->skipped = job->errors = job->bytes = job->excluded = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("  %sTÁC VỤ  /  %s%s\n", paint("\033[1;36m"),
           job->dry_run ? "XEM TRƯỚC" : "SAO CHÉP", paint("\033[0m"));
    printf("  Nguồn    : %s\n  Đích     : %s\n", src, dest);
    printf("  Trùng tên: %s | Verify: %s | Metadata: %s\n\n",
           job->skip_existing ? "Bỏ qua" : "Ghi đè",
           job->dry_run ? "Không chạy" : job->verify ? "Bật" : "Tắt",
           job->dry_run ? "Không thay đổi" : job->preserve ? "Giữ rwx + thời gian" : "Mặc định");
    if (job->exclude_count) {
        puts("  Bộ lọc loại trừ (theo tên, áp dụng trong cây):");
        for (size_t i = 0; i < job->exclude_count; ++i) printf("    - %s\n", job->excludes[i]);
    }
    if (job->dry_run) puts("  CHỈ XEM TRƯỚC — không tạo, ghi đè hoặc sửa metadata đích.\n");
    int result = copy_path(src, dest, job);
    if (bar_open) { putchar('\n'); bar_open = 0; }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = seconds(start, end);
    int ok = result == 0 && !interrupted;
    printf("\n  %s%s%s\n", paint(ok ? "\033[1;32m" : "\033[1;31m"),
           interrupted ? "ĐÃ DỪNG THEO YÊU CẦU" : ok ? "HOÀN THÀNH" : "HOÀN THÀNH CÓ LỖI", paint("\033[0m"));
    puts("  ────────────────────────────────────────────────────");
    printf("  %-18s %ju\n", job->dry_run ? "File dự kiến" : "File thành công", job->files);
    printf("  Thư mục xử lý      %ju\n  Symlink             %ju\n", job->folders, job->links);
    printf("  Loại trừ            %ju mục\n", job->excluded);
    printf("  Bỏ qua              %ju\n  Lỗi                 %ju\n", job->skipped, job->errors);
    printf("  %s %ju byte\n", job->dry_run ? "Dung lượng dự kiến  " : "Dữ liệu file OK     ", job->bytes);
    printf("  Thời gian           %.3f giây\n", elapsed);
    if (!job->dry_run)
        printf("  Tốc độ tác vụ       %.2f MiB/s\n", elapsed > 0 ? (double)job->bytes / 1048576 / elapsed : 0);
    else
        puts("  Kế hoạch không bảo đảm thao tác ghi thực tế sẽ thành công.");
    puts("  ────────────────────────────────────────────────────\n");
    return interrupted ? 130 : result == 0 ? 0 : 1;
}
static int check_pair(const char *src, const char *dest)
{
    interrupted = 0;
    printf("\n  SO SÁNH ĐỘC LẬP — chỉ đọc, không sao chép\n  A: %s\n  B: %s\n", src, dest);
    int result = verify_file_ex(src, dest, &interrupted);
    int saved = errno;
    if (interrupted) { puts("  ĐÃ DỪNG KIỂM TRA\n"); return 130; }
    if (!result) {
        printf("  %sGIỐNG HỆT NHAU — xác nhận từng byte%s\n\n", paint("\033[1;32m"), paint("\033[0m"));
        return 0;
    }
    if (saved == EBADMSG) puts("  KHÁC NHAU — nội dung hoặc chiều dài không trùng khớp.\n");
    else fprintf(stderr, "  [LOI] So sánh: %s\n  %s\n", strerror(saved), error_hint(saved));
    return 1;
}
static int demo(int quiet)
{
    char *dest = NULL;
    char *src = create_demo_source(&dest);
    if (!src) { int e = errno; fprintf(stderr, "  [LOI] Tạo demo: %s\n  %s\n", strerror(e), error_hint(e)); return 1; }
    puts("\n  DỮ LIỆU MẪU ĐÃ ĐƯỢC TẠO TRÊN MÁY NÀY");
    puts("  Gồm: văn bản tiếng Việt, nhị phân 262147 byte, file .tmp, docs và folder rỗng.");
    puts("  Dữ liệu nằm trong thư mục copyflow-demo-* mới, được giữ lại để bạn thực hành.");
    CopyJob job = {0};
    job.recursive = job.verify = job.preserve = 1;
    job.quiet = quiet;
    int result = execute(src, dest, &job);
    puts("  Bạn có thể chọn menu 7, dùng nguồn ở trên và mẫu *.tmp để thử bộ lọc.\n");
    free(src); free(dest);
    return result;
}
static int menu(void)
{
    for (;;) {
        if (interrupted) return 130;
        banner();
        printf("  %s[1]%s  FILE       Sao chép văn bản / nhị phân\n", paint("\033[36m"), paint("\033[0m"));
        printf("  %s[2]%s  FOLDER     Sao chép thư mục nhiều cấp\n", paint("\033[36m"), paint("\033[0m"));
        printf("  %s[4]%s  PREVIEW    Xem trước kế hoạch sao chép\n", paint("\033[36m"), paint("\033[0m"));
        printf("  %s[5]%s  DEMO       Tạo dữ liệu mẫu và sao chép tự động\n", paint("\033[36m"), paint("\033[0m"));
        printf("  %s[6]%s  COMPARE    So sánh hai file có sẵn\n", paint("\033[36m"), paint("\033[0m"));
        printf("  %s[7]%s  FILTER     Sao chép folder có bộ lọc loại trừ\n", paint("\033[36m"), paint("\033[0m"));
        puts("  [3]  HELP       Hướng dẫn & ví dụ");
        puts("  [0]  EXIT       Thoát\n");
        puts("  Mặc định menu: bỏ qua trùng tên • verify • giữ metadata\n");
        char *choice = prompt("Chọn chức năng: ");
        if (!choice) return interrupted ? 130 : 0;
        int action = !strcmp(choice, "1") ? 1 : !strcmp(choice, "2") ? 2 :
                     !strcmp(choice, "3") ? 3 : !strcmp(choice, "4") ? 4 :
                     !strcmp(choice, "5") ? 5 : !strcmp(choice, "6") ? 6 :
                     !strcmp(choice, "7") ? 7 : !strcmp(choice, "0") ? 0 : -1;
        free(choice);
        if (!action) { puts("  Đã thoát CopyFlow."); return 0; }
        if (action == 3) { help(); continue; }
        if (action == 5) { (void)demo(0); continue; }
        if (action == -1) { puts("  Lựa chọn không hợp lệ. Nhập số từ 0 đến 7."); continue; }
        char *src = source_prompt(action);
        if (!src) return interrupted ? 130 : 0;
        if (!*src) { free(src); continue; }
        char *dest = destination_prompt(action);
        if (!dest) { free(src); return interrupted ? 130 : 0; }
        if (!*src || !*dest) { puts("  Đường dẫn không được để trống."); free(src); free(dest); continue; }
        if (action == 6) {
            (void)check_pair(src, dest);
            free(src); free(dest);
            continue;
        }
        char *policy = prompt("Trùng tên: [1] Ghi đè / [2] Bỏ qua (mặc định 2): ");
        if (!policy) { free(src); free(dest); return interrupted ? 130 : 0; }
        if (*policy && strcmp(policy, "1") && strcmp(policy, "2")) {
            puts("  Chính sách không hợp lệ."); free(src); free(dest); free(policy); continue;
        }
        int skip = strcmp(policy, "1") != 0;
        free(policy);
        if (action == 4) {
            CopyJob preview = {0};
            preview.recursive = 1;
            preview.dry_run = 1;
            preview.skip_existing = skip;
            (void)execute(src, dest, &preview);
            free(src); free(dest);
            continue;
        }
        char *exclude = NULL;
        if (action == 7) {
            exclude = prompt("Loại trừ theo tên (ví dụ *.tmp hoặc .git; trống: không lọc): ");
            if (!exclude) { free(src); free(dest); return interrupted ? 130 : 0; }
        }
        char *verify = prompt("So sánh từng byte sau khi chép? [Y/n]: ");
        if (!verify) { free(exclude); free(src); free(dest); return interrupted ? 130 : 0; }
        if (*verify && strcmp(verify, "Y") && strcmp(verify, "y") && strcmp(verify, "N") && strcmp(verify, "n")) {
            puts("  Nhập Y hoặc n."); free(exclude); free(src); free(dest); free(verify); continue;
        }
        CopyJob job = {0};
        job.recursive = action == 2 || action == 7;
        job.preserve = 1;
        job.skip_existing = skip;
        job.verify = strcmp(verify, "n") && strcmp(verify, "N");
        if (exclude && *exclude) { job.excludes[0] = exclude; job.exclude_count = 1; }
        free(verify);
        (void)execute(src, dest, &job);
        free(exclude);
        free(src); free(dest);
        if (interrupted) return 130;
    }
}
int main(int argc, char **argv)
{
    terminal = isatty(STDOUT_FILENO);
    color = terminal && !getenv("NO_COLOR") && getenv("TERM") && strcmp(getenv("TERM"), "dumb");
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1) { perror("sigaction"); return 1; }
    if (argc == 1) return menu();
    CopyJob job = {0};
    int check_only = 0, demo_only = 0;
    const struct option opts[] = {
        {"recursive", no_argument, NULL, 'r'}, {"skip-existing", no_argument, NULL, 'n'},
        {"verify", no_argument, NULL, 'V'}, {"preserve", no_argument, NULL, 'p'},
        {"dry-run", no_argument, NULL, 1002}, {"quiet", no_argument, NULL, 'q'},
        {"no-color", no_argument, NULL, 1000}, {"version", no_argument, NULL, 1001},
        {"exclude", required_argument, NULL, 'x'}, {"check", no_argument, NULL, 1003},
        {"demo", no_argument, NULL, 1004},
        {"help", no_argument, NULL, 'h'}, {NULL, 0, NULL, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "rnVpqhx:", opts, NULL)) != -1) {
        switch (option) {
            case 'r': job.recursive = 1; break;
            case 'n': job.skip_existing = 1; break;
            case 'V': job.verify = 1; break;
            case 'p': job.preserve = 1; break;
            case 'q': job.quiet = 1; break;
            case 'x':
                if (!*optarg || job.exclude_count == 16) {
                    fputs("Mẫu loại trừ không được trống, tối đa 16 mẫu.\n", stderr); return 2;
                }
                job.excludes[job.exclude_count++] = optarg; break;
            case 'h': help(); return 0;
            case 1000: color = 0; break;
            case 1003: check_only = 1; break;
            case 1004: demo_only = 1; break;
            case 1002: job.dry_run = 1; break;
            case 1001: puts("CopyFlow 4.0 - POSIX C / Lubuntu / Linux"); return 0;
            default: return 2;
        }
    }
    if ((check_only || demo_only) && (job.recursive || job.skip_existing || job.verify ||
        job.preserve || job.dry_run || job.exclude_count || (check_only && demo_only))) {
        fputs("--check/--demo dùng riêng, không kết hợp tùy chọn sao chép.\n", stderr); return 2;
    }
    if (demo_only) {
        if (argc != optind) { help(); return 2; }
        banner(); return demo(job.quiet);
    }
    if (argc - optind != 2) { help(); return 2; }
    banner();
    if (check_only) return check_pair(argv[optind], argv[optind + 1]);
    return execute(argv[optind], argv[optind + 1], &job);
}
