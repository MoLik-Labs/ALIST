#!/usr/bin/env python3
"""Optional local Kova metadata backup. Never backs up mounted files or NodeWarden."""
import argparse
import base64
import hashlib
import http.client
import datetime
import json
import os
import re
import ssl
from pathlib import Path
import secrets
import sqlite3
import stat
import struct
import sys
import tempfile
import time
from urllib.parse import quote, unquote, urlsplit

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

MAGIC = b'KOVABK01'
MAX_DB = 64 * 1024 * 1024
MAX_CONFIG = 1024 * 1024
MAX_ARCHIVE = MAX_DB + MAX_CONFIG + 128


def regular_file(path: Path, max_bytes: int) -> bytes:
    # Read a bounded regular file without following a final symlink.
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        if not stat.S_ISREG(s.st_mode) or s.st_size > max_bytes:
            raise ValueError('invalid or oversized input file')
        with os.fdopen(fd, 'rb', closefd=False) as source:
            data = source.read(max_bytes + 1)
        if len(data) > max_bytes:
            raise ValueError('input exceeds size limit')
        return data
    finally:
        os.close(fd)


def read_key(path: Path) -> bytes:
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        if not stat.S_ISREG(s.st_mode) or s.st_uid != os.geteuid() or s.st_mode & 0o077:
            raise ValueError('key must be a private file owned by the current user (mode 0600)')
        key = os.read(fd, 33)
        if len(key) != 32:
            raise ValueError('backup key must contain exactly 32 bytes')
        return key
    finally:
        os.close(fd)


def create_key(path: Path) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    try:
        with os.fdopen(fd, 'wb', closefd=False) as target:
            target.write(secrets.token_bytes(32))
            target.flush()
            os.fsync(target.fileno())
    finally:
        os.close(fd)


def read_webdav_credentials(path: Path) -> tuple[str, str]:
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        s = os.fstat(fd)
        if not stat.S_ISREG(s.st_mode) or s.st_uid != os.geteuid() or s.st_mode & 0o077 or s.st_size > 4096:
            raise ValueError('WebDAV credentials must be a private owner-only file')
        with os.fdopen(fd, 'rb', closefd=False) as source:
            raw = source.read(4097)
        if len(raw) > 4096:
            raise ValueError('WebDAV credentials exceed size limit')
    finally:
        os.close(fd)
    value = json.loads(raw)
    if not isinstance(value, dict): raise ValueError('invalid WebDAV credentials')
    username, password = value.get('username'), value.get('password')
    if (not isinstance(username, str) or not username or ':' in username or
        not isinstance(password, str) or not password or
        any(ord(c) < 32 or ord(c) == 127 for c in username + password)):
        raise ValueError('invalid WebDAV credentials')
    return username, password


def webdav_destination(url: str) -> tuple[str, int, str]:
    try:
        parsed = urlsplit(url)
        port = parsed.port or 443
        hostname = parsed.hostname
    except ValueError:
        raise ValueError('invalid WebDAV HTTPS URL') from None
    if (parsed.scheme != 'https' or not hostname or parsed.username is not None or
        parsed.password is not None or parsed.query or parsed.fragment or
        any(ord(c) <= 32 or ord(c) == 127 for c in url)):
        raise ValueError('WebDAV destination must be an HTTPS URL without credentials, query or fragment')
    # Decode and re-encode each path segment, avoiding traversal and escaped slashes.
    segments = []
    for raw in parsed.path.split('/'):
        if not raw: continue
        segment = unquote(raw, errors='strict')
        if segment in ('.', '..') or '/' in segment or '\\' in segment or any(ord(c) < 32 or ord(c) == 127 for c in segment):
            raise ValueError('invalid WebDAV path')
        segments.append(quote(segment, safe='-._~'))
    return hostname, port, '/' + '/'.join(segments)


def upload_webdav(archive: Path, url: str, credentials_path: Path, key_path: Path,
                  connection_factory=http.client.HTTPSConnection) -> None:
    hostname, port, base = webdav_destination(url)
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,199}\.kvbak', archive.name):
        raise ValueError('invalid backup filename for WebDAV')
    user, password = read_webdav_credentials(credentials_path)
    payload = regular_file(archive, MAX_ARCHIVE)
    unpack_data(payload, key_path)  # Authenticate the exact bytes sent.
    token = base64.b64encode((user + ':' + password).encode('utf-8')).decode('ascii')
    remote = base.rstrip('/') + '/' + quote(archive.name, safe='-._~')
    # HTTPS only, system trust store, no redirect following, no credentials in
    # URL or command-line arguments. The operator controls this destination;
    # never expose this tool as a public URL-fetching service.
    conn = connection_factory(hostname, port, timeout=60, context=ssl.create_default_context())
    try:
        conn.request('HEAD', remote, headers={'Authorization': 'Basic ' + token})
        response = conn.getresponse()
        response.read(4096)
        if response.status != 404:
            raise RuntimeError('WebDAV target exists or cannot be checked')
    finally:
        conn.close()
    conn = connection_factory(hostname, port, timeout=60, context=ssl.create_default_context())
    try:
        conn.request('PUT', remote, body=payload, headers={
            'Authorization': 'Basic ' + token, 'Content-Type': 'application/octet-stream',
            'Content-Length': str(len(payload)), 'If-None-Match': '*',
        })
        response = conn.getresponse()
        response.read(4096)
        if response.status not in (200, 201, 204):
            raise RuntimeError('WebDAV upload failed')
    finally:
        conn.close()
    conn = connection_factory(hostname, port, timeout=60, context=ssl.create_default_context())
    try:
        conn.request('GET', remote, headers={'Authorization': 'Basic ' + token, 'Accept-Encoding': 'identity'})
        response = conn.getresponse()
        if response.status != 200:
            raise RuntimeError('WebDAV verification download failed')
        digest = hashlib.sha256()
        remaining = len(payload)
        while remaining:
            chunk = response.read(min(65536, remaining))
            if not chunk: raise RuntimeError('WebDAV verification truncated')
            digest.update(chunk); remaining -= len(chunk)
        if response.read(1) or digest.digest() != hashlib.sha256(payload).digest():
            raise RuntimeError('WebDAV verification mismatch')
    finally:
        conn.close()


def db_integrity(data: bytes) -> None:
    # Serialized SQLite snapshots may retain WAL-mode header flags, which
    # SQLite cannot deserialize into :memory:. Validate in a private temporary
    # directory before exposing the restored file. This writes plaintext scratch
    # data on the system temp filesystem; operators must protect it accordingly.
    with tempfile.TemporaryDirectory(prefix='kova-verify-') as temp:
        path = Path(temp) / 'snapshot.db'
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
        con = sqlite3.connect('file:' + quote(str(path), safe='/') + '?mode=ro', uri=True)
        try:
            if con.execute('PRAGMA integrity_check').fetchone() != ('ok',):
                raise ValueError('SQLite integrity check failed')
        finally:
            con.close()


def snapshot(data_dir: Path) -> bytes:
    data_dir = data_dir.resolve(strict=True)
    cfg_path = data_dir / 'config.json'
    db_path = data_dir / 'data.db'
    config = regular_file(cfg_path, MAX_CONFIG)
    info = json.loads(config)
    if not isinstance(info, dict) or not isinstance(info.get('database'), dict):
        raise ValueError('invalid Kova config')
    configured_db = info['database'].get('db_file')
    if not isinstance(configured_db, str) or Path(configured_db).resolve() != db_path:
        raise ValueError('custom or mismatched database path is unsupported; run from the server working directory')
    # Reject a symlink as the final database path before opening in read-only mode.
    st = db_path.lstat()
    if not stat.S_ISREG(st.st_mode):
        raise ValueError('database must be a regular file')
    source = sqlite3.connect('file:' + quote(str(db_path), safe='/') + '?mode=ro', uri=True, timeout=10)
    dest = sqlite3.connect(':memory:')
    try:
        page_count = source.execute('PRAGMA page_count').fetchone()[0]
        page_size = source.execute('PRAGMA page_size').fetchone()[0]
        names = {row[0] for row in source.execute("SELECT name FROM sqlite_master WHERE type='table' AND name IN ('x_users','x_roles','x_storages')")}
        if names != {'x_users', 'x_roles', 'x_storages'}:
            raise ValueError('database is not a supported native Kova database')
        if page_count * page_size > MAX_DB:
            raise ValueError('database exceeds 64 MiB limit')
        deadline = time.monotonic() + 30
        def progress(_status, _remaining, _total):
            if time.monotonic() > deadline:
                raise TimeoutError('SQLite backup timed out')
        source.backup(dest, pages=256, progress=progress, sleep=0.05)
        if dest.execute('PRAGMA integrity_check').fetchone() != ('ok',):
            raise ValueError('SQLite integrity check failed')
        db = dest.serialize()
        if len(db) > MAX_DB:
            raise ValueError('database exceeds 64 MiB limit')
    finally:
        dest.close()
        source.close()
    if regular_file(cfg_path, MAX_CONFIG) != config:
        raise ValueError('config changed during backup; retry')
    return struct.pack('>IQ', len(config), len(db)) + config + db


def backup(data_dir: Path, output: Path, key_path: Path) -> None:
    root = data_dir.resolve(strict=True)
    parent = output.parent.resolve(strict=True)
    if parent == root or root in parent.parents:
        raise ValueError('backup must be stored outside the data directory')
    parent_stat = parent.stat()
    if parent_stat.st_uid != os.geteuid() or parent_stat.st_mode & 0o077:
        raise ValueError('backup directory must be private and owned by the current user')
    key_parent = key_path.parent.resolve(strict=True)
    if key_parent == parent or key_parent == root or root in key_parent.parents:
        raise ValueError('store the backup key separately from data and backups')
    key = read_key(key_path)
    payload = snapshot(root)
    nonce = secrets.token_bytes(12)
    encrypted = MAGIC + nonce + AESGCM(key).encrypt(nonce, payload, MAGIC)
    # A private, encrypted temporary file plus link() prevents overwriting an
    # existing backup, even if another process creates it concurrently.
    fd, temp = tempfile.mkstemp(prefix='.kova-backup-', dir=parent)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(encrypted)
            f.flush()
            os.fsync(f.fileno())
        os.link(temp, parent / output.name, follow_symlinks=False)
    finally:
        os.unlink(temp)


def unpack(archive: Path, key_path: Path):
    return unpack_data(regular_file(archive, MAX_ARCHIVE), key_path)


def unpack_data(data: bytes, key_path: Path):
    if len(data) < len(MAGIC) + 12 + 16 or data[:8] != MAGIC:
        raise ValueError('invalid backup format')
    plain = AESGCM(read_key(key_path)).decrypt(data[8:20], data[20:], MAGIC)
    if len(plain) < 12:
        raise ValueError('invalid backup payload')
    config_len, db_len = struct.unpack('>IQ', plain[:12])
    if config_len > MAX_CONFIG or db_len > MAX_DB or len(plain) != 12 + config_len + db_len:
        raise ValueError('invalid backup lengths')
    config = json.loads(plain[12:12 + config_len])
    if not isinstance(config, dict) or not isinstance(config.get('database'), dict):
        raise ValueError('invalid backup configuration')
    db = plain[12 + config_len:]
    db_integrity(db)
    return config, db


def restore(archive: Path, key: Path, target: Path) -> None:
    config, db = unpack(archive, key)  # Authenticate and validate before writing.
    parent = target.parent.resolve(strict=True)
    parent_stat = parent.stat()
    if parent_stat.st_uid != os.geteuid() or parent_stat.st_mode & 0o077:
        raise ValueError('restore parent must be private and owned by the current user')
    target = parent / target.name
    target.mkdir(mode=0o700)  # Refuse an existing directory, including symlinks.
    try:
        config['database']['db_file'] = str(target / 'data.db')
        config['temp_dir'] = str(target / 'temp')
        if isinstance(config.get('log'), dict):
            config['log']['name'] = str(target / 'log' / 'alist.log')
        for name, contents in [('data.db', db), ('config.json', (json.dumps(config, indent=2) + '\n').encode())]:
            fd = os.open(target / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, 'wb') as f:
                f.write(contents)
                f.flush()
                os.fsync(f.fileno())
    except BaseException:
        # Remove only files we created in the fresh directory; never delete an
        # existing destination or recursively remove operator data.
        for name in ('data.db', 'config.json'):
            try: (target / name).unlink()
            except FileNotFoundError: pass
        target.rmdir()
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='cmd', required=True)
    key = sub.add_parser('create-key'); key.add_argument('key', type=Path)
    create = sub.add_parser('create'); create.add_argument('data', type=Path); create.add_argument('output', type=Path); create.add_argument('key', type=Path)
    verify = sub.add_parser('verify'); verify.add_argument('archive', type=Path); verify.add_argument('key', type=Path)
    recover = sub.add_parser('restore'); recover.add_argument('archive', type=Path); recover.add_argument('key', type=Path); recover.add_argument('new_dir', type=Path)
    upload = sub.add_parser('upload-webdav'); upload.add_argument('archive', type=Path); upload.add_argument('key', type=Path); upload.add_argument('url'); upload.add_argument('credentials', type=Path)
    schedule = sub.add_parser('schedule'); schedule.add_argument('data', type=Path); schedule.add_argument('output_dir', type=Path); schedule.add_argument('key', type=Path); schedule.add_argument('--interval', type=int, default=86400); schedule.add_argument('--webdav-url'); schedule.add_argument('--webdav-credentials', type=Path)
    args = parser.parse_args()
    try:
        if args.cmd == 'create-key': create_key(args.key)
        elif args.cmd == 'create': backup(args.data, args.output, args.key)
        elif args.cmd == 'verify': unpack(args.archive, args.key)
        elif args.cmd == 'restore': restore(args.archive, args.key, args.new_dir)
        elif args.cmd == 'upload-webdav':
            upload_webdav(args.archive, args.url, args.credentials, args.key)
        else:
            if bool(args.webdav_url) != bool(args.webdav_credentials): raise ValueError('provide both WebDAV URL and credentials file')
            if args.interval < 3600: raise ValueError('minimum interval is 3600 seconds')
            while True:
                try:
                    now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
                    archive = args.output_dir / f'kova-{now}-{secrets.token_hex(4)}.kvbak'
                    backup(args.data, archive, args.key)
                    if args.webdav_url:
                        upload_webdav(archive, args.webdav_url, args.webdav_credentials, args.key)
                    else:
                        unpack(archive, args.key)
                    print('Encrypted metadata backup created and integrity-checked', flush=True)
                except Exception as exc:
                    print(f'Backup failed: {type(exc).__name__}', file=sys.stderr, flush=True)
                time.sleep(args.interval)
        print(f'{args.cmd} completed', flush=True)
        return 0
    except Exception as exc:
        # Do not expose raw provider data, config contents, or secrets in logs.
        message = str(exc) if isinstance(exc, ValueError) else type(exc).__name__
        print(f'{args.cmd} failed: {message}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
