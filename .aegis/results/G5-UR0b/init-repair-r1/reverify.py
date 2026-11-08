"""Only recompile the fifteen immutable archives; no workers or artifact builds."""
import copy,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b/init-repair-r1'
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import repair_runner as admission
import source_identity
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
frozen=load(ROOT/'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json')
assert len(frozen['inputs'])==15
window_path=ROOT/'.aegis/results/G5-UR0b/resume-r3/window.json';window_sha=sha(window_path);window=load(window_path);assert window['status']=='CLOSED'
current_source=source_identity.identity(ROOT)
destination=OUT/'reverification-final';destination.mkdir(exist_ok=False)
results=[];paired_rows=[]
for entry in frozen['inputs']:
 for immutable in entry['immutable_inputs']:assert sha(ROOT/immutable['path'])==immutable['sha256'],immutable['path']
 source=ROOT/entry['input_directory'];target=destination/entry['run_id'];target.mkdir()
 with (target/'compiler.log').open('w') as log:subprocess.run(['node','verification/runners/g5/ur0/compile_run.mjs',str(source),str(target)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
 old=load(source/'compiled-summary.json');new=load(target/'compiled-summary.json');p=new['canonical_chain'];expected=entry['expected_v2']
 for key,value in expected.items():assert (new['schema_status'] if key=='metrics_schema_status' else p.get(key))==value,(entry['run_id'],key,p.get(key),value)
 assert p['schema']=='g5-ur0b-canonical-chain-proof-v2' and new['analysis']['mode']=='OFFLINE_REVERIFICATION'
 assert p['accepted_raw_count']==p['qualified_initialization_count']+p['qualified_committed_count']+p['unresolved_accepted_count']
 assert len(p['accepted_records'])==len({r['completion_seq'] for r in p['accepted_records']})==p['accepted_raw_count']
 for key in ['common','critical_loss','statistical_loss','identity_errors','clock','terminal','final_snapshot_sha256','product_errors','artifact_identity','source_stable','artifacts_stable']:
  assert new[key]==old[key],(entry['run_id'],'changed common/outcome',key)
 for name in ['metrics.json','external-metrics.json']:
  assert (target/name).read_bytes()==(source/name).read_bytes(),(entry['run_id'],'changed actual metrics bytes',name)
 old_decoded=load(source/'decoded.json');new_decoded=load(target/'decoded.json')
 for key in ['commits','scenes','frames','qualified_completions','render_frames','provider_submissions','owner_bindings','decisions','leaves','spans','gpu','counters','errors','loss']:
  assert new_decoded[key]==old_decoded[key],(entry['run_id'],'changed native committed/attempt observations',key)
 assert p['chains']==old['canonical_chain']['chains'],(entry['run_id'],'changed committed chain/provenance')
 assert new['actual_observations']['qualified_completions']==old['actual_observations']['qualified_completions']
 admission.require_canonical_chain(new,require_positive=p['qualified_committed_count']>0)
 try:admission.require_bound_compilation(target,new,{'source_tree_sha256':current_source['source_tree_sha256']})
 except RuntimeError as error:
  assert 'offline' in str(error);promotion_rejected=True
 else:raise AssertionError('offline compiler output admitted as fresh sampling')
 assert new['analysis']['input_source_tree_sha256']==frozen['recording_source_tree_sha256']
 for immutable,key in zip(entry['immutable_inputs'],['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):assert new['analysis'][key]==immutable['sha256']
 for name,value in new['analysis']['compiler_files'].items():assert sha(ROOT/'verification/runners/g5/ur0'/name)==value
 run=load(source/'worker-result.json');assert run['status']=='OBSERVED' and run['source_stable'] and run['artifacts_stable']
 assert sha(source/run['final']['snapshot']['path'])==run['final']['snapshot']['sha256']
 counts={key:p[key] for key in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','qualified_decoded_count','metric_receipt_count','unresolved_accepted_count']}
 results.append({'run_id':entry['run_id'],'recorded_input_refs':entry['immutable_inputs'],'derived_summary_ref':ref(target/'compiled-summary.json'),'proof_ref':ref(target/'canonical-chain.json'),'counts':counts,'status':p['status'],'schema_status':new['schema_status'],'original_common_external_and_metrics_bytes_unchanged':True,'committed_chains_receipts_and_attempts_unchanged':True,'snapshot_terminal_product_outcomes_unchanged':True,'offline_promotion_rejected':promotion_rejected,'recording_source_tree_sha256':new['analysis']['input_source_tree_sha256'],'compiler_files':new['analysis']['compiler_files']})
 original=next(r for r in window['rows'] if r['run_id']==entry['run_id']);row=copy.deepcopy(original);row['outcome']['summary']=new;paired_rows.append(row)
 print(json.dumps({'run_id':entry['run_id'],'status':p['status'],**counts}),flush=True)
for entry in frozen['inputs']:
 for immutable in entry['immutable_inputs']:assert sha(ROOT/immutable['path'])==immutable['sha256']
assert sha(window_path)==window_sha and load(window_path)['status']=='CLOSED'
assert source_identity.identity(ROOT)['source_tree_sha256']==current_source['source_tree_sha256']
pairs=admission.paired(paired_rows,'qualification');write(OUT/'recompiled-overhead-pairs.json',pairs)
old_pairs=load(ROOT/'out/ur0b/resume-r3/execution/qualification-pairs.json');assert pairs==old_pairs
script="import{qualifyOverhead}from'./verification/runners/g5/ur0/fixture_plan.mjs';import fs from'node:fs';console.log(JSON.stringify(qualifyOverhead(JSON.parse(fs.readFileSync(process.argv[1])))))"
overhead=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(OUT/'recompiled-overhead-pairs.json')],cwd=ROOT,text=True))
assert overhead==load(ROOT/'out/ur0b/resume-r3/execution/overhead-qualification.json') and overhead['status']=='QUALIFIED'
write(OUT/'recompiled-overhead-qualification.json',overhead)
assessment={'status':'ELIGIBLE_PENDING_INDEPENDENT_CONTROL_REVIEW','scope':'Existing r3 diagnostic and six-pair overhead observations only; not a fresh cohort or continuation authorization','current_compiler_source_tree_sha256':current_source['source_tree_sha256'],'recording_source_tree_sha256':frozen['recording_source_tree_sha256'],'invariance':{'all_fifteen_input_bindings_unchanged':True,'all_actual_metrics_and_external_bytes_unchanged':True,'all_committed_receipts_and_attempts_unchanged':True,'snapshot_terminal_product_outcomes_unchanged':True,'observer_workload_artifact_collector_consumer_unchanged':'source-scope/product projection is verified at materialization','original_six_pair_values_unchanged':pairs==old_pairs,'overhead_verdict_unchanged':True},'overhead_ref':ref(OUT/'recompiled-overhead-qualification.json'),'prior_sampling_debit_seconds':1133,'new_browser_workers':0,'new_sampling':False,'old_window_status':'CLOSED','old_window_sha256':window_sha,'performance_pass':False,'official_gate_pass':False,'independent_acceptance_required':True,'eligible_samples_not_adopted_as_fresh':True}
write(RESULT/'reuse-assessment.json',assessment)
write(RESULT/'offline-reverification.json',{'status':'ALL_FIFTEEN_IMMUTABLE_INPUTS_RECONCILED','count':len(results),'initialization_total':sum(r['counts']['qualified_initialization_count'] for r in results),'committed_total':sum(r['counts']['qualified_committed_count'] for r in results),'raw_accepted_total':sum(r['counts']['accepted_raw_count'] for r in results),'unresolved_total':sum(r['counts']['unresolved_accepted_count'] for r in results),'rows':results,'new_browser_workers':0,'new_artifact_builds':0,'offline_only':True,'source_tree_sha256':current_source['source_tree_sha256'],'reuse_assessment_ref':ref(RESULT/'reuse-assessment.json')})
print(json.dumps({'status':'ALL_FIFTEEN_IMMUTABLE_INPUTS_RECONCILED','inputs':len(results),'reuse':assessment['status'],'new_sampling':False}))
