"""Verify this local engineering evidence envelope;never sign a Gate PASS."""
import gzip,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parent
manifest=json.loads((root/'SHA256.json').read_text())
assert {str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and p.name!='SHA256.json'}==set(manifest)
for name,digest in manifest.items():
 p=root/name;assert hashlib.sha256(p.read_bytes()).hexdigest()==digest,name
 if name.endswith('.json'):json.loads(p.read_text())
 elif name.endswith('.json.gz'):json.loads(gzip.decompress(p.read_bytes()))
r=json.loads((root/'result.json').read_text());assert not r['official_gate_pass_claimed']
workers=[]
for p in root.rglob('*.worker.json'):
 if 'exploratory' in p.parts:continue
 j=json.loads(p.read_text());workers.append((str(p.relative_to(root)),j))
 assert j['returncode']==0 and j['cleanup_complete'] and not j['remaining_owned_pids'],p
 assert j['peak_process_family_rss_kib']<=j['rss_limit_kib'],p
counts=[]
for mode in ['on','off']:
 p=root/mode/'matrix.json';j=json.loads(p.read_text()) if p.exists() else json.loads(gzip.decompress(p.with_suffix('.json.gz').read_bytes()));counts.append(len(j['results']))
assert len(workers)==len(r['gpu_modes'])+len(r['corroboration'])*2+sum(counts)+1
print(json.dumps({'verified_files':len(manifest),'qualifying_workers':len(workers),'status':r['status'],'all_hashes_json_worker_limits_and_cleanup_verified':True}))
