#!/usr/bin/env python3
"""Verify the stored local engineering evidence;never sign Gate acceptance."""
from pathlib import Path
import hashlib,json,gzip
D=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
sha=lambda b:hashlib.sha256(b).hexdigest()
manifest=read(D/'SHA256.json');actual={str(p.relative_to(D))for p in D.rglob('*')if p.is_file()and p.name!='SHA256.json'}
assert set(manifest)==actual,'manifest inventory drift'
for name,h in manifest.items():assert sha((D/name).read_bytes())==h,name
result=read(D/'result.json');assert result['status']=='READY_FOR_CONTROL_REVIEW'and result['officialGatePassClaimed']is False
native=read(D/'raw/native-comparison.json');prior=read(D/'raw/prior-native-comparison.json');assert native['passed']==1012 and native['total']==1078 and native['failed_names']==prior['failed_names']and not native['new_failed_names']
perf=read(D/'native-performance.json');assert all(perf['after']['checks'].values())and perf['after']['rows'][-1]['upMs']<250 and max(x['upMs']for x in perf['after']['rows'])<500
workers=[read(p)for p in (D/'raw').glob('*.worker.json')];assert len(workers)==12 and all(w['reason']=='completed'and w['cleanup_complete']and not w['remaining_owned_pids']for w in workers)
for p in (D/'raw').glob('*dpr*/observation.json.gz'):
 b=read(p);expected='REPRODUCED_REJECTION_BUG'if p.parent.name=='baseline-off-dpr2'else'MEASURED';assert b['status']==expected and b['sourceStable']and b['artifactStable'];assert not b['sourceBefore']['dirty']and not b['sourceAfter']['dirty'];assert b['sourceBefore']['commit']==b['sourceAfter']['commit']==result['verificationRevision']
 if p.parent.name.startswith('candidate')and'post-edit'not in p.parent.name:
  assert all(b['recovery']['checks'].values())and all(b['noopChecks'].values())and all(b['historyChecks'].values());assert not b['recovery']['retryErrors']and b['close']['closed']
 for r in b.get('rows',[]):
  if r['text'].startswith('AXIOM_NAVIGATION_TIMING '):assert json.loads(r['text'][24:]).get('budget_used_bytes',0)<=96*1024**2
for v in read(D/'paired-content.json').values():assert all(v.values())
verified={}
for entry in read(D/'raw/snapshot-index.json').values():
 name=entry['stored']
 if name not in verified:verified[name]=gzip.decompress((D/name).read_bytes())
 body=verified[name];assert sha(body)==entry['sha256']and len(body)==entry['bytes']
for name in ['capacity','rejected-pixels']:
 b=read(D/'raw'/(name+'.json'));assert b['status']in['PASS','MEASURED']and all(b['checks'].values())
for meta in read(D/'packages.json').values():assert len(meta['files'])==36 and not any(Path(p).suffix in['.h','.hpp','.a']for p in meta['files'])
assert all(v['unchangedFromAnchor']for v in read(D/'execution-inputs.json')['unchanged'].values())
demo=read(D/'demo/smoke.json');assert demo['status']=='MEASURED'and all(demo['checks'].values())
print(json.dumps({'status':'VERIFIED_LOCAL_EVIDENCE','files':len(manifest),'workers':len(workers),'uniqueSnapshots':len(verified),'officialGatePassClaimed':False}))
