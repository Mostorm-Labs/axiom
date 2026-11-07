"""Verify integrity and a BLOCKED engineering result; never issue a Gate PASS."""
from pathlib import Path
import gzip,hashlib,json
R=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes())if p.suffix=='.gz'else p.read_bytes())
sha=lambda b:hashlib.sha256(b).hexdigest()
inventory=read(R/'SHA256.json')
actual={str(p.relative_to(R))for p in R.rglob('*')if p.is_file()and p.name!='SHA256.json'}
assert set(inventory)==actual,'inventory drift'
for name,digest in inventory.items():assert sha((R/name).read_bytes())==digest,name
result=read(R/'result.json')
assert result['status']=='BLOCKED_ENDPOINT'and result['officialGatePassClaimed']is False
native=read(R/'checks/g5-masked-native-comparison.json')
prior=read(R/'checks/prior-native-comparison.json')
assert not native['new_failed_names']and not native['binariesChangedDuringRun']
assert native['failed_names']==prior['failed_names']
assert native['total']==prior['total']+1 and native['passed']==prior['passed']+1
assert result['native']['total']==native['total']and result['native']['passed']==native['passed']
observations={}
for name in ['red','on','off']:
 folder=R/'raw'/name;v=read(folder/'observation.json.gz');w=read(folder/'worker.json');observations[name]=v
 assert v['sourceStable']and v['artifactStable']and not v['errors']
 assert v['sourceBefore']['commit']==v['sourceAfter']['commit']==result['verification_fixture_revision']
 assert not v['sourceBefore']['dirty']and not v['setup']['build']['dirty']
 assert v['artifactBefore']==v['artifactAfter']and v['close']['closed']and not v['close']['errors']
 assert v['captureSnapshots']is False and v['pixelParity']is None
 assert v['fixture']['rawSamples']==7200 and v['fixture']['duplicates']==15 and v['fixture']['erasePoints']==25
 assert all(all(p['raw']['checks'].values())for p in v['phases'])
 assert all(all(p['raw']['checks'].values())for p in v['erasures'])
 assert all(p['raw']['quiescence']['pending']==0 for p in v['phases'])
 assert all(p['feedback']['contentGapMs']['max']>=result['target_max_gap_ms']for p in v['phases'][1:])
 assert w['cleanup_complete']and not w['remaining_owned_pids']and w['peak_process_family_rss_kib']<=w['rss_limit_kib']
 if name=='red':
  assert v['status']=='REPRODUCED_MASKED_NAVIGATION_STALL'and w['reason']=='completed'
 else:
  assert v['status']=='BLOCKED_ENDPOINT'and w['reason']=='nonzero_exit'and w['returncode']==1
  assert v['setup']['build']['sourceRevision']==result['code_revision']
for a,b in zip(observations['red']['phases'],observations['off']['phases']):
 assert a['raw']['after']['camera']==b['raw']['after']['camera']
 assert a['raw']['pixels']['sha256']==b['raw']['pixels']['sha256']
records=[json.loads(r['text'][24:])for p in observations['on']['phases'][1:]for r in p['raw']['nativeRecords']if r['text'].startswith('AXIOM_NAVIGATION_TIMING ')]
assert records and sum(x['gpu_contours']for x in records)>0
assert sum(x['coarse_masks']for x in records)==0
assert all(x['budget_used_bytes']<=96*1024**2 for x in records)
for name in ['g5-masked-clean-gpu-progress.json','g5-masked-clean-gpu-canonical.json','g5-coarse-gpu-qualified.json','g5-masked-mixed-gpu.json','g5-v4-canonical-gpu.json','g5-masked-affine-gpu.json']:
 v=read(R/'checks'/name);assert v['status']=='PASS'and v['result']['code']==0 and not v['pageErrors']
red=read(R/'checks/g5-coarse-gpu-red-final.json')
assert red['status']=='FAIL'and red['result']['code']==1
assert any('masked v4 coarse navigation still rasterized on CPU'in x for x in red['result']['errors'])
assert all(x['offBaselineImageEdge']==0 and x['changed']<=1 for x in read(R/'checks/g5-masked-pixel-comparison.json')['rows'])
for entry in read(R/'snapshot-index.json').values():
 b=gzip.decompress((R/entry['stored']).read_bytes());assert len(b)==entry['bytes']and sha(b)==entry['sha256']
assert all(x['unchangedFromAnchor']for x in read(R/'execution-inputs.json').values())
packages=read(R/'packages.json')
assert all(len(p['files'])==36 for p in packages.values())
for name in ['axiom.mjs','axiom.d.mts','pointer_input.mjs','axiom.data']:
 assert packages['ordinary']['files'][name]==packages['diagnostic']['files'][name]
assert all(read(R/'checks/g5-masked-demo-smoke.json')['checks'].values())
assert '# pass 42'in(R/'checks/g5-masked-node-verified.log').read_text()
assert '# fail 0'in(R/'checks/g5-masked-node-verified.log').read_text()
assert '119 contract files'in(R/'checks/g5-masked-boundaries-verified.log').read_text()
print(json.dumps({'status':'VERIFIED_BLOCKED_ENGINEERING_RESULT','files':len(inventory),'native_passed':native['passed'],'native_total':native['total'],'unchanged_native_failures':len(native['failed_names']),'ordinary_pixel_checks':len(observations['off']['phases']),'officialGatePassClaimed':False}))
