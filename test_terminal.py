#!/usr/bin/env python3
"""Exercise ANSI/UTF-8 output and SIGINT using a real pseudo-terminal."""
import os
import pty
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
env = os.environ.copy()
env['TERM'] = 'xterm-256color'
env.pop('NO_COLOR', None)

def start():
    master, slave = pty.openpty()
    process = subprocess.Popen([str(root/'filecopy')], stdin=slave, stdout=slave,
                               stderr=slave, env=env)
    os.close(slave)
    return process, master

def collect(process, master, until=None):
    data = bytearray()
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        ready, _, _ = select.select([master], [], [], .1)
        if ready:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            data.extend(chunk)
            if until and until in data:
                return bytes(data)
        elif process.poll() is not None:
            break
    return bytes(data)

with tempfile.TemporaryDirectory(prefix='copyflow-ui-') as tmp:
    source, target = Path(tmp)/'source.bin', Path(tmp)/'copy.bin'
    source.write_bytes(bytes(range(256))*100)
    process, master = start()
    os.write(master, f'1\n{source}\n{target}\n2\nY\n0\n'.encode())
    data = collect(process, master)
    os.close(master)
    assert process.wait(timeout=3) == 0
    text = data.decode('utf-8')
    assert '\x1b[1;36m' in text and 'COPYFLOW  4.0' in text
    assert '━'*30 in text and '100%' in text
    assert 'HOÀN THÀNH' in text and 'KIEM TRA' in text
    assert source.read_bytes() == target.read_bytes()
    print('PASS PTY: real menu, ANSI, Unicode progress, copy + verify')

process, master = start()
data = collect(process, master, 'Chọn chức năng:'.encode())
assert 'Chọn chức năng:'.encode() in data
process.send_signal(signal.SIGINT)
collect(process, master)
os.close(master)
assert process.wait(timeout=3) == 130
print('PASS PTY: actual SIGINT at menu exits with 130')
