#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROGRAM="$PROJECT_DIR/mycp"
TMP_DIR="$(mktemp -d)"

trap 'rm -rf "$TMP_DIR"' EXIT

SRC="$TMP_DIR/source"
DST="$TMP_DIR/copied"

mkdir -p "$SRC/nested/empty"
printf 'Hello, POSIX!\n' > "$SRC/hello.txt"
printf 'hidden content\n' > "$SRC/.hidden"
printf 'nested content\n' > "$SRC/nested/data.txt"

dd if=/dev/urandom of="$SRC/large.bin" \
   bs=1024 count=1024 status=none

ln -s "hello.txt" "$SRC/link-to-file"
ln -s "missing-target" "$SRC/dangling-link"

echo "[1/7] Copy directory tree"
"$PROGRAM" "$SRC" "$DST"

echo "[2/7] Compare regular files"
cmp "$SRC/hello.txt" "$DST/hello.txt"
cmp "$SRC/.hidden" "$DST/.hidden"
cmp "$SRC/nested/data.txt" "$DST/nested/data.txt"
cmp "$SRC/large.bin" "$DST/large.bin"

echo "[3/7] Check empty directory"
test -d "$DST/nested/empty"

echo "[4/7] Check symbolic links"
test -L "$DST/link-to-file"
test "$(readlink "$DST/link-to-file")" = "hello.txt"
test -L "$DST/dangling-link"
test "$(readlink "$DST/dangling-link")" = "missing-target"

echo "[5/7] Copy a single file"
"$PROGRAM" "$SRC/hello.txt" "$TMP_DIR/single.txt"
cmp "$SRC/hello.txt" "$TMP_DIR/single.txt"

echo "[6/7] Check invalid arguments"
if "$PROGRAM" >/dev/null 2>&1; then
    echo "FAIL: missing arguments were accepted"
    exit 1
fi

echo "[7/7] Check permissions"
chmod 640 "$SRC/hello.txt"
"$PROGRAM" "$SRC/hello.txt" "$TMP_DIR/permission-copy.txt"

SRC_MODE="$(stat -c '%a' "$SRC/hello.txt")"
DST_MODE="$(stat -c '%a' "$TMP_DIR/permission-copy.txt")"
test "$SRC_MODE" = "$DST_MODE"

echo "All tests passed."
