"""Materialize the independently authorized remaining36 continuation."""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
CONTROL=ROOT/'.aegis/results/G5-UR0b/control-review-correctness-final'
P=ROOT/'.aegis/packages/G5-UR0b-resume-r5'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import source_identity
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
load=lambda p:json.loads(p.read_text())
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p),'identity_kind':'local-content-addressed'}
assert Path.cwd()==ROOT
assert subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],text=True).strip()=='codex/geometry-navigation-integration'
assert sha(CONTROL/'continuation-request.json')=='151aad4d4625a671c4132fc614416c9536e76d74c7f23722b2677ef29b93e501'
request=load(CONTROL/'continuation-request.json')
assert request['status']=='AUTHORIZED_BOUNDED_REVERIFICATION_CONTINUATION'
refs={}
def verify(x):
 if isinstance(x,dict):
  if isinstance(x.get('path'),str) and 'sha256'in x:
   q=(ROOT/x['path']).resolve();assert q.is_relative_to(ROOT) and q.is_file() and sha(q)==x['sha256'],x['path'];refs[x['path']]=x['sha256']
  for v in x.values():verify(v)
 elif isinstance(x,list):
  for v in x:verify(v)
verify(request);verify(load(CONTROL/'reuse-acceptance.json'))
for k in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',request[k]['revision'],'HEAD'],check=True)
current=source_identity.identity(ROOT)
assert current['source_tree_sha256']==request['resume_cursor']['accepted_source_sha256']
assert not (D/'window.json').exists() and not (D/'admission-stop.json').exists()
write(D/'accepted-start-source-identity.json',current)
P.mkdir(parents=True,exist_ok=False)
write(P/'authority.lock.json',{'schema_version':1,'status':'CURRENT',
 'continuation_request_ref':ref(CONTROL/'continuation-request.json'),
 'independent_review_ref':request['review_result_ref'],
 'accepted_reuse_ref':request['accepted_reuse_ref'],
 'accepted_source_identity_ref':ref(ROOT/'.aegis/results/G5-UR0b/correctness-test-reverify-r1/full-source-identity.json'),
 'artifact_mechanism_ref':request['accepted_artifact_provenance_ref'],
 'historical_ledger_ref':request['historical_ledger_ref'],
 'trusted_authorization':request['authority'],
 'old_windows_reopened':False})
write(P/'verification.lock.json',{'schema_version':1,'status':'CURRENT',
 'inherited_verification_ref':ref(ROOT/'.aegis/packages/G5-UR0b-correctness-repair-r2/verification.lock.json'),
 'initialization_spec_ref':ref(ROOT/'.aegis/verification/G5-UR0b-initialization-v1/verification-spec.json'),
 'accepted_reuse_ref':request['accepted_reuse_ref'],
 'obligations':[{'requirement':v,'oracle':'Exact frozen input/guard/admission validation, offline tests before starts, then actual named workers only.','acceptance':'No invented source/receipt/worker status, no stale or incomplete pair admitted.','evidence_required':['Bound machine facts and test logs','Actual guards/window/cleanup/fresh-or-recorded proof identities']} for v in request['required_preflight_and_orchestration']],
 'required_tests':request['required_tests'],
 'pixel_tolerance_and_correctness_phase':'Unchanged inherited specification; no new acceptance threshold.'})
(P/'run-plan.json').write_bytes((CONTROL/'remaining-run-plan.json').read_bytes())
write(P/'execution-contract.json',{'schema_version':1,'EXECUTION_CLOSURE_CONTRACT':{
 'authorized_source_paths':request['source_scope'],'authorized_outputs':request['outputs'],
 'read_only':request['read_only'],'required_changes':request['required_preflight_and_orchestration'],
 'required_tests':request['required_tests'],'sampling_limits':request['sampling_limits'],
 'run_plan_ref':ref(CONTROL/'remaining-run-plan.json'),'historical_ledger_ref':request['historical_ledger_ref'],
 'accepted_reuse_ref':request['accepted_reuse_ref'],'fixed_state_root':request['state_root'],
 'execution_design':'New orchestration owns the durable claim, reserve/charge/close ledger and sequencing. Existing immutable observed_worker/bounded_worker own process containment. Existing current canonical and compilation readers own native/receipt proof validation. Guard failure precedes cold compilation or missing-summary diagnostics.',
 'forbidden':['Any hidden/extra worker, cohort/RT rerun, retry, relocation/restart allowance','Any source outside the two named paths','Build/dependency/SDK/profile/threshold/corpus mutation','Any prior window/package/result/evidence mutation','Commit/push, UR0c/UR1, performance/platform/P34 PASS'],
 'hosted_verification':{'required':[],'optional':[]},
 'blocking_evidence':request['required_tests'],
 'terminal_success':request['return_policy']['success'],
 'terminal_blockers':request['return_policy']['blockers'],
 'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','stop_after':'UR0b',
 'notify_terminal':request['notify_terminal']}})
(P/'implementation-context.md').write_text('''# r5 bounded remaining execution

Only the new reverified_remaining_runner.py and its new offline test may change.
Preserve the completed harness/profile/compiler and old windows. New independent
acceptance has17 records with two original source groups; verify each exact row,
not the old r4 packet or a fabricated shared source. OFFLINE remains recorded.

Use a fixed r5 state root and durable exclusive claim, serial1, full reservation
before spawn, exact actual ceil charge, full charge for unknown interrupted guard.
Guard/nonzero/cleanup failure is the primary cause, before cold compile/summary
checks. First failure closes the window and marks pending rows MISSING. No retry.

Consume exact existing SDK artifacts and native evidence, not rebuilds. Offline
tests and mandatory suites run before the sole real window. Then execute the
6 paired correctness workers, followed by30 main only if all prerequisites are
actually admitted by inherited rules. Preserve complete matching product RED.
Return after all36 or the first real frozen stop, with exact guards/cleanup,
current/fresh vs recorded identities and the original33starts/1251seconds intact.
No intermediate approval checkpoint, performance/Gate PASS or commit/push.
''')
write(P/'evidence-contract.json',{'schema_version':1,
 'continuation_request_ref':ref(CONTROL/'continuation-request.json'),
 'materialization':'Shared controlled worktree content-addressed bundle, exact result/manifest SHA256; no commit/push.',
 'result_root':request['state_root'],'evidence_root':'verification/evidence/performance/g5-unified-render-ur0b/<new-source-sha>/resume-r5/',
 'required':['All exact17 reuse/source/artifact/compiler/native proofs','Offline runner/mandatory test logs plus2 explicitly permitted malformed native unit checks','Source/scope/mechanism/artifact proof before the sole claim','Durable historical33/1251 plus actual new starts/ceil charges/unknowns/reservations ledger','Every started run guard/raw/compiler/receipt/correctness result and owned process cleanup','All remaining rows MISSING on first hard stop, preserving cause before missing-summary errors','READY_FOR_CONTROL_REVIEW or exact explicit blocker, with proactive terminal notice'],
 'notification':request['notify_terminal'],'no_official_gate_pass':True})
package={'schema_version':1,'task_id':'G5-UR0b-resume-r5','status':'AUTHORIZED_BOUNDED_REVERIFICATION_CONTINUATION','execution_authority_mode':'repo_materialized','repository':request['repository'],'worktree':request['worktree'],'execution_ref':request['execution_ref'],'task_anchor':request['task_anchor'],'resume_cursor':request['resume_cursor'],'authorization':{'continuation_request_ref':ref(CONTROL/'continuation-request.json'),'commit_push':False},'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','stop_after':'UR0b'}
for key,name in [('authority_lock_ref','authority.lock.json'),('verification_lock_ref','verification.lock.json'),('execution_contract_ref','execution-contract.json'),('implementation_context_ref','implementation-context.md'),('evidence_contract_ref','evidence-contract.json'),('run_plan_ref','run-plan.json')]:package[key]={'path':name,'sha256':sha(P/name)}
write(P/'package.json',package)
for key,v in package.items():
 if key.endswith('_ref') and isinstance(v,dict):assert (P/v['path']).resolve().parent==P and sha(P/v['path'])==v['sha256']
write(D/'package-preflight.json',{'status':'PASS','package_ref':ref(P/'package.json'),'verified_input_refs':refs,'starting_source_sha256':current['source_tree_sha256'],'new_window_claimed':False,'new_workers':0})
print(json.dumps({'package_ref':ref(P/'package.json'),'state_root':str(D),'bindings':len(refs)}))
