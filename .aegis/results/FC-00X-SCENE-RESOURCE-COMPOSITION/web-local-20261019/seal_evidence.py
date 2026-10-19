"""Hash exact normalized Git blobs, or verify a committed evidence snapshot."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

directory = Path(__file__).resolve().parent
root = directory.parents[3]
prefix = directory.relative_to(root).as_posix()
index_path = prefix + '/artifact-index.json'
revision = sys.argv[1] if len(sys.argv) > 1 else None


def blob(path):
    return subprocess.check_output(['git', 'show', f'{revision or ""}:{path}'], cwd=root)


if revision:
    records = json.loads(blob(index_path))['files']
    for record in records:
        data = blob(record['path'])
        assert hashlib.sha256(data).hexdigest() == record['sha256'], record['path']
        assert len(data) == record['bytes'], record['path']
    print(f'Verified {len(records)} exact Git blobs at {revision}.')
else:
    paths = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--', prefix], cwd=root)
    records = []
    for path in sorted(paths.decode().rstrip('\0').split('\0')):
        if path == index_path:
            continue
        data = blob(path)
        records.append({'path': path, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    (directory / 'artifact-index.json').write_text(json.dumps({'format': 'git-blob-sha256-v1', 'files': records}, indent=2) + '\n')
    print(f'Sealed {len(records)} staged Git blobs; stage artifact-index.json next.')
