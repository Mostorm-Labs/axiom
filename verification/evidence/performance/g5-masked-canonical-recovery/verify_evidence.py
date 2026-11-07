#!/usr/bin/env python3
"""Offline local evidence verification;no Gate/physical/competitor acceptance."""
from pathlib import Path
import json,gzip,hashlib
D=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
sha=lambda b:hashlib.sha256(b).hexdigest()
manifest=read(D/'SHA256.json');actual={str(p.relative_to(D))for p in D.rglob('*')if p.is_file()and p.name!='SHA256.json'};assert set(manifest)==actual
for name,h in manifest.items():assert sha((D/name).read_bytes())==h,name
result=read(D/'result.json');assert result['status']=='READY_FOR_CONTROL_REVIEW'and not result['officialGatePassClaimed']
summary=read(D/'summary.json');cases={p.parent.name:read(p)for p in (D/'raw').glob('candidate*/observation.json.gz')};cases['baseline-on-dpr2']=read(D/'raw/baseline-on-dpr2/observation.json.gz');assert len(cases)==7
for name,b in cases.items():
 assert b['sourceStable']and b['artifactStable']and not b['sourceBefore']['dirty']and not b['sourceAfter']['dirty'];assert b['sourceBefore']['commit']==b['sourceAfter']['commit'];assert all(b['historyChecks'].values())and b['close']['closed']and not b['close']['errors']and not b['errors']
 for i,p in enumerate(b['phases']):
  assert all(p['raw']['checks'].values());r=p['raw'];frames=[json.loads(x['text'][19:])for x in r['nativeRecords']if x['at']>=r['upAt']and x['text'].startswith('AXIOM_FRAME_TIMING ')];total=sum(x['stages'].get('cpu_coverage',{}).get('inclusive_ns',0)for x in frames)/1e6;stored=summary[name]['phases'][i]['timing']['cpuCoverageMs']
  if '-on-'in name:
   assert frames,'required diagnostic unavailable';assert abs(total-stored)<.001
   if name.startswith('candidate'):assert total<1500 and p['timing']['maxCanonicalAttemptMs']<500 and p['timing']['recoveryAfterInputMs']<2000
  else:assert not frames and stored is None
  if name=='baseline-on-dpr2':assert total>1500
for name,pair in read(D/'paired-content.json').items():assert pair['checkpoints']==5 and pair['exportPixels']and pair['snapshotBytes'];assert '-dpr1' in name or '-dpr4' in name or all(pair['compositorHashesEqual'])
index=read(D/'raw/snapshot-index.json');seen={}
for v in index.values():
 if v['stored']not in seen:seen[v['stored']]=gzip.decompress((D/v['stored']).read_bytes())
 body=seen[v['stored']];assert sha(body)==v['sha256']and len(body)==v['bytes']
workers=[read(p)for p in (D/'raw').glob('*worker.json')];assert len(workers)==12 and all(w['reason']=='completed'and w['cleanup_complete']and not w['remaining_owned_pids']for w in workers)
native=read(D/'raw/native-comparison.json');prior=read(D/'raw/prior-native-comparison.json');assert native['passed']==1013 and native['total']==1079 and native['failed_names']==prior['failed_names']and not native['new_failed_names']
assert read(D/'raw/gpu.json')['status']=='PASS'
for name in ['dense','mixed']:assert read(D/'raw'/name/'observation.json.gz')['status']=='MEASURED'
for name in ['capacity','rejected-pixels']:
 b=read(D/'raw'/(name+'.json'));assert b['status']in['PASS','MEASURED']and all(b['checks'].values())
for meta in read(D/'packages.json').values():assert len(meta['files'])==36 and not any(Path(n).suffix in['.a','.h','.hpp']for n in meta['files'])
assert all(v['unchangedFromAnchor']for v in read(D/'execution-inputs.json')['unchanged'].values())
demo=read(D/'demo/smoke.json');assert demo['status']=='MEASURED'and all(demo['checks'].values())
print(json.dumps({'status':'VERIFIED_LOCAL_EVIDENCE','files':len(manifest),'workers':len(workers),'snapshots':len(seen),'officialGatePassClaimed':False}))
