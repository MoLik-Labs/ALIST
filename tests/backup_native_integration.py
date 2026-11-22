#!/usr/bin/env python3
"""Exercise encrypted snapshot and recovery against a running native server."""
import importlib.util
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.request

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('backup', root / 'tools/backup/kova_backup.py')
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)

def port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0)); return s.getsockname()[1]

def start(data):
    p = port()
    env = os.environ.copy(); env.pop('ALIST_JWT_SECRET', None)
    env.update(ALIST_ADMIN_PASSWORD='BackupTestOnly!234', ALIST_ADDR='127.0.0.1')
    proc = subprocess.Popen([str(root / 'alist'), 'server', '--data', str(data), '--port', str(p)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(150):
        if proc.poll() is not None: raise RuntimeError('server failed to start')
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{p}/ping', timeout=0.25) as resp:
                if resp.read() == b'pong': return proc, p
        except OSError: time.sleep(0.1)
    proc.terminate(); proc.wait(timeout=10); raise RuntimeError('server start timeout')

def login(p):
    payload = json.dumps({'username': 'admin', 'password': 'BackupTestOnly!234'}).encode()
    request = urllib.request.Request(f'http://127.0.0.1:{p}/api/auth/login', payload, {'Content-Type':'application/json'})
    with urllib.request.urlopen(request, timeout=5) as response: return json.load(response)

with tempfile.TemporaryDirectory(prefix='kova-real-backup-') as temp:
    base = Path(temp); data = base / 'data'; out = base / 'backup'; out.mkdir(mode=0o700); keys = base / 'keys'; keys.mkdir(mode=0o700)
    key = keys / 'key'; b.create_key(key)
    proc, p = start(data)
    try:
        assert login(p)['code'] == 200
        b.backup(data, out / 'live.kvbak', key)
        b.unpack(out / 'live.kvbak', key)
    finally:
        proc.terminate(); proc.wait(timeout=15)
    restored = base / 'restore'; b.restore(out / 'live.kvbak', key, restored)
    proc, p = start(restored)
    try: assert login(p)['code'] == 200
    finally: proc.terminate(); proc.wait(timeout=15)
print('Live SQLite snapshot and isolated native-server login after restore passed')
