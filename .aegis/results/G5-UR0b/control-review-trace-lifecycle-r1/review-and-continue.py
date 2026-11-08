"""Independent lifecycle review, immutable record acceptance and bounded handoff."""
import copy
import hashlib
import json
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];OUT=Path(__file__).resolve().parent
read=lambda p:json.loads(Path(p).read_text())
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def ref(p):
 p=Path(p);return {'path':str(p.relative_to(ROOT)),'sha256':sha(p),'identity_kind':'local-content-addressed'}
def put(n,v):(OUT/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import source_identity,repair_runner
rp=ROOT/'.aegis/results/G5-UR0b/trace-lifecycle-repair-r1/result.json'
assert sha(rp)=='fd38c6a1be1411eb57b5618396c689a548d3a9ec2c06378f22a5815934d4a1c5'
r=read(rp);bundle=Path(r['materialized_ref']['path']);manifest=read(bundle/'manifest.json')
assert sha(bundle/'manifest.json')==r['materialized_ref']['manifest_sha256']
for n,item in manifest['files'].items():assert sha(bundle/n)==item['sha256'],n
for key,item in r.items():
 if key.endswith('_ref') and isinstance(item,dict) and 'sha256' in item:assert sha(ROOT/item['path'])==item['sha256'],key
current=source_identity.identity(ROOT)
assert current['source_tree_sha256']==r['source_tree_sha256']
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=ROOT,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()=='codex/geometry-navigation-integration'
assert '# pass 119' in (OUT/'node.log').read_text() and '# fail 0' in (OUT/'node.log').read_text()
assert 'Ran 76 tests' in (OUT/'python.log').read_text() and (OUT/'python.log').read_text().rstrip().endswith('OK')
assert 'passed (120 contract files)' in (OUT/'boundary.log').read_text()
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
old_accept=read(ROOT/'.aegis/results/G5-UR0b/control-review-correctness-final/reuse-acceptance.json')
rows={x['run_id']:x for x in read(ROOT/r['reverification_ref']['path'])['rows']}
assert set(rows)=={x['run_id'] for x in old_accept['records']}|{'actual-r5-lower-z'}
compiler={n:sha(ROOT/'verification/runners/g5/ur0'/n) for n in old_accept['compiler_file_sha256']}
for n,d in old_accept['repaired_worker_and_helpers'].items():assert sha(ROOT/n)==d,n
assert old_accept['repaired_worker_and_helpers']==repair_runner.REPAIRED_INITIALIZATION_PROFILE
accepted=[]
for old in old_accept['records']:
 name=old['run_id'];row=rows[name];directory=bundle/'execution-and-checks/recompiled-recordings'/name
 summary=read(directory/'compiled-summary.json');baseline=ROOT/old['summary_ref']['path']
 for n in ['canonical-chain.json','decoded.json','metrics.json','external-metrics.json','compiler-provenance.json']:
  assert sha(directory/n)==sha(baseline.parent/n),(name,n)
 previous=read(baseline);assert {k:v for k,v in summary.items() if k!='analysis'}=={k:v for k,v in previous.items() if k!='analysis'},name
 a=summary['analysis'];assert a['compiler_files']==compiler and a['mode']=='OFFLINE_REVERIFICATION'
 for k in ['input_source_tree_sha256','raw_trace_sha256','worker_result_sha256','run_inputs_sha256','verification_spec_ref','completion_contract_ref']:assert a[k]==previous['analysis'][k],(name,k)
 for item,key in zip(old['recorded_input_refs'],['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):assert sha(ROOT/item['path'])==item['sha256']==a[key],name
 repair_runner.require_canonical_chain(summary,require_positive=old['counts']['qualified_committed_count']>0)
 item=copy.deepcopy(old);item['summary_ref']=ref(directory/'compiled-summary.json');item['proof_ref']=ref(directory/'canonical-chain.json');accepted.append(item)

case_row=rows['actual-r5-lower-z'];case_dir=bundle/'execution-and-checks/recompiled-recordings/actual-r5-lower-z'
s=read(case_dir/'compiled-summary.json');chain=s['canonical_chain']
own=read(OUT/'actual-r5/compiled-summary.json');assert own==s
assert sha(OUT/'actual-r5/canonical-chain.json')==sha(case_dir/'canonical-chain.json')
expected={'accepted_raw_count':6,'qualified_initialization_count':1,'qualified_committed_count':5,'qualified_decoded_count':5,'metric_receipt_count':None,'unresolved_accepted_count':0}
for k,v in expected.items():assert chain[k]==v,k
assert chain['scope']=='NATIVE_ONLY' and chain['status']=='QUALIFIED' and not chain['unresolved']
assert s['schema_status']=='MISSING' and not (case_dir/'metrics.json').exists()
repair_runner.require_canonical_chain(s,require_metrics=False)
rawrefs=case_row['original_input_refs']
for item,key in zip(rawrefs,['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):assert sha(ROOT/item['path'])==item['sha256']==s['analysis'][key]
run=read(ROOT/rawrefs[1]['path']);inputs=read(ROOT/rawrefs[2]['path']);raw=[json.loads(line) for line in (ROOT/rawrefs[0]['path']).read_text().splitlines() if line.strip()]
allowed=set(read(ROOT/'.aegis/verification/G5-UR0b-trace-lifecycle-v1/implementation-request.json')['source_scope'])
delta=sorted(n for n in current['files'].keys()|run['source_before']['files'].keys() if current['files'].get(n)!=run['source_before']['files'].get(n))
assert all(n in allowed or n.startswith(('.aegis/verification/G5-UR0b-trace-lifecycle-v1/','.aegis/packages/G5-UR0b-trace-lifecycle-repair-r1/')) for n in delta),delta
assert sum(n in allowed for n in delta)==5
origin=chain['initialization_chains'][0]['initialization_basis']
assert origin['trace_scope']==repair_runner.trace_scope_for_recording(run,inputs,len(raw))
assert origin['state_path']=='worker-result.json/paired_init/state' and len(origin['recorded_states'])==2
assert origin['recorded_states'][0]['public_state']==origin['recorded_states'][1]['public_state']
assert origin['recorded_states'][0]['page_time_origin_ms']!=origin['recorded_states'][1]['page_time_origin_ms']
assert origin['input_refs']=={k:s['analysis'][k] for k in ['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']}
original_dir=(ROOT/rawrefs[1]['path']).parent;original=read(original_dir/'compiled-summary.json')
assert chain['chains']==original['canonical_chain']['chains']
assert {(x['completion_seq'],x['work_id']) for x in chain['accepted_records']}=={(x['completion_seq'],x['work_id']) for x in original['canonical_chain']['accepted_records']}
assert s['analysis']['mode']=='OFFLINE_REVERIFICATION' and s['analysis']['input_source_tree_sha256']==run['source_before']['source_tree_sha256']=='17affbd9dcf5b2c262755fb2320aa617a815a0a021a18c479eea596e50f13dbe'
entries=run['correctness'];assert {(e['toggle'],e['step']) for e in entries}=={('OFF',0),('OFF',1),('ON',0),('ON',1)}
assert all(e['comparison']['status']=='QUALIFIED' and e['comparison']['interior_mismatched_pixels']==0 for e in entries)
assert all(e['applicability']['status']=='QUALIFIED' for e in entries if e['step']==0)
window_ref=read(ROOT/'.aegis/results/G5-UR0b/resume-r5/result.json')['window_ref']
assert sha(ROOT/window_ref['path'])==window_ref['sha256']
window=read(ROOT/window_ref['path']);assert window['status']=='CLOSED' and window['new_starts']==1 and window['charged_seconds']==1264
guard=window['rows'][0]['outcome']['guard'];assert guard['reason']=='completed' and guard['returncode']==0 and guard['cleanup_complete'] and not guard['remaining_owned_pids']
case={'case_id':'lower_z_translucent_drag','status':'ACCEPTED_RECORDED_CORRECTNESS_AFTER_SCOPED_OFFLINE_REVERIFICATION','fresh':False,
 'recording_mode':'WORKER_COMPILE','derived_mode':'OFFLINE_REVERIFICATION','recording_revision':run['source_before']['revision'],
 'recording_source_tree_sha256':run['source_before']['source_tree_sha256'],'input_refs':rawrefs,
 'summary_ref':ref(case_dir/'compiled-summary.json'),'proof_ref':ref(case_dir/'canonical-chain.json'),
 'counts':expected,'canonical_scope':'NATIVE_ONLY','metrics_not_applicable':True,'trace_scope':origin['trace_scope'],
 'pixel_status':'QUALIFIED','subject_status':'QUALIFIED','guard':guard,'old_window_ref':window_ref,'old_admission_remains':'REJECTED'}
check={'status':'INDEPENDENT_RECORD_AND_LIFECYCLE_AUDIT_PASS','subject_ref':ref(rp),'manifest_ref':ref(bundle/'manifest.json'),
 'payload_files':len(manifest['files']),'current_source_sha256':current['source_tree_sha256'],'compiler_files':compiler,
 'old17_invariance_verified':True,'r5_independent_compilation_equal':True,'raw_accepted_set_preserved':True,'five_committed_chains_preserved':True,
 'actual_case':case,'independent_source_delta':delta,'new_browser_workers':0,'native_qualification_probes':0}
put('independent-check.json',check)
reuse=copy.deepcopy(old_accept)
reuse.update(schema='ur0b-independent-scoped-recorded-reuse-v1',status='ACCEPTED_CURRENT_COMPILER_17_PREREQUISITES_AND_ONE_CORRECTNESS',
 compiler_source_reviewed_at=current['source_tree_sha256'],compiler_file_sha256=compiler,records=accepted,recorded_correctness_cases=[case],
 independent_reconciliation_ref=ref(OUT/'independent-check.json'),
 inherited_acceptance_ref=ref(ROOT/'.aegis/results/G5-UR0b/control-review-correctness-final/reuse-acceptance.json'))
reuse['accepted_prerequisites']['recorded_correctness']=['lower_z_translucent_drag']
reuse['applicability'].append('The one correctness case is the original completed r5 recording with corrected ON-page source proof. It is native-only with metrics not applicable, not a fresh worker or retroactive r5 admission; its exact4pixel/2subject/guard evidence is unchanged.')
put('reuse-acceptance.json',reuse)
review={'schema_version':1,'primary_owner':'aegis-gate-review','status':'ACCEPTED_TRACE_LIFECYCLE_REPAIR_AND_RECORDED_CORRECTNESS',
 'subject_ref':ref(rp),'source_tree_sha256':current['source_tree_sha256'],'revision':current['revision'],'dirty':True,
 'frozen_requirements_passed':['Scoped final-ON-page protocol before unique native/state matching','Exact unchanged worker/helpers/profile/Runtime/SDK',
 '17 invariant prior records and actual r5 native-only6=1+5+0, no raw/committed/pixel mutation','All9 frozen oracles and independent regression'],
 'independent_regression':{'node':119,'python':76,'boundary_contracts':120,'malformed_native_unit_checks':2,'new_browser_workers':0},
 'independent_evidence_ref':ref(OUT/'independent-check.json'),'reuse_acceptance_ref':ref(OUT/'reuse-acceptance.json'),
 'old_r5_status':'REJECTED_CLOSED_UNCHANGED','historical_starts':34,'historical_seconds':1264,
 'remaining':['5 real correctness workers','30 main rows'],'full_ur0b_status':'BLOCKED_EVIDENCE_PENDING_REMAINING_REAL_BROWSER_RUNS',
 'findings':['One offline compiler-child wait timeout was preserved; deterministic final byte invariance is verified. No new browser attempt or performance selection occurred.'],
 'performance_platform_or_official_P34_PASS':False}
put('result.json',review)

old_request=read(ROOT/'.aegis/results/G5-UR0b/control-review-correctness-final/continuation-request.json')
old_plan=read(ROOT/'.aegis/packages/G5-UR0b-resume-r5/run-plan.json')['rows']
planned=copy.deepcopy(old_plan[1:]);actual={x['run_id']:x for x in window['rows']}
for row in planned:
 row.update(window_run_key='G5-UR0b-resume-r6/'+row['run_id'],predecessor_status=actual[row['run_id']]['status'],predecessor_admission_status=actual[row['run_id']].get('admission_status','NOT_STARTED'))
assert len(planned)==35 and sum(x.get('mode')=='correctness' for x in planned)==5 and sum(x['limit_seconds'] for x in planned)==5850
put('remaining-run-plan.json',{'schema':'ur0b-scoped-remaining35-v1','rows':planned,'recorded_correctness_prerequisite':case,
 'origin_ref':ref(ROOT/'.aegis/packages/G5-UR0b-resume-r5/run-plan.json'),'historical_starts':34,'historical_seconds':1264,
 'new_max_workers':35,'new_nominal_seconds':5850,'cumulative_nominal_seconds':7114,'original_cap_seconds':8370})
request=copy.deepcopy(old_request)
request.update(schema='ur0b-scoped-recorded-correctness-continuation-v1',status='AUTHORIZED_BOUNDED_REVERIFICATION_CONTINUATION',
 review_result_ref=ref(OUT/'result.json'),accepted_reuse_ref=ref(OUT/'reuse-acceptance.json'),
 package_to_materialize='.aegis/packages/G5-UR0b-resume-r6/package.json',
 source_scope=['verification/runners/g5/ur0/scoped_remaining_runner.py','verification/tests/test_g5_ur0_scoped_remaining_runner.py'],
 new_control_paths=['.aegis/packages/G5-UR0b-resume-r6/**'],run_plan_ref=ref(OUT/'remaining-run-plan.json'),
 historical_ledger_ref=window_ref,state_root='.aegis/results/G5-UR0b/resume-r6/',
 outputs=['.aegis/results/G5-UR0b/resume-r6/**','out/ur0b/resume-r6/**','verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/resume-r6/**'])
request['resume_cursor']={'revision':current['revision'],'accepted_source_sha256':current['source_tree_sha256'],
 'completed_through':'Independent lifecycle repair acceptance, current compiler17 prerequisites and exact one recorded lower-z correctness case; all old windows closed.',
 'next_action':'Materialize/test new r6-window orchestration, then run the5remaining correctness and conditional30main continuously until terminal result.'}
request['authority']='Existing human continuation/delegation plus independent repair acceptance. This request authorizes exactly35 named new starts, not reopening or retrying any old window. The recorded lower-z case is explicitly reused, never silently marked fresh or retroactively admitted.'
limits=request['sampling_limits'];limits.update(new_workers_max=35,new_nominal_worker_seconds_max=5850,prior_starts=34,prior_seconds=1264,cumulative_nominal_seconds=7114)
request['required_preflight_and_orchestration']=[
 'Explicitly consume this new current-compiler acceptance, validating17 exact metrics/native prerequisites and the distinct one native-only recorded correctness. Preserve each original source/profile/input/artifact and offline identity; never run lower-z again or fabricate a metrics row for it.',
 'The recorded correctness source17aff and raw/worker/input/guard/4pixel/2subject/proof6=1+5+0 must match this control record. Validate its ON-page lifecycle and scope spec via current strict readers. Accept it only in the new baseline aggregate, leaving r5 REJECTED/CLOSED unchanged.',
 'Main requires this one exact recorded correctness plus all5new correct cases admitted under the inherited full-pair QUALIFIED or exact matching PRODUCT_RED_IN_BOTH_OFF_ON rule. No duplicated/missing case, source alias or dropping accepted native records.',
 *old_request['required_preflight_and_orchestration'][1:]]
request['required_preflight_and_orchestration']=[x.replace('all6 fresh correctness prerequisites','all5 new correctness prerequisites plus the recorded lower-z case').replace('all17 listed recordings','all17 prerequisite recordings') for x in request['required_preflight_and_orchestration']]
request['required_preflight_and_orchestration']=[x.replace('single fixed r5 state root','single fixed r6 state root') for x in request['required_preflight_and_orchestration']]
request['required_tests']=[x.replace('exact17 reuse','exact17 prerequisite reuse and1native-only correctness reuse').replace('prior33/1251','prior34/1264').replace('6-correctness-before-main','recorded1+fresh5 correctness-before-main').replace('All36','All35') for x in old_request['required_tests']]
request['accepted_artifact_provenance_ref']=read(ROOT/'.aegis/results/G5-UR0b/resume-r5/result.json')['mechanism_proofs_ref']
request['return_policy']['success']='READY_FOR_CONTROL_REVIEW with35 new planned rows accounted and explicit17+1recorded reuse; no product/performance/P34PASS.'
request['notify_terminal']['contents']=[x.replace('original33 starts1251s','original34 starts1264s') for x in request['notify_terminal']['contents']]
put('continuation-request.json',request)
print(json.dumps({'status':review['status'],'review_ref':ref(OUT/'result.json'),'reuse_ref':ref(OUT/'reuse-acceptance.json'),
 'continuation_ref':ref(OUT/'continuation-request.json'),'new_workers':35,'recorded_correctness':1}))
