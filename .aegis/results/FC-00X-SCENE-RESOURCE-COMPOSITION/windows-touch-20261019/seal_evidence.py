"""Index exact canonical Git bytes, excluding the index's own future hash."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

here = Path(__file__).resolve().parent
root = here.parents[3]
prefix = here.relative_to(root).as_posix()
verify_ref = sys.argv[1] if len(sys.argv) > 1 else None
ref = verify_ref or ''

def blob(path):
    return subprocess.check_output(['git', 'show', ref + ':' + path], cwd=root)

if verify_ref:
    index = json.loads(blob(prefix + '/artifact-index.json'))
    for entry in index['files']:
        assert hashlib.sha256(blob(entry['path'])).hexdigest() == entry['sha256'], entry['path']
    print(f"Verified {len(index['files'])} exact Git evidence blobs at {verify_ref}.")
else:
    paths = subprocess.check_output(['git', 'ls-files', '-z', '--', prefix], cwd=root).decode().split('\0')
    entries = [{'path': p, 'sha256': hashlib.sha256(blob(p)).hexdigest()}
               for p in paths if p and not p.endswith('/artifact-index.json')]
    (here / 'artifact-index.json').write_text(json.dumps({
        'hash_basis': 'exact canonical Git index bytes; verify against containing commit',
        'self_hash_excluded': True, 'files': entries}, indent=2) + '\n', encoding='utf-8')
    print(f"Indexed {len(entries)} exact Git evidence blobs.")
