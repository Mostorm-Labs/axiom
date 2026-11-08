"""Materialize only the already-approved offline initialization work order."""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent
DESIGN=ROOT/'.aegis/verification/G5-UR0b-initialization-v1'
PACKAGE=ROOT/'.aegis/packages/G5-UR0b-init-repair-r1'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import source_identity
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):Path(p).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p),'identity_kind':'local-content-addressed'}
assert Path.cwd()==ROOT and subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],text=True).strip()=='codex/geometry-navigation-integration'
assert sha(DESIGN/'manifest.json')=='65b67b3eefd556eb66479dbc037232b4a6d8c155039264d5d21b9edf69c7c382'
assert sha(DESIGN/'implementation-request.json')=='4f78456fd79ff23ba2075924b72d412a27bcccfb2a85f0e3b4e0052b6393547c'
manifest=load(DESIGN/'manifest.json')
for name,value in manifest['files'].items():assert sha(DESIGN/name)==value['sha256'],name
order=load(DESIGN/'implementation-request.json');spec=load(DESIGN/'verification-spec.json')
def refs(v):
 if isinstance(v,dict):
  if isinstance(v.get('path'),str) and 'sha256' in v:
   p=(ROOT/v['path']).resolve();assert p.is_relative_to(ROOT) and sha(p)==v['sha256'],v['path']
  for value in v.values():refs(value)
 elif isinstance(v,list):
  for value in v:refs(value)
for name in ['implementation-request.json','completion-contract.json','verification-spec.json','oracle-cases.json','evidence-inputs.json']:refs(load(DESIGN/name))
for key in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',order[key]['revision'],'HEAD'],check=True)
prior=load(ROOT/'.aegis/results/G5-UR0b/resume-r3/result.json');old=load(Path(prior['materialized_ref']['path'])/'source/full-source-identity.json')
current=source_identity.identity(ROOT)
changed=[n for n in set(old['files'])|set(current['files']) if old['files'].get(n)!=current['files'].get(n)]
assert all(n.startswith('.aegis/verification/G5-UR0b-initialization-v1/') for n in changed),changed
assert load(ROOT/'.aegis/results/G5-UR0b/resume-r3/window.json')['status']=='CLOSED'
recorded=Path(prior['materialized_ref']['path']);rm=load(recorded/'manifest.json');assert sha(recorded/'manifest.json')==prior['materialized_ref']['manifest_sha256']
assert all(sha(recorded/n)==v['sha256'] for n,v in rm['files'].items())
PACKAGE.mkdir(parents=True,exist_ok=False)
write(PACKAGE/'authority.lock.json',{'schema_version':1,'status':'CURRENT','work_order_ref':ref(DESIGN/'implementation-request.json'),'design_manifest_ref':ref(DESIGN/'manifest.json'),'current_spec_ref':ref(DESIGN/'verification-spec.json'),'completion_contract_ref':ref(DESIGN/'completion-contract.json'),'oracle_ref':ref(DESIGN/'oracle-cases.json'),'evidence_inputs_ref':ref(DESIGN/'evidence-inputs.json'),'accepted_return_ref':ref(ROOT/'.aegis/results/G5-UR0b/resume-r3/result.json'),'accepted_review_ref':ref(ROOT/'.aegis/results/G5-UR0b/control-review-resume-r3-terminal/result.json'),'closed_window_ref':ref(ROOT/'.aegis/results/G5-UR0b/resume-r3/window.json'),'no_prior_authority_reopened':True})
write(PACKAGE/'verification.lock.json',{'schema_version':1,'status':'CURRENT','verification_spec_ref':ref(DESIGN/'verification-spec.json'),'acceptance_oracle_ref':ref(DESIGN/'oracle-cases.json'),'immutable_input_ref':ref(DESIGN/'evidence-inputs.json'),'obligations':spec['obligations'],'quality_evidence_gate':spec['quality_evidence_gate'],'oracle_precondition':load(DESIGN/'oracle-cases.json')['red_precondition']})
write(PACKAGE/'execution-contract.json',{'schema_version':1,'EXECUTION_CLOSURE_CONTRACT':{**order['closure'],'authorized_source_paths':order['source_scope'],'read_only_frozen_paths':order['read_only_frozen_paths'],'required_changes':order['required_changes'],'required_tests':order['required_tests'],'authorized_control_paths':order['new_control_paths'],'authorized_outputs':order['output_paths'],'forbidden_changes':order['forbidden'],'native_reuse':order['native_reuse'],'new_sampling_workers_max':0,'prior_sampling_debit_seconds':1133,'remaining_time_is_authorization':False,'commit_push':False,'notify_terminal':order['notify_terminal']}})
(PACKAGE/'implementation-context.md').write_text('''# Offline initialization completion repair\n\nUse the exact Current completion-contract v2 and work order. Preserve f651523a and accepted dade34 dirty runner changes. New design/package files legitimately change the complete source hash; no exclusion change.\n\nOwner/data flow: immutable raw + worker init.state/source + run-input hashes -> offline native decoder -> initialization basis helper -> complete v2 population proof -> committed-only metrics -> Python repair/resume admission. No Runtime, public SDK, collector, harness, metric schema, artifact or browser change.\n\nFirst run the actual frozen RT60 old compiler and semantic oracle: complete known init+committed observations require2=1+1+0 and exactly1mutation receipt. Old MISSING must be the RED cause, not missing imports/new fields. Then implement the16oracle groups and recompile15inputs to new output. Preserve external/common statistics, committed receipts/attempts, snapshots/terminal/product outcomes. Initialization uses init.state.target.runtimeEpoch as document epoch, not lastCommit.runtimeEpoch.\n\nReturn exact source/compiler/spec/recorded-input/artifact identities and eligibility-pending-review cohort reuse assessment. Old results and window remain CLOSED and byte-identical. No new sampling, builds/probes, commit/push or UR0c/UR1. Continue until full offline closure or an explicit blocker, then notify the designated control chat once.\n''')
write(PACKAGE/'evidence-contract.json',{'schema_version':1,'work_order_ref':ref(DESIGN/'implementation-request.json'),'materialization':'Shared controlled worktree content-addressed bundle with result/manifest SHA256; no commit/push','result_root':'.aegis/results/G5-UR0b/init-repair-r1/','evidence_root':'verification/evidence/performance/g5-unified-render-ur0b/<new-source-tree-sha>/init-repair-r1/','required':order['closure']['all_of'],'identity_policy':'Separate recording source/artifact identity from current compiler/spec/source tree; include exact immutable-input hashes, source delta and all retained old manifests. OFFLINE_REVERIFICATION cannot qualify as fresh WORKER_COMPILE.','reuse_policy':'Emit eligibility pending independent control acceptance; preserve six-pair samples without rerun; no automatic sampling authorization.','required_terminal_notice':order['notify_terminal']})
package={'schema_version':1,'task_id':'G5-UR0b-init-repair-r1','status':'AUTHORIZED_OFFLINE_IMPLEMENTATION','execution_authority_mode':'repo_materialized','repository':order['repository'],'worktree':order['worktree'],'execution_ref':order['execution_ref'],'task_anchor':order['task_anchor'],'resume_cursor':order['resume_cursor'],'authorization':{'work_order_ref':ref(DESIGN/'implementation-request.json'),'new_sampling_workers_max':0,'build_probe':False,'commit_push':False},'continue_until_terminal_state':True,'return_surface':'CONTROL_REVIEW','stop_after':'OFFLINE_INIT_REPAIR'}
for key,name in [('authority_lock_ref','authority.lock.json'),('verification_lock_ref','verification.lock.json'),('execution_contract_ref','execution-contract.json'),('implementation_context_ref','implementation-context.md'),('evidence_contract_ref','evidence-contract.json')]:package[key]={'path':name,'sha256':sha(PACKAGE/name)}
write(PACKAGE/'package.json',package)
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 p=(PACKAGE/package[key]['path']).resolve();assert p.is_relative_to(PACKAGE) and sha(p)==package[key]['sha256']
preflight={'status':'REPOSITORY_PACKAGE_AND_EVIDENCE_PREFLIGHT_VERIFIED','package_ref':ref(PACKAGE/'package.json'),'starting_revision':current['revision'],'accepted_dirty_tree':old['source_tree_sha256'],'prepackage_source_tree_sha256':current['source_tree_sha256'],'design_additions':sorted(changed),'cursor_class':'EXACT_REVISION_WITH_ACCEPTED_DIRTY_AND_CURRENT_DESIGN','authority_conflict':False,'verification_conflict':False,'scope_conflict':False,'unresolved_semantic_decisions':[],'old_bundle_files_hash_verified':len(rm['files']),'new_browser_workers':0,'build_probe_started':False,'public_api_impact':'NONE'}
write(RESULT/'preflight.json',preflight);write(RESULT/'starting-source-identity.json',source_identity.identity(ROOT));print(json.dumps(preflight))
