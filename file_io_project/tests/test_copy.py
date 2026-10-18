#!/usr/bin/env python3
"""Integration tests: byte equality, overwrite, errors and aliases."""
import os
from pathlib import Path
import subprocess
import tempfile

binary = Path(__file__).resolve().parents[1] / 'filecopy'
passed = 0

def run(src, dest, success=True):
    result = subprocess.run([str(binary), str(src), str(dest)], capture_output=True)
    assert (result.returncode == 0) == success, result.stderr.decode()

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    src, dst = root / 'source', root / 'destination'
    for size in [0, 1, 4095, 4096, 4097, 10003, 1048576]:
        data = os.urandom(size)
        src.write_bytes(data)
        dst.write_bytes(b'old data' * 200000)
        run(src, dst)
        assert dst.read_bytes() == data
        passed += 1
        print(f'PASS binary {size} bytes + overwrite')
    data = 'Lập trình File I/O\nDữ liệu tiếng Việt.\n'.encode()
    src.write_bytes(data)
    dst.unlink()
    run(src, dst)
    assert dst.read_bytes() == data
    passed += 1
    print('PASS UTF-8 text + create new destination')
    for alias in [src, root / 'hard', root / 'sym']:
        if alias.name == 'hard': os.link(src, alias)
        if alias.name == 'sym': alias.symlink_to(src)
        run(src, alias, False)
        assert src.read_bytes() == data
        passed += 1
        print(f'PASS reject same file: {alias.name}')
    for s, d in [(root/'missing', dst), (src, root/'missing-dir'/'file'),
                 (root, dst), (src, root)]:
        run(s, d, False)
        passed += 1
        print('PASS invalid source/destination')
    fifo = root / 'fifo'
    os.mkfifo(fifo)
    run(fifo, dst, False)
    passed += 1
    print('PASS reject FIFO without blocking')
print(f'{passed} integration tests passed')
