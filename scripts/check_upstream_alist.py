#!/usr/bin/env python3
"""Check the imported upstream baseline has not been truncated or modified."""
import hashlib
import json
from pathlib import Path, PurePosixPath

root = Path(__file__).resolve().parent.parent
source = root / 'upstream' / 'alist'
manifest = json.loads((root / 'upstream' / 'alist.sha256.json').read_text())
if len(manifest) != 859:
    raise SystemExit('Unexpected upstream baseline file count')
for name, expected in manifest.items():
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise SystemExit('Invalid upstream manifest path')
    file = source.joinpath(*path.parts)
    if file.is_symlink() or not file.is_file():
        raise SystemExit(f'Missing or unsafe baseline file: {name}')
    if hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        raise SystemExit(f'Upstream baseline changed: {name}')
print(f'Verified {len(manifest)} files in pinned upstream baseline')
