#!/usr/bin/env python3
"""Recheck durable evidence integrity and independently recompute frozen guards.
A successful verifier means evidence is internally consistent;result.json owns
READY/BLOCKED. A BLOCKED result never becomes a performance pass here.
"""
from pathlib import Path
import json,gzip,hashlib
D=Path(__file__).resolve().parent
def read(name):
 p=D/name
 return json.loads(p.read_text()) if p.exists() else json.loads(gzip.decompress(Path(str(p)+'.gz').read_bytes()))
m=read('manifest.json');listed=m['files']
actual={str(p.relative_to(D)) for p in D.rglob('*') if p.is_file() and p.name!='manifest.json'}
assert set(listed)==actual,'unlisted or missing evidence file'
for n,h in listed.items():assert hashlib.sha256((D/n).read_bytes()).hexdigest()==h,n
package=read('package/package.json')
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 ref=package[key];assert hashlib.sha256((D/'package'/ref['path']).read_bytes()).hexdigest()==ref['sha256'],key
assert package['task_id']=='G5-MATERIAL-TILE' and package['execution_ref']=='refs/heads/codex/geometry-navigation-integration'
r=read('result.json');runs=read('runs.json');bindings=read('source-artifact-bindings.json');comparisons=read('comparison.json')
assert r['officialGatePassClaimed'] is False and r['budgetMiB']==[32,96]
correct=True
for run in runs:
 s=read('runs/'+run['id']+'/summary.json');row=s['results'][0];b=bindings[run['id']];expected=r['baselineRevision'] if run['side']=='before' else r['sourceRevision']
 correct &= bool(s['status']=='MEASURED' and s['sourceStable'] and s['artifactStable'] and not s['sourceBefore']['dirty'] and s['sourceBefore']['commit']==r['sourceRevision'] and row['setup']['build']['sourceRevision']==expected and not row['setup']['build']['dirty'] and all(row['checks'].values()) and row['setupQuiescence']['stopped'])
 assert b['artifacts']==s['artifacts'] and b['sourceBefore']==s['sourceBefore'] and b['sourceAfter']==s['sourceAfter']
 w=run['worker'];correct &= w['returncode']==0 and w['reason']=='completed' and w['cleanup_complete'] and not w['remaining_owned_pids'] and w['peak_process_family_rss_kib']<=w['rss_limit_kib'] and w['elapsed_s']<=w['timeout_s']
 assert w['timeout_s']==180 and w['rss_limit_kib']==3145728
for a in runs:
 if a['side']!='before':continue
 b=next(x for x in runs if x['dpr']==a['dpr'] and x['case']==a['case'] and x['mode']==a['mode'] and x['side']=='after')
 c=next(x for x in comparisons if x['dpr']==a['dpr'] and x['case']==a['case'] and x['mode']==a['mode'])
 assert len(a['phases'])==len(b['phases'])==len(c['phases'])
 for p,q,x in zip(a['phases'],b['phases'],c['phases']):
  same=p['id']==q['id'] and p['pixels']['sha256']==q['pixels']['sha256'];assert x['sameFinalPixels']==same;correct &= same
  assert x['beforeWaitMs']==p['summary']['settle_ms'] and x['afterWaitMs']==q['summary']['settle_ms']
  assert x['beforeGapMs']==p['summary']['input_display_submission_gap_ms']['max'] and x['afterGapMs']==q['summary']['input_display_submission_gap_ms']['max']
  for prefix,phase in [('before',p),('after',q)]:
   quality=((phase['summary'].get('navigation') or {}).get('quality') or {}).get('timeWeightedLowResolutionFraction');assert x[prefix+'LowDensity']==quality
cold=next(x for c in comparisons if c['dpr']==4 and c['mode']=='on' for x in c['phases'] if x['phase']=='after-freeze')
guards={'cold15Percent':cold['afterWaitMs']<=.85*cold['beforeWaitMs'],'clarityNoRegression2Points':all(p['afterLowDensity']<=p['beforeLowDensity']+.02 for c in comparisons if c['mode']=='on' for p in c['phases']),'navigation150Ms':all(p['afterGapMs']<=150 for c in comparisons for p in c['phases'])}
assert guards==r['guards'] and correct==r['measurementBindingsAndPixels'] and r['boundedBrowserRuns']==len(runs)==22
for p in read('on-off-parity.json'):assert p['sameFinalPixels']
native=read('checks/g5-material-window-native-comparison.json');gpu=read('checks/g5-material-window-gpu-results.json');packages=read('checks/g5-material-window-package-checks.json');demo=read('checks/g5-material-window-demo-smoke.json')
regression=not native['new_failures'] and len(gpu)==10 and all(g['status']=='PASS' for g in gpu) and packages['passed'] and demo['passed']
assert native['tests']==112 and native['passed']==107 and len(native['failed_names'])==5 and set(native['failed_names'])==set(native['baseline_failed_names'])
for p in packages['profiles']:
 assert not p['forbiddenFiles'] and list(p['exports'])==['.'] and p['buildFacts']['AXIOM_RENDER_CACHE_MIB']=='32' and p['buildFacts']['AXIOM_RENDER_SHARED_MIB']=='96' and p['buildFacts']['AXIOM_INSTALL_DEVELOPER_COMPONENTS']=='OFF'
 assert p['buildFacts']['AXIOM_ENABLE_FRAME_TIMING']==p['profile'].upper()
for name,needle in [('node','# tests 31'),('node','# fail 0'),('boundaries','passed (118 contract files)')]:
 data=gzip.decompress((D/('checks/g5-material-window-'+name+'.log.gz')).read_bytes()).decode();regression &= needle in data
assert regression==r['regression'];expected='READY_FOR_CONTROL_REVIEW' if correct and regression and all(guards.values()) else 'BLOCKED';assert r['status']==expected

material=next(g for g in gpu if g['mode']=='material');rows=[json.loads(x) for x in material['observation']['logs'] if x.startswith('{"variant"')]
assert len(rows)==11 and all(x['changed_channels']==0 for x in rows)
assert all(x['material_tiles']>0 and x['scratch_peak_bytes']==0 and x['material_peak_bytes']<=532512 for x in rows if x['variant']<8 or x['variant']==10)
assert all(x['material_tiles']==0 for x in rows if x['variant'] in [8,9])
native_material=[json.loads(x)for x in gzip.decompress((D/'checks/g5-material-window-native-material.log.gz').read_bytes()).decode().splitlines()]
assert len(native_material)==11 and all(x['changed_channels']==0 and x['material_tiles']==0 for x in native_material)
if r.get('withdrawnExperiment'):

 active=read('retained-candidate.json');assert active==r['activeCandidate'] and not active['codeDiff'] and r['activePerformanceAcceptance']=='NOT_ACCEPTED'
 root=read('retained-root/summary.json');assert root['status']=='MEASURED' and root['sourceStable'] and root['artifactStable'] and not root['sourceBefore']['dirty'] and root['sourceBefore']['commit']==active['activeSourceRevision']
 row=root['results'][0];assert row['setup']['build']['sourceRevision']==active['activeSourceRevision'] and not row['setup']['build']['dirty'] and all(row['checks'].values())
 held=read('retained-checks/g5-material-retained-gpu-results.json');assert len(held)==9 and all(g['status']=='PASS'for g in held)
 assert read('retained-checks/g5-material-retained-demo-smoke.json')['passed'] and read('retained-checks/g5-material-retained-package-checks.json')['passed']
 assert '100% tests passed, 0 tests failed out of 8' in gzip.decompress((D/'retained-checks/g5-material-retained-target.log.gz').read_bytes()).decode()
 for name,needle in [('node','# tests 31'),('node','# fail 0'),('boundaries','passed (118 contract files)')]:
  assert needle in gzip.decompress((D/('retained-checks/g5-material-retained-'+name+'.log.gz')).read_bytes()).decode()
 repo=D.parents[3]
 if (repo/'.git').exists():
  import subprocess
  auth=read('package/authority.lock.json')
  for path,sha in auth['inputs'].items():
   blob=subprocess.check_output(['git','show',auth['revision']+':'+path],cwd=repo)
   assert hashlib.sha256(blob).hexdigest()==sha,path
  assert not subprocess.check_output(['git','diff','--name-only',active['codeEquivalentRevision'],active['activeSourceRevision'],'--',*active['checkedPaths']],cwd=repo,text=True).strip()
print('Evidence integrity verified:',len(listed),'files;',len(runs),'bounded browser runs;result='+r['status']+';guards='+json.dumps(guards,sort_keys=True))
