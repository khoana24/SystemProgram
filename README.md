# SystemProgram - File and Directory Copy

## 1. Overview

This project implements a file and directory copying program in C
using POSIX system calls.

## 2. Features

- Copy regular files using open(), read(), write(), and close().
- Copy directories recursively using opendir(), readdir(), and closedir().
- Detect file types using lstat().
- Copy symbolic links using readlink() and symlink().
- Attempt to preserve file permissions, ownership, access time,
  and modification time.
- Test regular files, hidden files, empty directories, large files,
  symbolic links, and invalid arguments.

## 3. Requirements

- Linux or a POSIX-compatible operating system
- GCC compiler
- GNU Make

## 4. Build

```bash
make
