#!/usr/bin/env python3
"""Optional local Kova metadata backup. Never backs up mounted files or NodeWarden."""
import argparse
import datetime
import json
import os
from pathlib import Path
import secrets
import sqlite3
import stat
import struct
import sys
import tempfile
import time
from urllib.parse import quote

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
    data = regular_file(archive, MAX_ARCHIVE)
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
    schedule = sub.add_parser('schedule'); schedule.add_argument('data', type=Path); schedule.add_argument('output_dir', type=Path); schedule.add_argument('key', type=Path); schedule.add_argument('--interval', type=int, default=86400)
    args = parser.parse_args()
    try:
        if args.cmd == 'create-key': create_key(args.key)
        elif args.cmd == 'create': backup(args.data, args.output, args.key)
        elif args.cmd == 'verify': unpack(args.archive, args.key)
        elif args.cmd == 'restore': restore(args.archive, args.key, args.new_dir)
        else:
            if args.interval < 3600: raise ValueError('minimum interval is 3600 seconds')
            while True:
                try:
                    now = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
                    archive = args.output_dir / f'kova-{now}-{secrets.token_hex(4)}.kvbak'
                    backup(args.data, archive, args.key)
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
