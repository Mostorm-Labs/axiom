"""Verify this bounded engineering result, never sign official G5/P34 PASS."""
from pathlib import Path
import gzip,hashlib,json
R=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes())if p.suffix=='.gz'else p.read_bytes())
inventory=read(R/'SHA256.json')
actual={str(p.relative_to(R))for p in R.rglob('*')if p.is_file()and p.name!='SHA256.json'}
assert set(inventory)==actual,'inventory drift'
for name,digest in inventory.items():assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
result=read(R/'result.json');assert result['status']=='READY_FOR_CONTROL_REVIEW'and result['officialGatePassClaimed']is False
native=read(R/'checks/native-comparison.json');prior=read(R/'checks/prior-native-comparison.json')
assert native['failed_names']==prior['failed_names']and not native['new_failed_names']and not native['binariesChangedDuringRun']
assert native['passed']==prior['passed']+1 and native['total']==prior['total']+1
assert native['passed']==result['native']['passed']and native['total']==result['native']['total']
obs={}
for name in ['red','on','off']:
 v=read(R/'raw'/name/'observation.json.gz');w=read(R/'raw'/name/'worker.json');obs[name]=v
 assert v['sourceStable']and v['artifactStable']and not v['errors']
 assert v['sourceBefore']['commit']==v['sourceAfter']['commit']==result['fixture_revision']
 assert not v['sourceBefore']['dirty']and not v['setup']['build']['dirty']
 assert v['artifactBefore']==v['artifactAfter']and v['close']['closed']and not v['close']['errors']
 assert not v['captureSnapshots']and v['pixelParity']is None
 assert v['fixture']['rawSamples']==7200 and v['fixture']['duplicates']==15 and v['fixture']['erasePoints']==25
 assert all(all(p['raw']['checks'].values())for p in v['phases']+v['erasures'])
 assert all(p['raw']['quiescence']['pending']==0 for p in v['phases'])
 assert w['cleanup_complete']and not w['remaining_owned_pids']and w['peak_process_family_rss_kib']<=w['rss_limit_kib']
 assert w['returncode']==0 and w['reason']=='completed'
 if name=='red':
  assert v['status']=='REPRODUCED_MASKED_NAVIGATION_STALL'and not v['endpoint']
  assert v['setup']['build']['sourceRevision']==result['baseline_code_revision']
 else:
  assert v['status']=='MEASURED'and v['endpoint']
  assert v['setup']['build']['sourceRevision']==result['code_revision']
  assert all(p['feedback']['contentGapMs']['max']<result['target_max_gap_ms']for p in v['phases'][1:])
for a,b in zip(obs['red']['phases'],obs['off']['phases']):
 assert a['raw']['after']['camera']==b['raw']['after']['camera']

pixel=read(R/'checks/pixel-comparison.json')
assert pixel['classification']=='PASS_WITH_FINDINGS'
for name in ['red','off']:
 v=read(R/'pixels'/name/'observation.json.gz');w=read(R/'pixels'/name/'worker.json')
 assert v['sourceStable']and v['artifactStable']and not v['sourceBefore']['dirty']
 assert v['sourceBefore']['commit']==result['fixture_revision']and not v['errors']
 assert v['artifactBefore']==obs[name]['artifactBefore']
 assert w['cleanup_complete']and w['peak_process_family_rss_kib']<=w['rss_limit_kib']
 for phase in v['phases']:
  b=gzip.decompress((R/'pixels'/name/(phase['label']+'.rgba.gz')).read_bytes())
  assert hashlib.sha256(b).hexdigest()==phase['pixelCapture']['sha256']==phase['raw']['pixels']['sha256']
for row in pixel['rows']:
 a=gzip.decompress((R/'pixels/red'/(row['label']+'.rgba.gz')).read_bytes());b=gzip.decompress((R/'pixels/off'/(row['label']+'.rgba.gz')).read_bytes())
 assert len(a)==len(b)==1600*1000*4
 indices=[i for i in range(0,len(a),4)if a[i:i+4]!=b[i:i+4]]
 assert len(indices)==row['changed']<=1
 for i in indices:
  x=i//4%1600;y=i//4//1600
  neighbors=[a[(yy*1600+xx)*4]for yy in range(max(0,y-1),min(1000,y+2))for xx in range(max(0,x-1),min(1600,x+2))]
  assert max(neighbors)-min(neighbors)>16 and a[i+3]==b[i+3]
records=[json.loads(r['text'][24:])for p in obs['on']['phases'][1:]for r in p['raw']['nativeRecords']if r['text'].startswith('AXIOM_NAVIGATION_TIMING ')]
assert records and max(x['regional_plane_proofs_total']for x in records)>0
assert sum(x['regional_tile_proofs']for x in records)>0
assert max(x['regional_plane_body_rejects_total']for x in records)==0
assert max(x['regional_plane_admission_rejects_total']for x in records)==0
assert all(x['budget_used_bytes']<=96*1024**2 for x in records)
assert sum(x['gpu_contours']for x in records)>0 and sum(x['coarse_masks']for x in records)==0
for mode in ['regional-mask','v4-gpu','masked-progress','masked-mixed','affine-erased']:
 v=read(R/'checks'/f'{mode}-gpu.json');assert v['status']=='PASS'and v['result']['code']==0 and not v['pageErrors']
assert 'local erasures discarded unchanged wide-view pixels'in(R/'checks/g5-regional-red.log').read_text()
assert 'admitted navigation pixels lost proof metadata solely at soft pressure'in(R/'checks/g5-regional-admission-red.log').read_text()
assert '# pass 42'in(R/'checks/g5-regional-node.log').read_text()and '# fail 0'in(R/'checks/g5-regional-node.log').read_text()
assert '119 contract files'in(R/'checks/g5-regional-boundary.log').read_text()
assert all(x['unchangedFromAnchor']for x in read(R/'execution-inputs.json').values())
assert all(read(R/'checks/demo-smoke.json')['checks'].values())
packages=read(R/'packages.json');assert all(len(p['files'])==36 for p in packages.values())
for name in ['axiom.mjs','axiom.d.mts','pointer_input.mjs','axiom.data']:
 assert packages['ordinary']['files'][name]==packages['diagnostic']['files'][name]
print(json.dumps({'status':'VERIFIED_SCOPED_ENGINEERING_RESULT','files':len(inventory),'native_passed':native['passed'],'native_total':native['total'],'inherited_native_failures':len(native['failed_names']),'officialGatePassClaimed':False}))
