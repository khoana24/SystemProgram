CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
.PHONY: all test clean
all: filecopy
filecopy: src/main.c src/copy_file.c src/copy_tree.c src/app_support.c include/copy_file.h include/copy_tree.h include/app_support.h
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/copy_file.c src/copy_tree.c src/app_support.c -o $@
tests/fault_test: tests/fault_test.c src/copy_file.c include/copy_file.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/fault_test.c src/copy_file.c -Wl,--wrap=read -Wl,--wrap=write -Wl,--wrap=close -o $@
test: filecopy tests/fault_test
	python3 tests/test_copy.py
	python3 tests/test_tree.py
	python3 tests/test_upgrade.py
	python3 tests/test_features.py
	python3 tests/test_terminal.py
	./tests/fault_test
clean:
	rm -f filecopy tests/fault_test
