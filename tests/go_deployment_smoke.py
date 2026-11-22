#!/usr/bin/env python3
"""Exercise the built experimental server as a platform-style subprocess."""
import json
import os
import pathlib
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

BINARY = pathlib.Path("build/alist-go").resolve()
TOKEN = "test-token-not-for-production-1234567890"
KEYS = ("KOVA_GO_ALLOW_WRITE", "KOVA_GO_ALLOW_DELETE", "KOVA_GO_ALLOW_MOVE", "KOVA_GO_ENABLE_WEBDAV", "KOVA_GO_ENABLE_SHARES")


def request(url, method="GET", body=None, headers=None):
    req = urllib.request.Request(url, data=body, method=method, headers=headers or {})
    try:
        with urllib.request.urlopen(req, timeout=2) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


def run_case(writable):
    with tempfile.TemporaryDirectory() as root:
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        env = os.environ.copy()
        for key in (*KEYS, "ALIST_HTTP_PORT"):
            env.pop(key, None)
        env.update(PORT=str(port), KOVA_GO_ROOT=root, KOVA_GO_TOKEN=TOKEN)
        if writable:
            env.update(KOVA_GO_ALLOW_WRITE="true", KOVA_GO_ENABLE_WEBDAV="true", KOVA_GO_ENABLE_SHARES="true")
        process = subprocess.Popen([str(BINARY), "server", "--experimental", "--addr", "127.0.0.1"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        base = f"http://127.0.0.1:{port}"
        try:
            deadline = time.monotonic() + 10
            while True:
                if process.poll() is not None:
                    raise AssertionError(f"server exited: {process.stderr.read()}")
                try:
                    if request(base + "/ping")[0] == 200:
                        break
                except (urllib.error.URLError, TimeoutError):
                    pass
                if time.monotonic() > deadline:
                    raise AssertionError("server did not start")
                time.sleep(0.05)
            auth = {"Authorization": "Bearer " + TOKEN}
            status, body = request(base + "/api/local/list?path=/", headers=auth)
            assert status == 200
            capabilities = json.loads(body)["capabilities"]
            assert capabilities["create"] == writable and capabilities["share"] == writable
            status, _ = request(base + "/api/local/file?path=/file", "PUT", b"test", auth)
            assert status == (201 if writable else 405), status
            status, _ = request(base + "/dav/", "PROPFIND", headers={**auth, "Depth": "0"})
            assert status == (207 if writable else 503), status
            if writable:
                status, body = request(base + "/api/local/file?path=/file", headers=auth)
                assert status == 200 and body == b"test"
                status, body = request(base + "/api/local/shares", "POST", b'{"path":"/file","ttl_seconds":60}', auth)
                assert status == 201, status
                share_path = json.loads(body)["url"]
                status, body = request(base + "/api/local/shares", headers=auth)
                assert status == 200 and any(item["url"] == share_path for item in json.loads(body)["shares"])
                assert request(base + "/api/local/shares")[0] == 401
                status, body = request(base + share_path)
                assert status == 200 and body == b"test"
                status, _ = request(base + "/api/local/shares/" + share_path.removeprefix("/s/"), "DELETE", headers=auth)
                assert status == 204
                assert request(base + share_path)[0] == 404
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
            try:
                _, stderr = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                raise AssertionError("server failed to stop on SIGTERM")
            assert process.returncode == 0, f"exit {process.returncode}: {stderr}"


if __name__ == "__main__":
    assert BINARY.is_file(), "run make go-build first"
    run_case(False)
    run_case(True)
    print("Go deployment subprocess smoke test passed")
