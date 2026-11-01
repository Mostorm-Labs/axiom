#!/usr/bin/env python3
"""Validate the exact-source inventory, not product or Gate acceptance."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
data = json.loads(Path(__file__).with_name('bindings.json').read_text(encoding='utf-8'))
assert len(data['p01_p10']) == 10 and len(data['api01_api12']) == 12
for path, expected in data['inputs'].items():
    blob = subprocess.run(['git', '-C', str(root), 'show', data['source_revision'] + ':' + path],
                          capture_output=True, check=True).stdout
    if hashlib.sha256(blob).hexdigest() != expected:
        raise SystemExit('source mismatch: ' + path)
print(f"PASS exact inputs={len(data['inputs'])}, P cases=10, API families=12; inventory only")
