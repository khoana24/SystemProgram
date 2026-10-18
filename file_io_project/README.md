# CopyFlow — File & Folder Copy

Chương trình C/POSIX cho môn **Lập trình hệ thống**, chạy trong Terminal trên **Lubuntu/Linux**. Sao chép nội dung file theo byte bằng `open()`, `read()`, `write()`, `close()` với buffer **4096 byte**.

## Chức năng

- Sao chép file văn bản, file nhị phân và cây thư mục nhiều cấp, kể cả thư mục rỗng.
- Ghi đè hoặc bỏ qua file đích đã có.
- Xem trước kế hoạch bằng `--dry-run`, không ghi dữ liệu đích.
- So sánh từng byte sau khi chép hoặc so sánh hai file độc lập.
- Giữ quyền rwx và atime/mtime của regular file và thư mục với `--preserve`.
- Loại trừ tên file/thư mục theo glob, ví dụ `*.tmp`, `.git`.
- Giữ nguyên symbolic link, không đi theo liên kết khi đệ quy.
- Menu tiếng Việt, màu ANSI, thanh tiến trình từng file, thống kê và Ctrl+C.
- Tự tạo dữ liệu mẫu để demo mà không cần chuẩn bị file nguồn.

## Cài đặt và chạy trên Lubuntu

Cài công cụ nếu chưa có:

```bash
sudo apt update
sudo apt install build-essential python3 git
```

Mở Terminal trong thư mục dự án:

```bash
make
./filecopy
```

**Người mới nên chọn `5` rồi Enter.** Chương trình tự tạo dữ liệu mẫu và chạy sao chép kèm verify. Mẫu được giữ trong thư mục `copyflow-demo-*` tại vị trí đang chạy.

| Menu | Chức năng |
|---|---|
| 1 — FILE | Sao chép file văn bản / nhị phân |
| 2 — FOLDER | Sao chép thư mục nhiều cấp |
| 3 — HELP | Hướng dẫn và ví dụ |
| 4 — PREVIEW | Xem kế hoạch trước khi ghi |
| 5 — DEMO | Tạo dữ liệu mẫu mới và sao chép tự động |
| 6 — COMPARE | So sánh hai file có sẵn |
| 7 — FILTER | Sao chép folder với một glob loại trừ |
| 0 — EXIT | Thoát |

Trong menu, nguồn sai hoặc thiếu thư mục cha của đích được báo ngay để nhập lại. Enter trống tại bước đường dẫn quay về menu. Có thể nhập `~/Documents/...`; không thực thi câu lệnh shell trong prompt. Khi copy file, đích phải gồm tên bản sao, không chỉ là thư mục chứa.

Menu copy mặc định bỏ qua file đã tồn tại, bật verify và preserve. Trên CLI, mặc định ghi đè, không verify và không preserve.

## Ví dụ dòng lệnh

```bash
# File và kiểm tra sau sao chép
./filecopy -V source.txt destination.txt

# Cây thư mục: giữ quyền/thời gian và so sánh dữ liệu
./filecopy -rpV source_dir backup_dir

# Bỏ qua file đã có
./filecopy -rn source_dir backup_dir

# Xem trước, không ghi đích
./filecopy -r --dry-run source_dir backup_dir

# Loại trừ tên khớp một trong các mẫu ở mọi cấp
./filecopy -rpV -x '*.tmp' -x '.git' source_dir backup_dir

# Chỉ so sánh hai regular file có sẵn
./filecopy --check source.bin destination.bin

# Tự tạo mẫu và chạy demo
./filecopy --demo

# Hướng dẫn và phiên bản
./filecopy --help
./filecopy --version
```

Đặt đường dẫn có khoảng trắng trong dấu nháy. Dùng `--` trước đường dẫn bắt đầu bằng dấu trừ.

Đích là đường dẫn **chính xác**: `source_dir/a.txt` được chép thành `backup_dir/a.txt`. Không tự thêm `source_dir` vào bên trong `backup_dir`. Thư mục cha của đích phải có sẵn. Folder đích đã tồn tại được merge; file không liên quan không bị xóa.

`-x` nhận tối đa 16 mẫu không rỗng, khớp basename ở mọi cấp bằng `fnmatch()`. Không lọc chính nguồn gốc được yêu cầu. Loại một thư mục thì bỏ cả nhánh; không xóa bản đích cũ của mục bị loại. Bộ đếm loại trừ tách khỏi bộ đếm bỏ qua trùng tên.

`--check` và `--demo` dùng riêng, không kết hợp tùy chọn sao chép. Có thể dùng `--no-color` hoặc `-q`. Mã thoát CLI: `0` thành công/giống nhau, `1` lỗi/khác nhau, `2` sai tham số, `130` hủy.

## Demo và kiểm thử

```bash
bash demo.sh
make test
```

`demo.sh` dùng dữ liệu tạm và tự dọn khi kết thúc. Menu 5 hoặc `--demo` giữ lại mẫu để thực hành.

Bộ kiểm thử gồm **61 ca tích hợp**, thêm **2 ca Terminal/Signal** và fault injection:

- File rỗng, UTF-8, nhị phân và các kích thước quanh mốc 4096 byte.
- Ghi đè đích dài hơn, skip, verify, cùng inode, lỗi đường dẫn và loại file.
- Folder nhiều cấp/rỗng, symlink vòng/hỏng, chặn cây nguồn/đích chồng lấn.
- Dry-run không sửa đích; preserve quyền và thời gian; filter, compare và dữ liệu mẫu.
- Menu nhập sai rồi sửa, mở rộng HOME, màu ANSI, tiến trình và SIGINT.
- Ép short read/write, EINTR, ENOSPC, write trả 0, read/close EIO; kiểm tra errno và descriptor qua `/proc/self/fd`.

Đã kiểm thử trên Linux. Chạy `make test` trên máy Lubuntu sử dụng thực tế để xác nhận môi trường. Fault injection là lỗi mô phỏng, không phải thử nghiệm mất điện hoặc hỏng phần cứng.

## Cấu trúc dự án

| Đường dẫn | Trách nhiệm |
|---|---|
| `src/copy_file.c` | Byte-level copy, verify, preserve và cleanup |
| `src/copy_tree.c` | Duyệt cây, tạo folder, symlink và bộ lọc |
| `src/app_support.c` | Tạo dữ liệu mẫu, gợi ý lỗi tiếng Việt |
| `src/main.c` | Menu, CLI, tiến trình, thống kê và SIGINT |
| `include/` | Header và API |
| `tests/` | Kiểm thử tự động và fault injection |
| `docs/BAO_CAO.md` | Thuyết minh thuật toán, thiết kế và đánh giá |
| `Makefile` | Build, test, clean |
| `demo.sh` | Kịch bản demo |
| `.gitignore` | Bỏ qua executable, dữ liệu mẫu và file tạm |

API cơ bản:

```c
int copy_file(const char *src, const char *dest);
```

Trả `0` khi thành công, `-1` khi lỗi với nguyên nhân trong `errno`. API mở rộng cung cấp verify, preserve, callback tiến trình và cờ hủy. Xem header để biết cấu hình. `EBADMSG` biểu thị khác dữ liệu khi verify; khác với `EIO` của lỗi đọc/ghi.

## Xử lý I/O và giới hạn

Chương trình ghi đúng số byte thực tế đã đọc, lặp đến khi hoàn tất partial write và thử lại read/write bị EINTR. Không dùng `strlen()` với nội dung file. Descriptor được đóng ở mọi nhánh, giữ errno gốc; kiểm tra lỗi close và không retry close trên Linux.

- Nguồn và đường dẫn cần ổn định trong quá trình chạy; kiểm tra đường dẫn trước khi mở vẫn có giới hạn TOCTOU.
- O_TRUNC xóa nội dung đích cũ khi bắt đầu ghi. Lỗi/hủy có thể để đích dở; chưa có rollback, atomic replacement hoặc fsync.
- Preserve chỉ giữ rwx và atime/mtime của regular file/folder; không giữ ownership, ACL, xattr, bit quyền đặc biệt, sparse layout, hard-link layout hoặc metadata symlink.
- Symlink giữ nguyên chuỗi liên kết; không theo symlink đích để ghi. FIFO/socket/device không được sao chép.
- Verify đọc lại dữ liệu; thao tác đọc có thể cập nhật atime. Giữ thời gian đích được thực hiện sau verify khi bật preserve.
- Dry-run chỉ là kế hoạch, không bảo đảm quyền/dung lượng khi chạy thật. Nguồn có thể được hệ điều hành cập nhật atime khi duyệt.
- Thanh tiến trình là tiến độ ghi từng file. Dòng OK sau đó mới xác nhận các bước verify, metadata và close đã hoàn tất.
- Tốc độ là trung bình tác vụ, gồm verify nếu bật, chỉ tính byte của file thành công; không phải benchmark ổ đĩa thuần.
- Đệ quy dùng stack và một DIR mỗi cấp, cây quá sâu có thể chạm giới hạn tài nguyên.

## Đưa lên GitHub lần đầu

Tạo một repository **trống**, ví dụ `copyflow-posix`, chưa thêm README, license hay .gitignore trên GitHub. Trong thư mục chứa Makefile của dự án:

```bash
git init
git branch -M main
git add .
git status
git commit -m "Initial commit: CopyFlow POSIX file and folder copy"
git remote add origin https://github.com/TEN_GITHUB/copyflow-posix.git
git push -u origin main
```

Thay `TEN_GITHUB` bằng tên tài khoản thật và dùng URL chính xác của repo đã tạo. Nếu Git yêu cầu thông tin tác giả, đặt tên/email của bạn bằng `git config user.name` và `git config user.email` rồi commit lại. Bước push cần xác thực tài khoản GitHub theo cách bạn đã thiết lập.

`.gitignore` tự loại file build và dữ liệu demo khỏi lần add đầu. Nếu những file đó đã được commit trước đây, `.gitignore` không tự bỏ chúng khỏi lịch sử hoặc danh sách tracked.
