#!/usr/bin/env python3
"""Check deployment invariants without provisioning any cloud resources."""
import json
from pathlib import Path
import tomllib

root = Path(__file__).resolve().parent.parent
with (root / 'fly.toml').open('rb') as f:
    fly = tomllib.load(f)
assert fly['build']['dockerfile'] == 'Dockerfile'
assert fly['mounts']['destination'] == '/opt/alist/data'
assert fly['http_service']['internal_port'] == int(fly['env']['PORT'])
assert fly['http_service']['force_https'] is True
assert any(check['path'] == '/ping' for check in fly['http_service']['checks'])
worker = root / 'deploy/cloudflare-worker'
with (worker / 'wrangler.toml').open('rb') as f:
    wrangler = tomllib.load(f)
assert (worker / wrangler['main']).is_file()
assert 'KOVA_ORIGIN=' in (worker / '.dev.vars.example').read_text()
assert 'KOVA_ORIGIN' in json.loads((worker / 'package.json').read_text())['cloudflare']['bindings']
readme = (root / 'README.md').read_text()
assert 'render.com/deploy?repo=' in readme
assert 'deploy.workers.cloudflare.com/?url=' in readme
assert 'private' in readme and 'edge proxy' in readme
render = (root / 'render.yaml').read_text()
assert 'mountPath: /opt/alist/data' in render and 'healthCheckPath: /ping' in render
assert 'autoDeployTrigger: off' in render
print('Platform configuration invariants passed (no cloud deployment performed)')
