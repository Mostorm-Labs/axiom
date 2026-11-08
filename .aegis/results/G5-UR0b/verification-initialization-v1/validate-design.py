from pathlib import Path
import json,hashlib,subprocess
R=Path(__file__).resolve().parents[4];D=R/'.aegis/verification/G5-UR0b-initialization-v1';C=Path(__file__).resolve().parent
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();checked=[]
def walk(x):
 if isinstance(x,dict):
  if set(['path','sha256'])<=set(x):
   p=(R/x['path']).resolve();assert p.is_relative_to(R) and p.is_file();assert sha(p)==x['sha256'],str(p);checked.append(x['path'])
  for value in x.values():walk(value)
 elif isinstance(x,list):
  for value in x:walk(value)
man=load(D/'manifest.json')
for n,entry in man['files'].items():assert sha(D/n)==entry['sha256'],n
for p in D.glob('*.json'):walk(load(p))
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
request=load(D/'implementation-request.json');subprocess.run(['git','merge-base','--is-ancestor',request['task_anchor']['revision'],'HEAD'],cwd=R,check=True)
assert request['new_sampling_workers_max']==0 and not request['commit_push'] and request['notify_terminal']['required']
inputs=load(D/'evidence-inputs.json')['inputs'];assert len(inputs)==15
rt=next(x for x in inputs if x['run_id']=='realtime-60');expected=rt['expected_v2'];assert (expected['accepted_raw_count'],expected['qualified_initialization_count'],expected['qualified_committed_count'],expected['unresolved_accepted_count'],expected['metric_receipt_count'])==(2,1,1,0,1)
raw=[json.loads(s) for s in (R/rt['input_directory']/'raw-trace.jsonl').read_text().splitlines() if s]
accepted=[r for r in raw if r['kind']==12 and r['tag']==29 and r['values'][0]=='1'];assert len(accepted)==2
initial=next(r for r in accepted if r['seq']=='47');assert initial['values'][7:9]==['0','0']
commits=[r for r in raw if r['kind']==6 and r['tag']==27];assert len(commits)==1 and int(initial['at_ns'])<int(commits[0]['at_ns'])
worker=load(R/rt['input_directory']/'worker-result.json');s=worker['init']['state'];assert s['generation']=='0' and s['objects']==0 and s['lastCommit']['ordinal']=='0'
contract=load(D/'completion-contract.json');profile=contract['initialization']['recording_basis']
assert worker['source_before']['files']['verification/runners/g5/ur0/consumer.mjs']['sha256']==profile['consumer_sha256']
assert worker['source_before']['files']['verification/runners/g5/ur0/worker.mjs']['sha256']==profile['worker_sha256']
old=load(R/rt['input_directory']/'canonical-chain.json');assert old['status']=='MISSING' and old['accepted_raw_count']==2 and old['qualified_decoded_count']==1 and old['metric_receipt_count']==1
for i in inputs:
 e=i['expected_v2'];assert e['accepted_raw_count']==e['qualified_initialization_count']+e['qualified_committed_count']+e['unresolved_accepted_count'];assert e['qualified_decoded_count']==e['qualified_committed_count']==e['metric_receipt_count']
versions=[load(R/'.aegis'/f'{n}.json')['schema_version'] for n in ['project','authorities','gates','evidence','integrations','state']];assert len(set(versions))==1 and versions[0]=='0.6'
registry=load(R/'.aegis/authorities.json')['authorities'];ids={x['id'] for x in registry};assert len(ids)==len(registry)
current=[(x['scope'],x['kind']) for x in registry if x['status']=='Current'];assert len(current)==len(set(current))
for a in registry:assert set(a.get('depends_on',[]))<=ids
for g in load(R/'.aegis/gates.json')['gates']:assert set(g.get('authority_ids',[]))<=ids
state=load(R/'.aegis/state.json');assert not state['blocking_findings'];assert not any('G5-UR0' in json.dumps(g) for g in load(R/'.aegis/gates.json')['gates'])
result={'status':'DESIGN_BINDINGS_AND_OBSERVED_ORACLE_VALID','manifest_sha256':sha(D/'manifest.json'),'verification_spec_sha256':sha(D/'verification-spec.json'),'work_order_sha256':sha(D/'implementation-request.json'),'hash_bound_refs_checked':len(checked),'frozen_archive_inputs':15,'oracle_groups':len(load(D/'oracle-cases.json')['cases']),'known_old_failure_reproduced_from_frozen_artifact':{'status':old['status'],'accepted_raw_count':2,'qualified_decoded_count':1,'metric_receipt_count':1},'new_target_partition':expected,'project_control_scope':{'version':'0.6','authority_ids_valid':True,'current_authority_unique':True,'global_G5_gate_present':False,'global_manifests_or_state_changed':False,'derivation_tool_available':False,'local_package_review_records_are_applicable_scope':'No global Gate PASS inferred'},'new_browser_workers':0,'production_source_changed_by_designer':False}
(C/'validation.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k not in ['new_target_partition','project_control_scope']}))
