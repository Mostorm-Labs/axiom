"""Verify the engineering evidence envelope,without signing a Gate verdict."""
import gzip
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parent
manifest = json.loads((root / 'SHA256.json').read_text())
actual = {str(p.relative_to(root)) for p in root.rglob('*')
          if p.is_file() and p.name != 'SHA256.json'}
assert actual == set(manifest)
for name, digest in manifest.items():
    p = root / name
    assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, name
    if name.endswith('.json'):
        json.loads(p.read_text())
    elif name.endswith('.json.gz'):
        json.loads(gzip.decompress(p.read_bytes()))

result = json.loads((root / 'result.json').read_text())
assert result['all_required_checks_satisfied']
assert not result['official_gate_pass_claimed']
assert result['default_budget_mib'] == [32, 96]
workers = []
for p in root.rglob('*.worker.json'):
    if 'exploratory' in p.parts:
        continue
    worker = json.loads(p.read_text())
    assert worker['returncode'] == 0 and worker['cleanup_complete'], p
    assert not worker['remaining_owned_pids'], p
    assert worker['peak_process_family_rss_kib'] <= worker['rss_limit_kib'], p
    assert worker['timeout_s'] == 180, p
    workers.append(p)
assert len(workers) == result['qualifying_browser_samples'] + len(result['gpu_modes']) + 1
for comparison in json.loads((root / 'comparison.json').read_text()):
    assert all(phase['same_final_pixels'] for phase in comparison['phases'])
native = result['native']
assert not native['new_failures']
assert set(native['failed_names']) == set(native['baseline_failed_names'])
print(json.dumps({'status': result['status'], 'verified_files': len(manifest),
                  'qualifying_workers': len(workers),
                  'hashes_json_pixel_comparisons_and_worker_limits_verified': True}))
