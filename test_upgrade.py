#!/usr/bin/env python3
"""CopyFlow 3.0: dry-run and metadata checks on real files."""
from pathlib import Path
import os
import stat
import subprocess
import tempfile
binary = Path(__file__).resolve().parents[1] / 'filecopy'
passed = 0

def run(*args, ok=True):
    result = subprocess.run([str(binary), '--no-color', *map(str, args)],
                            capture_output=True, timeout=15)
    assert (result.returncode == 0) == ok, result.stderr.decode()
    return result

def good(message):
    global passed
    passed += 1
    print('PASS', message)

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    src = root/'source'
    (src/'nested'/'empty').mkdir(parents=True)
    data = src/'nested'/'data.bin'
    data.write_bytes(bytes(range(256))*32)
    (src/'link').symlink_to('nested/data.bin')
    dest = root/'preview'
    result = run('-r', '--dry-run', src, dest)
    assert not dest.exists()
    assert b'DU KIEN' in result.stdout
    good('dry-run plans entire nonexistent tree without creating destination')
    run('-r', src, dest)
    file = dest/'nested'/'data.bin'
    file.write_bytes(b'destination content must stay unchanged')
    file.chmod(0o640)
    os.utime(file, ns=(1600000000123456789, 1600000000987654321))
    before = file.stat()
    run('-rpV', '--dry-run', src, dest)
    after = file.stat()
    assert before.st_mode == after.st_mode and before.st_mtime_ns == after.st_mtime_ns
    assert before.st_atime_ns == after.st_atime_ns
    assert file.read_bytes() == b'destination content must stay unchanged'
    good('dry-run -pV does not overwrite data, permissions or timestamps')
    result = run('-rn', '--dry-run', src, dest)
    assert b'BO QUA' in result.stdout
    good('dry-run respects skip-existing policy')
    run('-r', '--dry-run', src, src/'inside', ok=False)
    assert not (src/'inside').exists()
    good('dry-run rejects destination inside source')
    # Capture original times before copy/verify reads modify source atime.
    data.chmod(0o751)
    atime, mtime = 1500000000123456789, 1500000000987654321
    os.utime(data, ns=(atime, mtime))
    single = root/'preserved.bin'
    run('-pV', data, single)
    target = single.stat()
    assert stat.S_IMODE(target.st_mode) == 0o751
    assert target.st_mtime_ns == mtime and target.st_atime_ns == atime
    assert single.read_bytes() == data.read_bytes()
    good('preserve file rwx and nanosecond atime/mtime, even with verify')
    # Directory times must be restored after all children are processed.
    data.chmod(0o640)
    for folder in [src, src/'nested', src/'nested'/'empty']:
        folder.chmod(0o750)
        os.utime(folder, ns=(atime, mtime))
    directory_copy = root/'preserved-tree'
    run('-rpV', src, directory_copy)
    for relative in ['.', 'nested', 'nested/empty']:
        target = (directory_copy/relative).stat()
        assert stat.S_IMODE(target.st_mode) == 0o750
        assert target.st_mtime_ns == mtime and target.st_atime_ns == atime
    assert stat.S_IMODE((directory_copy/'nested'/'data.bin').stat().st_mode) == 0o640
    good('preserve nested folder rwx and timestamps after recursion')
    os.utime(file, ns=(atime, mtime))
    mode_before = stat.S_IMODE(file.stat().st_mode)
    run('-npV', data, file)
    assert file.stat().st_mtime_ns == mtime
    assert stat.S_IMODE(file.stat().st_mode) == mode_before
    assert file.read_bytes() == b'destination content must stay unchanged'
    good('skip-existing file never gets metadata or contents replaced')
    existing = root/'existing.bin'
    existing.write_bytes(b'old')
    existing.chmod(0o600)
    data.chmod(0o644)
    run(data, existing)
    assert stat.S_IMODE(existing.stat().st_mode) == 0o600
    good('without -p existing file permissions stay unchanged')
    menu_target = root/'menu-preview'
    script = f'4\n{src}\n{menu_target}\n1\n0\n'
    result = subprocess.run([str(binary)],input=script,text=True,capture_output=True,timeout=15)
    assert result.returncode == 0 and not menu_target.exists()
    assert 'XEM TRƯỚC' in result.stdout and 'File dự kiến' in result.stdout
    good('interactive Preview flow has explicit planned-only statistics')
print(f'{passed} upgrade integration tests passed')
