#!/usr/bin/env python3
"""Encrypted metadata backup, WAL snapshot, restore and failure regression tests."""
import importlib.util
import datetime
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ssl
import threading
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
import tempfile
import unittest

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

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

    def test_verified_webdav_upload_and_failures(self):
        b.backup(self.data, self.archive, self.key)
        creds = self.keys / 'dav.json'
        creds.write_text(json.dumps({'username': 'test-user', 'password': 'test-password'}))
        os.chmod(creds, 0o600)
        key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'localhost')])
        cert = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(key.public_key())
                .serial_number(x509.random_serial_number()).not_valid_before(datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(minutes=1))
                .not_valid_after(datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=1))
                .add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost')]), critical=False)
                .sign(key, hashes.SHA256()))
        cert_file, key_file = self.root / 'cert.pem', self.root / 'tls.key'
        cert_file.write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        key_file.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        objects = {}
        verified_paths = set()
        settings = {'corrupt': False}
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def do_HEAD(self):
                if self.path.startswith('/redirect/'):
                    self.send_response(302); self.send_header('Location', 'https://elsewhere.example/'); self.end_headers(); return
                self.send_response(200 if self.path in objects else 404); self.end_headers()
            def do_PUT(self):
                assert self.headers.get('If-None-Match') == '*'
                assert self.headers.get('Authorization') == 'Basic dGVzdC11c2VyOnRlc3QtcGFzc3dvcmQ='
                if self.path in objects:
                    self.send_response(412); self.end_headers(); return
                objects[self.path] = self.rfile.read(int(self.headers['Content-Length']))
                self.send_response(201); self.end_headers()
            def do_GET(self):
                data = objects.get(self.path)
                if data is not None and settings['corrupt']:
                    data = bytes([data[0] ^ 1]) + data[1:]
                if data is None:
                    self.send_response(404); self.end_headers(); return
                self.send_response(200); self.send_header('Content-Length', str(len(data))); self.end_headers(); self.wfile.write(data)
                if not settings['corrupt']: verified_paths.add(self.path)
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(cert_file, key_file)
        server.socket = ctx.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
        def factory(hostname, port, **kwargs):
            return http.client.HTTPSConnection(hostname, port, timeout=kwargs['timeout'], context=ssl.create_default_context(cafile=str(cert_file)))
        try:
            url = f'https://localhost:{server.server_port}/secure%20folder'
            b.upload_webdav(self.archive, url, creds, self.key, connection_factory=factory)
            self.assertEqual(objects['/secure%20folder/backup.kvbak'], self.archive.read_bytes())
            with self.assertRaises(RuntimeError): b.upload_webdav(self.archive, url, creds, self.key, connection_factory=factory)
            with self.assertRaises(RuntimeError): b.upload_webdav(self.archive, f'https://localhost:{server.server_port}/redirect', creds, self.key, connection_factory=factory)
            second = self.out / 'second.kvbak'; b.backup(self.data, second, self.key)
            settings['corrupt'] = True
            with self.assertRaises(RuntimeError): b.upload_webdav(second, url, creds, self.key, connection_factory=factory)
            settings['corrupt'] = False
            third = self.out / 'third.kvbak'; b.backup(self.data, third, self.key)
            script = Path(__file__).resolve().parents[1] / 'tools/backup/kova_backup.py'
            env = {**os.environ, 'SSL_CERT_FILE': str(cert_file)}
            result = subprocess.run([sys.executable, str(script), 'upload-webdav', str(third), str(self.key), url, str(creds)], env=env, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(objects['/secure%20folder/third.kvbak'], third.read_bytes())
            process = subprocess.Popen([sys.executable, str(script), 'schedule', str(self.data), str(self.out), str(self.key), '--interval', '3600', '--webdav-url', url, '--webdav-credentials', str(creds)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            try:
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline and not any('/kova-' in path for path in verified_paths):
                    if process.poll() is not None: self.fail('WebDAV scheduler exited unexpectedly')
                    time.sleep(.05)
                self.assertTrue(any('/kova-' in path for path in verified_paths), 'scheduled WebDAV read-back missing')
            finally:
                process.terminate(); process.communicate(timeout=5)
            os.chmod(creds, 0o644)
            with self.assertRaises(ValueError): b.upload_webdav(self.archive, url, creds, self.key, connection_factory=factory)
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=5)
        for url in ('http://example.com/a', 'https://user:pass@example.com', 'https://example.com/a?key=secret',
                    'https://example.com/a/../b', 'https://example.com/a/%2f/b', 'https://example.com/a/%5c/b'):
            with self.assertRaises(ValueError): b.webdav_destination(url)

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
