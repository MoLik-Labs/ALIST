#!/usr/bin/env python3
"""Exercise the entrypoint as the invoking unprivileged user, without Docker."""
import os
from pathlib import Path
import stat
import subprocess
import tempfile

if os.geteuid() == 0:
    raise SystemExit('Run this test as a non-root user')
root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='kova-entrypoint-test-') as tmp:
    tmp = Path(tmp)
    fake = tmp / 'alist'
    fake.write_text('#!/bin/sh\nprintf "ARG:%s\\n" "$@"\n')
    fake.chmod(0o755)
    script = tmp / 'entrypoint.sh'
    script.write_text((root / 'entrypoint.sh').read_text().replace('/opt/alist/alist', str(fake)))
    env = os.environ.copy()
    for key in ('ALIST_DATA_DIR', 'DYNO', 'PUID', 'PGID', 'UMASK', 'TMPDIR', 'KOVA_DISPOSABLE_DEMO'):
        env.pop(key, None)
    env.update(PUID='10001', PGID='10001', TMPDIR=str(tmp))
    data = tmp / 'data with spaces'
    result = subprocess.run(['sh', str(script), 'server', '--port', '1234'], env={**env, 'ALIST_DATA_DIR': str(data)}, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines() == ['ARG:server', 'ARG:--data', f'ARG:{data}', 'ARG:--port', 'ARG:1234']
    assert data.is_dir()
    result = subprocess.run(['sh', str(script), 'server'], env={**env, 'ALIST_DATA_DIR': str(data), 'KOVA_DISPOSABLE_DEMO': 'true'}, text=True, capture_output=True)
    assert result.returncode == 0 and 'Disposable demo' in result.stderr and 'lost' in result.stderr
    # A Heroku-like arbitrary UID must never attempt useradd or chown; the
    # fallback is private, and the warning cannot be mistaken for persistence.
    result = subprocess.run(['sh', str(script), 'server'], env={**env, 'DYNO': 'web.1'}, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    args = result.stdout.splitlines()
    assert args[:2] == ['ARG:server', 'ARG:--data']
    heroku_data = Path(args[2][4:])
    assert heroku_data.parent == tmp and heroku_data.name.startswith('kova.')
    assert stat.S_IMODE(heroku_data.stat().st_mode) == 0o700
    assert 'ephemeral' in result.stderr and 'lost' in result.stderr
    # An explicit persistent path must not be replaced by a temporary one.
    result = subprocess.run(['sh', str(script), 'server'], env={**env, 'DYNO': 'web.1', 'ALIST_DATA_DIR': str(data)}, text=True, capture_output=True)
    assert result.returncode == 0 and f'ARG:{data}' in result.stdout
    assert 'ephemeral' not in result.stderr
    result = subprocess.run(['sh', str(script)], env={**env, 'ALIST_DATA_DIR': str(data), 'PUID': 'invalid'}, text=True, capture_output=True)
    assert result.returncode == 64 and 'non-negative integers' in result.stderr
    readonly = tmp / 'readonly'
    readonly.mkdir(mode=0o500)
    try:
        result = subprocess.run(['sh', str(script)], env={**env, 'ALIST_DATA_DIR': str(readonly)}, text=True, capture_output=True)
        assert result.returncode == 73 and 'not writable' in result.stderr
    finally:
        readonly.chmod(0o700)
print('Native entrypoint non-root, Heroku demo and configuration checks passed')
