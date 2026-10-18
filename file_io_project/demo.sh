#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
make
sample_dir=$(mktemp -d /tmp/copyflow-demo-XXXXXX)
trap 'rm -rf -- "$sample_dir"' EXIT
mkdir -p "$sample_dir/source/docs" "$sample_dir/source/empty"
printf 'Xin chao CopyFlow\nVi du File I/O.\n' > "$sample_dir/source/docs/readme.txt"
head -c 10003 /dev/urandom > "$sample_dir/source/data.bin"
printf '\nPREVIEW: Plan folder copy without writing destination\n'
./filecopy -r --dry-run "$sample_dir/source" "$sample_dir/backup"
printf '\nDEMO 1: Copy binary file and verify\n'
./filecopy -V "$sample_dir/source/data.bin" "$sample_dir/file-copy.bin"
cmp "$sample_dir/source/data.bin" "$sample_dir/file-copy.bin"
printf '\nDEMO 2: Copy nested folders and verify\n'
./filecopy -rpV "$sample_dir/source" "$sample_dir/backup"
printf '\nDEMO 3: Skip existing files\n'
./filecopy -rn "$sample_dir/source" "$sample_dir/backup"
printf '\nAll demos completed. Temporary demo data will be removed.\n'
