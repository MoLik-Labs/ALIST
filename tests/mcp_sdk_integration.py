#!/usr/bin/env python3
"""Local-only smoke test using the official Python MCP SDK (install mcp==1.30.0).

Run: python tests/mcp_sdk_integration.py ./alist
Never point this test at a real server: it creates a disposable local instance.
"""
import asyncio
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

import httpx
from mcp import ClientSession
from mcp.client.streamable_http import streamable_http_client


async def exercise(base: str, storage: Path) -> None:
    async with httpx.AsyncClient(timeout=10, trust_env=False) as http:
        login = await http.post(base + "/api/auth/login", json={"username": "admin", "password": "IntegrationTestPassword!2026"})
        login.raise_for_status()
        token = login.json()["data"]["token"]
        headers = {"Authorization": "Bearer " + token}
        created = await http.post(base + "/api/admin/storage/create", headers=headers, json={
            "driver": "Local", "mount_path": "/local", "addition": {"root_folder_path": str(storage)}
        })
        assert created.json()["code"] == 200, created.text
        http.headers.update(headers)
        async with streamable_http_client(base + "/api/mcp", http_client=http, terminate_on_close=False) as (read, write, _):
            async with ClientSession(read, write) as session:
                result = await session.initialize()
                assert result.protocolVersion == "2025-03-26"
                tools = await session.list_tools()
                assert {tool.name for tool in tools.tools} == {"fs_get", "fs_list"}
                listing = await session.call_tool("fs_list", {"path": "/local"})
                assert not listing.isError, listing
                entries = json.loads(listing.content[0].text)["content"]
                assert any(item["name"] == "example.txt" for item in entries), entries
                item = await session.call_tool("fs_get", {"path": "/local/example.txt"})
                assert not item.isError, item
                assert json.loads(item.content[0].text)["name"] == "example.txt"
    print("Official Python MCP SDK: initialize, list tools, fs_list and fs_get passed")


def main() -> None:
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else "./alist").resolve())
    with tempfile.TemporaryDirectory(prefix="kova-mcp-sdk-") as tmp:
        storage = Path(tmp) / "storage"
        storage.mkdir()
        (storage / "example.txt").write_text("test fixture", encoding="utf-8")
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]
        env = dict(os.environ, ALIST_ENABLE_MCP="true", ALIST_ADDR="127.0.0.1",
                   ALIST_ADMIN_PASSWORD="IntegrationTestPassword!2026")
        proc = subprocess.Popen([binary, "server", "--data", str(Path(tmp) / "data"), "--port", str(port)],
                                env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        base = f"http://127.0.0.1:{port}"
        try:
            for _ in range(100):
                if proc.poll() is not None:
                    raise RuntimeError("server exited during startup")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise RuntimeError("server did not start")
            asyncio.run(exercise(base, storage))
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == "__main__":
    main()
