"""Make Windows SDK case-insensitive to Clang on a case-sensitive build host.
Run after sourcing msvc-wine's msvcenv-native.sh. Original header bytes are unchanged.
Only library-name symlinks are added inside the prepared compiler/SDK installation.
"""
import copy
import json
import os
import sys
from pathlib import Path

def contents(path):
    result, seen = [], set()
    for item in sorted(path.iterdir()):
        if item.name.lower() in seen:
            continue
        seen.add(item.name.lower())
        if item.is_dir():
            result.append(dict(type='directory', name=item.name, contents=contents(item)))
        elif item.is_file():
            result.append({'type': 'file', 'name': item.name, 'external-contents': str(item.resolve())})
    return result

roots, seen = [], set()
for entry in os.environ.get('INCLUDE', '').split(';'):
    path = Path(entry)
    if not entry or not path.is_dir():
        continue
    for alias in [path, path.resolve()]:
        if str(alias) not in seen:
            roots.append(dict(type='directory', name=str(alias), contents=contents(path)))
            seen.add(str(alias))
if not roots:
    raise SystemExit('No SDK include roots. Source msvcenv-native.sh first.')
Path(sys.argv[1]).write_text(json.dumps({'version': 0, 'case-sensitive': False, 'roots': roots}))
for entry in os.environ.get('LIB', '').split(';'):
    path = Path(entry)
    if not entry or not path.is_dir():
        continue
    files = {f.name.lower(): f for f in path.iterdir() if f.is_file()}
    for name in ['DbgHelp.lib', 'Dwrite.lib', 'D2d1.lib', 'DXGI.lib', 'D3D11.lib', 'DComp.lib']:
        source, target = files.get(name.lower()), path / name
        if source and not target.exists() and not target.is_symlink():
            target.symlink_to(source.name)
