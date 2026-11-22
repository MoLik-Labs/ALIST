#!/usr/bin/env python3
"""Encrypted metadata backup, WAL snapshot, restore and failure regression tests."""
import importlib.util
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('backup', Path(__file__).resolve().parents[1] / 'tools/backup/kova_backup.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)

class BackupTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.data = self.root / 'data'; self.data.mkdir(mode=0o700)
        self.out = self.root / 'backups'; self.out.mkdir(mode=0o700)
        self.keys = self.root / 'keys'; self.keys.mkdir(mode=0o700)
        self.key = self.keys / 'key'; b.create_key(self.key)
        self.archive = self.out / 'backup.kvbak'
        self.config = {'jwt_secret': 'super-secret-not-plaintext', 'database': {'type': 'sqlite3', 'db_file': str(self.data / 'data.db')}, 'temp_dir': str(self.data / 'temp'), 'log': {'name': str(self.data / 'log/alist.log')}}
        (self.data / 'config.json').write_text(json.dumps(self.config))
        self.db = sqlite3.connect(self.data / 'data.db')
        self.db.execute('PRAGMA journal_mode=WAL')
        for name in ('x_users', 'x_roles', 'x_storages'):
            self.db.execute(f'CREATE TABLE {name} (id INTEGER)')
        self.db.execute('CREATE TABLE example (value TEXT)')
        self.db.execute('INSERT INTO example VALUES (?)', ('secret-in-wal',))
        self.db.commit()

    def tearDown(self):
        self.db.close(); self.tmp.cleanup()

    def test_snapshot_restore_and_wal(self):
        b.backup(self.data, self.archive, self.key)
        cipher = self.archive.read_bytes()
        self.assertNotIn(b'secret-in-wal', cipher)
        self.assertNotIn(b'super-secret-not-plaintext', cipher)
        config, db = b.unpack(self.archive, self.key)
        self.assertEqual(config['jwt_secret'], self.config['jwt_secret'])
        dest = self.root / 'restored'
        b.restore(self.archive, self.key, dest)
        with sqlite3.connect(dest / 'data.db') as con:
            self.assertEqual(con.execute('SELECT value FROM example').fetchone(), ('secret-in-wal',))
        recovered = json.loads((dest / 'config.json').read_text())
        self.assertEqual(recovered['database']['db_file'], str(dest / 'data.db'))
        self.assertEqual(recovered['jwt_secret'], self.config['jwt_secret'])
        self.assertEqual((dest / 'config.json').stat().st_mode & 0o777, 0o600)
        self.assertEqual(dest.stat().st_mode & 0o777, 0o700)
        with self.assertRaises(FileExistsError): b.restore(self.archive, self.key, dest)
        with self.assertRaises(FileExistsError): b.backup(self.data, self.archive, self.key)
        self.assertEqual(cipher, self.archive.read_bytes())

    def test_tamper_wrong_key_no_restore(self):
        b.backup(self.data, self.archive, self.key)
        other = self.keys / 'other'; b.create_key(other)
        target = self.root / 'restored'
        with self.assertRaises(Exception): b.restore(self.archive, other, target)
        self.assertFalse(target.exists())
        raw = bytearray(self.archive.read_bytes()); raw[-1] ^= 1; self.archive.write_bytes(raw)
        with self.assertRaises(Exception): b.restore(self.archive, self.key, target)
        self.assertFalse(target.exists())

    def test_schedule_creates_verified_backup(self):
        script = Path(__file__).resolve().parents[1] / 'tools/backup/kova_backup.py'
        p = subprocess.Popen([sys.executable, str(script), 'schedule', str(self.data), str(self.out), str(self.key), '--interval', '3600'], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                files = list(self.out.glob('*.kvbak'))
                if files:
                    try:
                        b.unpack(files[0], self.key)
                        break
                    except Exception:
                        pass
                if p.poll() is not None:
                    self.fail('scheduler exited before creating a valid backup')
                time.sleep(.05)
            else:
                self.fail('scheduler did not create a valid backup')
        finally:
            p.terminate(); p.communicate(timeout=5)

    def test_guardrails(self):
        with self.assertRaises(ValueError): b.backup(self.data, self.data / 'bad', self.key)
        with self.assertRaises(ValueError): b.backup(self.data, self.out / 'bad', self.out / 'key')
        os.chmod(self.key, 0o644)
        with self.assertRaises(ValueError): b.backup(self.data, self.archive, self.key)
        os.chmod(self.key, 0o600)
        self.config['database']['db_file'] = str(self.root / 'other.db')
        (self.data / 'config.json').write_text(json.dumps(self.config))
        with self.assertRaises(ValueError): b.backup(self.data, self.archive, self.key)
        self.assertFalse(self.archive.exists())

if __name__ == '__main__': unittest.main()
