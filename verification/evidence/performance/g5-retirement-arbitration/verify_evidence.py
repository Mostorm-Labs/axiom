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
assert package['task_id']=='G5-RETIREMENT-ARBITRATION' and package['execution_ref']=='refs/heads/codex/geometry-navigation-integration'
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
guards={'clarityNoRegression2Points':all(p['afterLowDensity']<=p['beforeLowDensity']+.02 for c in comparisons if c['mode']=='on' for p in c['phases']),
        'waitNonRegression':all(p['afterWaitMs']<=p['beforeWaitMs']+max(16.7,.10*p['beforeWaitMs'])for c in comparisons for p in c['phases']),
        'inputGapNonRegression':all(p['afterGapMs']<=p['beforeGapMs']+max(16.7,.10*p['beforeGapMs'])for c in comparisons for p in c['phases'])}

assert guards==r['guards'] and correct==r['measurementBindingsAndPixels'] and r['boundedBrowserRuns']==len(runs)==22
for p in read('on-off-parity.json'):assert p['sameFinalPixels']
native=read('checks/g5-retire-native-comparison.json');gpu=read('checks/g5-retire-gpu-results.json');packages=read('checks/g5-retire-package-checks.json');demo=read('checks/g5-retire-demo-smoke.json')
regression=not native['new_failures'] and len(gpu)==10 and all(g['status']=='PASS' for g in gpu) and packages['passed'] and demo['passed']
assert native['tests']==112 and native['passed']==107 and len(native['failed_names'])==5 and set(native['failed_names'])==set(native['baseline_failed_names'])
for p in packages['profiles']:
 assert not p['forbiddenFiles'] and list(p['exports'])==['.'] and p['buildFacts']['AXIOM_RENDER_CACHE_MIB']=='32' and p['buildFacts']['AXIOM_RENDER_SHARED_MIB']=='96' and p['buildFacts']['AXIOM_INSTALL_DEVELOPER_COMPONENTS']=='OFF'
 assert p['buildFacts']['AXIOM_ENABLE_FRAME_TIMING']==p['profile'].upper()
for name,needle in [('node','# tests 31'),('node','# fail 0'),('boundaries','passed (118 contract files)')]:
 data=gzip.decompress((D/('checks/g5-retire-'+name+'.log.gz')).read_bytes()).decode();regression &= needle in data
assert regression==r['regression'];expected='READY_FOR_CONTROL_REVIEW' if correct and regression and all(guards.values()) else 'BLOCKED';assert r['status']==expected

green=next(g for g in gpu if g['mode']=='retirement');records=[json.loads(x)for x in green['observation']['logs']]
assert records[0]['retirement_frame']=='PASS' and records[0]['canonical_after']==records[0]['canonical_before']+1
cells=[x for x in records if 'retirement_cells'in x];assert len(cells)==2 and next(x for x in cells if x['retiring'])['evictions']==0 and next(x for x in cells if not x['retiring'])['evictions']>0
red=read('checks/g5-retire-red-oracles.json')
for mode in ['frame','cells']:
 o=red['oracles'][mode];assert o['result']['status']=='FAIL' and o['result']['observation']['errors']==[o['expectedFailure']] and o['worker']['cleanup_complete'] and not o['worker']['remaining_owned_pids']
aa=read('checks/g5-retire-aa-runs.json');aa_result=read('checks/g5-retire-aa-result.json');observed=[]
assert len(aa)==6
for row in aa:
 name=Path(row['output']).name;snapshot=read('corroborative-aa/'+name+'/summary.json');worker=read('checks/'+name+'-worker.worker.json')
 assert snapshot['sourceStable']and snapshot['artifactStable']and not snapshot['sourceBefore']['dirty']and snapshot['sourceBefore']['commit']==r['sourceRevision']
 assert snapshot['results'][0]['setup']['build']['sourceRevision']==r['baselineRevision']and not snapshot['results'][0]['setup']['build']['dirty']
 assert all(snapshot['results'][0]['checks'].values())and worker['returncode']==0 and worker['cleanup_complete']and not worker['remaining_owned_pids']
 assert worker['timeout_s']==180 and worker['rss_limit_kib']==3145728
for pair in range(3):
 a=next(x for x in aa if x['pair']==pair and x['side']=='first');b=next(x for x in aa if x['pair']==pair and x['side']=='second');assert a['artifacts']==b['artifacts']
 for p,q in zip(a['summary']['phases'],b['summary']['phases']):
  assert p['id']==q['id']and p['pixels']['sha256']==q['pixels']['sha256']
  s,t=p['summary'],q['summary'];low=lambda x:x['navigation']['quality']['timeWeightedLowResolutionFraction']
  v={'pair':pair,'phase':p['id'],'beforeWaitMs':s['settle_ms'],'afterWaitMs':t['settle_ms'],'beforeGapMs':s['input_display_submission_gap_ms']['max'],'afterGapMs':t['input_display_submission_gap_ms']['max'],'beforeLowDensity':low(s),'afterLowDensity':low(t)}
  v['guards']={'wait':v['afterWaitMs']<=v['beforeWaitMs']+max(16.7,.1*v['beforeWaitMs']),'gap':v['afterGapMs']<=v['beforeGapMs']+max(16.7,.1*v['beforeGapMs']),'clarity':low(t)<=low(s)+.02};observed.append(v)
assert observed==aa_result['comparisons']and [x for x in observed if not all(x['guards'].values())]==aa_result['falseRegressionReports']
repo=D.parents[3]
if (repo/'.git').exists():
 import subprocess
 auth=read('package/authority.lock.json')
 for path,sha in auth['inputs'].items():assert hashlib.sha256(subprocess.check_output(['git','show',auth['revision']+':'+path],cwd=repo)).hexdigest()==sha,path
 assert hashlib.sha256(subprocess.check_output(['git','show',r['sourceRevision']+':runtime/render/tests/render_cache_test.cpp'],cwd=repo)).hexdigest()==red['testSha256']
print('Evidence integrity verified:',len(listed),'files;',len(runs),'bounded browser runs;result='+r['status']+';guards='+json.dumps(guards,sort_keys=True))
