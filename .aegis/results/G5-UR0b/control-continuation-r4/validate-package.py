"""Package/recorded-reuse/budget preflight. Never starts a browser/worker."""
from pathlib import Path
import json,hashlib,math,sys,subprocess
R=Path(__file__).resolve().parents[4];D=R/'.aegis/packages/G5-UR0b-resume-r4';C=Path(__file__).resolve().parent
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();refs=[]
def check_ref(x,base=R):
 p=(base/x['path']).resolve();assert p.is_relative_to(R) and p.is_file();assert sha(p)==x['sha256'],str(p);refs.append(str(p.relative_to(R)));return load(p) if p.suffix=='.json' else p.read_text()
def walk(x):
 if isinstance(x,dict):
  if set(['path','sha256'])<=set(x):check_ref(x)
  for v in x.values():walk(v)
 elif isinstance(x,list):
  for v in x:walk(v)
pkg=load(D/'package.json');assert sha(D/'package.json')=='2cadd6e12c73dc4006fd20691feca9402974a9f31fe705b2200c4654e020b569'
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
for stamp in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',pkg[stamp]['revision'],'HEAD'],cwd=R,check=True)
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 p=(D/pkg[key]['path']).resolve();assert p.parent==D;check_ref(pkg[key],D)
for name in ['authority.lock.json','verification.lock.json','execution-contract.json','evidence-contract.json','run-plan.json','historical-budget.json','reuse-inputs.json']:walk(load(D/name))
history=load(D/'historical-budget.json');total=0
for row in history['rows']:
 if 'guard_ref' in row:
  g=load(R/row['guard_ref']['path']);assert g['cleanup_complete'] and g['elapsed_s']==row['elapsed_seconds'];assert math.ceil(g['elapsed_s'])==row['charged_seconds']
 else:
  ledger=load(R/row['ledger_ref']['path']);x=next(r for r in ledger if r['run_id']=='qualification-p2-A');assert x['status']=='CONTROL_STOP' and row['charged_seconds']==90
 total+=row['charged_seconds']
assert len(history['rows'])==30 and len({x['run_key'] for x in history['rows']})==30 and total==1133
plan=load(D/'run-plan.json');rows=plan['rows'];window=check_ref(plan['predecessor_closed_window_ref']);assert window['status']=='CLOSED'
old_unstarted=[r for r in window['rows'] if r['status']=='MISSING'];old_started={r['run_id'] for r in window['rows'] if 'outcome' in r};assert len(rows)==38 and not old_started.intersection(r['run_id'] for r in rows)
assert [r['run_id'] for r in rows]==[r['run_id'] for r in old_unstarted]
for a,b in zip(rows,old_unstarted):
 for k,v in b.items():
  if k not in ['reason','status']:assert a[k]==v,(a['run_id'],k)
 assert a['status']=='PLANNED' and a['window_run_key']=='G5-UR0b-resume-r4/'+a['run_id']
assert [r['branch'] for r in rows[:2]]==['120','240'];assert sum(r.get('mode')=='correctness' for r in rows)==6;assert sum(r['phase']=='main' for r in rows)==30
assert sum(r['limit_seconds'] for r in rows)==6120 and total+6120==7253<=8370
sys.path.insert(0,str(R/'verification/runners/g5/ur0'));import repair_runner as admission;import source_identity
expected_main=[r for r in admission.plan() if r['phase']=='main'];actual_main=[r for r in rows if r['phase']=='main']
for a,b in zip(actual_main,expected_main):
 for k,v in b.items():
  if k!='reason':assert a[k]==v,(a['run_id'],k)
reused=load(D/'reuse-inputs.json');assert len(reused['records'])==15
for name,digest in reused['compiler_file_sha256'].items():assert sha(R/'verification/runners/g5/ur0'/name)==digest
for item in reused['records']:
 s=check_ref(item['accepted_derived_summary_ref']);p=check_ref(item['accepted_v2_proof_ref']);assert s['canonical_chain']==p and s['analysis']['mode']=='OFFLINE_REVERIFICATION';assert s['analysis']['input_source_tree_sha256']==reused['recording_source_tree_sha256'];assert s['analysis']['compiler_files']==reused['compiler_file_sha256']
 for field,value in item['expected_counts'].items():assert p[field]==value
 admission.require_canonical_chain(s,require_positive=item['run_id']=='diagnostic-ON')
current=source_identity.identity(R);accepted=load(R/'.aegis/results/G5-UR0b/control-review-init-repair-r1/result.json');assert accepted['status']=='ACCEPTED_OFFLINE_INITIALIZATION_REPAIR_AND_RECORDED_COHORT_REUSE'
accepted_result=load(R/'.aegis/results/G5-UR0b/init-repair-r1/result.json');base=load(Path(accepted_result['materialized_ref']['path'])/'source/full-source-identity.json')
delta=sorted(n for n in set(base['files'])|set(current['files']) if base['files'].get(n)!=current['files'].get(n));expected_additions=sorted(str(p.relative_to(R)) for p in D.iterdir() if p.is_file());assert delta==expected_additions
c=load(D/'execution-contract.json')['EXECUTION_CLOSURE_CONTRACT'];limits=c['execution_limits'];assert limits['new_workers_max']==38 and limits['concurrency']==1 and limits['process_family_rss_bytes']==3221225472;assert limits['new_diagnostic_or_overhead_workers']==0;assert not limits['extra_browser_preflights'] and not limits['unallocated_is_retry_authorization']
assert pkg['terminal_notification']['required'] and pkg['terminal_notification']['target_thread']==pkg['control_thread'];assert not pkg['authorization']['commit_push']
j={'status':'PACKAGE_BINDINGS_REUSE_AND_BUDGET_VERIFIED','package_sha256':sha(D/'package.json'),'hash_bound_refs_checked':len(refs),'new_plan_rows':38,'reused_recordings':15,'historical_starts':30,'historical_charged_seconds':total,'new_nominal_worker_limit_seconds_sum':6120,'cumulative_planned_seconds':7253,'original_cap_seconds':8370,'source_before_implementation_sha256':current['source_tree_sha256'],'accepted_source_sha256':base['source_tree_sha256'],'source_delta_only_package_files':delta,'new_browser_workers_by_control':0,'old_window_status':'CLOSED','terminal_notification_required':True}
(C/'validation.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps({k:v for k,v in j.items() if k!='source_delta_only_package_files'}))
