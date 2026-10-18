#!/usr/bin/env python3
"""CopyFlow 4.0: filtering, compare, automatic demo and guided path entry."""
import os
from pathlib import Path
import subprocess
import tempfile

binary = Path(__file__).resolve().parents[1] / 'filecopy'
passed = 0

def good(label):
    global passed
    passed += 1
    print('PASS', label)

def run(*args, cwd=None, code=0):
    result = subprocess.run([str(binary),'--no-color',*map(str,args)],cwd=cwd,
                            capture_output=True,text=True,timeout=15)
    assert result.returncode == code, (result.returncode,result.stdout,result.stderr)
    return result

def menu(script,cwd=None,env=None):
    result = subprocess.run([str(binary)],input=script,text=True,capture_output=True,
                            cwd=cwd,env=env,timeout=15)
    assert result.returncode == 0, result.stderr
    return result

with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp)
    src=root/'source'
    (src/'nested').mkdir(parents=True)
    (src/'.git').mkdir()
    (src/'readme.txt').write_text('keep')
    (src/'one.tmp').write_text('skip')
    (src/'nested'/'two.tmp').write_text('skip')
    (src/'nested'/'keep.bin').write_bytes(bytes(range(256))*30)
    (src/'.git'/'config').write_text('git')
    dest=root/'filtered'
    result=run('-rpV','-x','*.tmp','-x','.git',src,dest)
    assert (dest/'readme.txt').read_text()=='keep'
    assert (dest/'nested'/'keep.bin').read_bytes()==(src/'nested'/'keep.bin').read_bytes()
    assert not (dest/'one.tmp').exists() and not (dest/'nested'/'two.tmp').exists()
    assert not (dest/'.git').exists() and result.stdout.count('[LOAI TRU]')==3
    good('repeatable glob excludes files at any depth and prunes matching directories')
    planned=root/'planned'
    result=run('-r','--dry-run','-x','*.tmp',src,planned)
    assert not planned.exists() and '[LOAI TRU]' in result.stdout
    good('dry-run applies same exclude policy without creating target')
    # Excluding an unsupported item prevents processing it.
    os.mkfifo(src/'pipe.tmp')
    run('-r','-x','*.tmp',src,root/'skip-fifo')
    good('exclude matching special file before trying to copy it')
    os.unlink(src/'pipe.tmp')
    existing=root/'existing'
    existing.mkdir()
    (existing/'one.tmp').write_text('must stay')
    run('-r','-x','*.tmp',src,existing)
    assert (existing/'one.tmp').read_text()=='must stay'
    good('excluded destination content remains unchanged')
    run('-r','-x','source',src,root/'root-not-excluded')
    assert (root/'root-not-excluded'/'readme.txt').exists()
    good('exclude patterns apply to descendants, never exclude requested root')
    result=run('-r','-x','',src,root/'invalid',code=2)
    run('-r',*(['-x','x']*17),src,root/'invalid',code=2)
    assert not (root/'invalid').exists()
    good('reject empty patterns and more than 16 excludes before writing')
    a,b=root/'a.bin',root/'b.bin'
    a.write_bytes(b'\x00\x01ABC\xff')
    b.write_bytes(a.read_bytes())
    result=run('--check',a,b)
    assert 'GIỐNG HỆT NHAU' in result.stdout
    good('independent compare recognizes equal binary files')
    b.write_bytes(b'\x00\x01ABD\xff')
    result=run('--check',a,b,code=1)
    assert 'KHÁC NHAU' in result.stdout and b.read_bytes()==b'\x00\x01ABD\xff'
    good('compare distinguishes equal-length mismatch without overwriting')
    b.write_bytes(a.read_bytes()+b'extra')
    run('--check',a,b,code=1)
    b.write_bytes(b'')
    run('--check',a,b,code=1)
    good('compare detects both longer and shorter destination')
    run('--check',a,root/'missing.bin',code=1)
    run('--check',src,b,code=1)
    assert not (root/'missing.bin').exists()
    good('compare reports missing file or wrong type without creating files')
    run('--check','-V',a,b,code=2)
    run('--demo','--dry-run',code=2)
    good('reject contradictory compare/demo and copy flags')
    target=root/'retry-copy.bin'
    script=f'1\n{root}/missing-source\n{a}\n{src}\n{root}/missing-parent/file.bin\n{target}\n1\nY\n0\n'
    result=menu(script)
    assert target.read_bytes()==a.read_bytes()
    assert 'Nguồn' in result.stdout and 'Thư mục cha' in result.stdout
    good('source validation and destination corrections stay in same wizard')
    env=os.environ.copy();env['HOME']=str(root)
    result=menu('1\n~/a.bin\n~/home-copy.bin\n1\nY\n0\n',env=env)
    assert (root/'home-copy.bin').read_bytes()==a.read_bytes()
    good('interactive paths expand ~/ relative to HOME without executing shell')
    result=menu(f'1\n\n6\n{a}\n{target}\n0\n')
    assert 'GIỐNG HỆT NHAU' in result.stdout
    good('blank source returns to menu, Compare action succeeds')
    menu_dest=root/'menu-filter'
    result=menu(f'7\n{src}\n{menu_dest}\n1\n*.tmp\nY\n0\n')
    assert (menu_dest/'readme.txt').exists() and not (menu_dest/'one.tmp').exists()
    good('interactive Filter action copies with configured glob')
    sandbox=root/'demo-tests';sandbox.mkdir()
    result=run('--demo',cwd=sandbox)
    demos=list(sandbox.glob('copyflow-demo-*'))
    assert len(demos)==1
    demo=demos[0]
    for f in (demo/'source').rglob('*'):
        if f.is_file(): assert f.read_bytes()==(demo/'backup'/f.relative_to(demo/'source')).read_bytes()
    assert (demo/'source'/'binary.bin').stat().st_size==262147
    assert (demo/'backup'/'empty').is_dir()
    good('standalone --demo creates persistent fresh dataset and verified copy')
    result=menu('5\n0\n',cwd=sandbox)
    assert len(list(sandbox.glob('copyflow-demo-*')))==2
    good('menu Demo works without paths and does not overwrite prior demo data')
    result=run(root/'not-there',root/'no-target',code=1)
    assert 'Gợi ý:' in result.stderr and 'menu 5' in result.stderr
    good('copy errors include actionable Vietnamese explanations')
print(f'{passed} feature integration tests passed')
