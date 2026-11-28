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
assert 'render.com/deploy?repo=https%3A%2F%2Fgithub.com%2FMoLik-Labs%2FKova-INC' in readme
assert 'deploy.workers.cloudflare.com/?url=' in readme
assert 'public repository' in readme and 'edge proxy' in readme
render = (root / 'render.yaml').read_text()
assert 'mountPath: /opt/alist/data' in render and 'healthCheckPath: /ping' in render
assert "autoDeployTrigger: 'off'" in render
assert 'key: ALIST_ADMIN_PASSWORD\n        generateValue: true' in render
free = (root / 'deploy/render/render.free-demo.yaml').read_text()
assert 'plan: free' in free and 'disk:' not in free
assert 'healthCheckPath: /ping' in free and 'KOVA_DISPOSABLE_DEMO' in free
assert 'generateValue: true' in free and 'runtime: docker' in free
assert 'key: ALIST_WORKER_THREADS' in free and 'key: ALIST_TASK_WORKERS' in free
assert 'key: ALIST_MAX_CONNECTIONS' in free
assert 'key: ALIST_WORKER_THREADS' not in render
print('Platform configuration invariants passed (no cloud deployment performed)')
