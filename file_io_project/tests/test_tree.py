#!/usr/bin/env python3
import os
from pathlib import Path
import subprocess
import tempfile
binary = Path(__file__).resolve().parents[1] / 'filecopy'
passed = 0

def run(*args, ok=True):
    result = subprocess.run([str(binary), '--no-color', *map(str, args)],
                            capture_output=True, timeout=15)
    assert (result.returncode == 0) == ok, result.stderr.decode()
    assert b'\x1b[' not in result.stdout
    return result

def good(label):
    global passed
    passed += 1
    print('PASS', label)

with tempfile.TemporaryDirectory() as tmp:
    base = Path(tmp)
    source, dest = base/'source', base/'backup'
    (source/'nested'/'empty').mkdir(parents=True)
    data = os.urandom(10003)
    (source/'nested'/'data.bin').write_bytes(data)
    (source/'tên có khoảng trắng.txt').write_text('Xin chào\n')
    (source/'relative').symlink_to('nested/data.bin')
    (source/'cycle').symlink_to('.')
    (source/'broken').symlink_to('does-not-exist')
    run('-rV', source, dest)
    assert (dest/'nested'/'data.bin').read_bytes() == data
    assert (dest/'nested'/'empty').is_dir()
    assert (dest/'tên có khoảng trắng.txt').read_text() == 'Xin chào\n'
    good('recursive nested/empty folders, binary, UTF-8, verify')
    for name, target in [('relative','nested/data.bin'),('cycle','.'),('broken','does-not-exist')]:
        assert (dest/name).is_symlink() and os.readlink(dest/name) == target
    good('preserve relative, cyclic, dangling symlinks without traversing')
    (dest/'nested'/'data.bin').write_bytes(b'keep')
    run('-rnV', source, dest)
    assert (dest/'nested'/'data.bin').read_bytes() == b'keep'
    good('skip existing files and links')
    run('-rV', source, dest)
    assert (dest/'nested'/'data.bin').read_bytes() == data
    good('overwrite regular file and existing symlinks')
    (dest/'extra').write_text('keep extra')
    run('-r', source, dest)
    assert (dest/'extra').read_text() == 'keep extra'
    good('merge directory preserves unrelated destination files')
    run(source, base/'no-r', ok=False)
    good('directory requires recursive flag')
    run('-r', source, source, ok=False)
    run('-r', source, source/'inside', ok=False)
    assert not (source/'inside').exists()
    good('reject same tree and nested destination before creation')
    run('-r', source/'nested', source, ok=False)
    good('reject destination ancestor of source')
    (base/'alias').symlink_to(source, target_is_directory=True)
    run('-r', source, base/'alias'/'nested', ok=False)
    good('resolve symlink parent for overlap check')
    run('-r', source, base/'missing'/'backup', ok=False)
    good('missing destination parent reports error')
    outside = base/'outside'
    outside.write_bytes(b'protected')
    link = base/'target-link'
    link.symlink_to(outside)
    run(source/'nested'/'data.bin', link, ok=False)
    assert outside.read_bytes() == b'protected'
    good('never overwrite through destination symlink')
    run(source/'relative', base/'copied-link')
    assert os.readlink(base/'copied-link') == 'nested/data.bin'
    good('top-level source symlink copied as link')
    special_src = base/'special'
    special_src.mkdir()
    os.mkfifo(special_src/'fifo')
    run('-r', special_src, base/'special-copy', ok=False)
    good('reject FIFO inside directory without blocking')
    wrong = base/'wrong'
    wrong.write_bytes(b'keep')
    run('-r', source, wrong, ok=False)
    assert wrong.read_bytes() == b'keep'
    good('reject file/directory type conflict without truncating file')
    source_file = source/'nested'/'data.bin'
    hard = base/'hard'
    os.link(source_file, hard)
    run(source_file, hard, ok=False)
    assert source_file.read_bytes() == data
    good('hardlink alias preserves source')
    menu_dest = base/'menu-copy.bin'
    script = f'9\n1\n{source_file}\n{menu_dest}\n2\nY\n0\n'
    result = subprocess.run([str(binary)], input=script, text=True, capture_output=True, timeout=15)
    assert result.returncode == 0 and menu_dest.read_bytes() == data
    assert 'không hợp lệ' in result.stdout
    good('interactive menu validation and full file-copy flow')
    folder_dest = base/'menu-folder'
    script = f'2\n{source}\n{folder_dest}\n2\n\n0\n'
    result = subprocess.run([str(binary)], input=script, text=True, capture_output=True, timeout=15)
    assert result.returncode == 0 and (folder_dest/'nested'/'data.bin').read_bytes() == data
    good('interactive recursive-copy flow with default verification')
    result = run('--help')
    assert b'--verify' in result.stdout
    result = run('--version')
    assert b'4.0' in result.stdout
    run('--unknown', ok=False)
    good('CLI help, version, invalid option, no ANSI in redirected output')
print(f'{passed} tree/UI integration tests passed')
