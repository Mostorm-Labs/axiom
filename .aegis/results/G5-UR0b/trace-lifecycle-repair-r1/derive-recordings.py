"""Recompile only the frozen18 recordings to fresh offline directories."""
import copy,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import repair_runner
load=lambda p:json.loads(p.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
inputs=load(ROOT/'.aegis/verification/G5-UR0b-trace-lifecycle-v1/evidence-inputs.json')
acceptance=load(ROOT/inputs['old17_reuse_ref']['path']);assert len(acceptance['records'])==17
def compile_record(name,refs):
 for r in refs:assert sha(ROOT/r['path'])==r['sha256']
 raw=next(ROOT/r['path'] for r in refs if Path(r['path']).name=='raw-trace.jsonl').parent
 output=D/'recompiled-recordings'/name
 if not output.exists():
  with (D/(name+'-compile.log')).open('w') as log:subprocess.run(['node',str(ROOT/'verification/runners/g5/ur0/compile_run.mjs'),str(raw),str(output)],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
 else:assert (output/'compiled-summary.json').is_file(),str(output)
 summary=load(output/'compiled-summary.json');assert summary['analysis']['mode']=='OFFLINE_REVERIFICATION'
 for compiler,digest in summary['analysis']['compiler_files'].items():assert sha(ROOT/'verification/runners/g5/ur0'/compiler)==digest
 for r,k in zip(refs,['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):assert summary['analysis'][k]==r['sha256']
 return raw,output,summary
rows=[]
for entry in acceptance['records']:
 raw,out,new=compile_record(entry['run_id'],entry['recorded_input_refs'])
 baseline=ROOT/entry['summary_ref']['path'];assert sha(baseline)==entry['summary_ref']['sha256'];old=load(baseline)
 before=copy.deepcopy(old);after=copy.deepcopy(new)
 before['analysis'].pop('compiler_files');after['analysis'].pop('compiler_files');assert before==after,entry['run_id']
 checked={}
 for name in ['decoded.json','canonical-chain.json','metrics.json','external-metrics.json','compiler-provenance.json']:
  a=baseline.parent/name;b=out/name;assert a.exists()==b.exists(),(entry['run_id'],name)
  if a.exists():assert sha(a)==sha(b),(entry['run_id'],name);checked[name]=sha(b)
 assert new['artifact_identity']==entry['accepted_artifacts']
 assert new['analysis']['input_source_tree_sha256']==entry['recording_source_tree_sha256']
 repair_runner.require_canonical_chain(new,require_metrics=True)
 for k,v in entry['counts'].items():assert new['canonical_chain'][k]==v,(entry['run_id'],k)
 rows.append({'run_id':entry['run_id'],'result':'INVARIANT_EXCEPT_COMPILER_PROVENANCE','recording_source_tree_sha256':entry['recording_source_tree_sha256'],'mode':new['analysis']['mode'],'original_input_refs':entry['recorded_input_refs'],'baseline_summary_ref':entry['summary_ref'],'derived_summary_ref':ref(out/'compiled-summary.json'),'derived_proof_ref':ref(out/'canonical-chain.json'),'unchanged_file_sha256':checked,'counts':entry['counts']})
 print(json.dumps({'record':entry['run_id'],'status':'INVARIANT','completed':len(rows)}),flush=True)
raw,out,new=compile_record('actual-r5-lower-z',inputs['r5_worker_inputs'])
old=load(raw/'compiled-summary.json');p=new['canonical_chain'];assert (p['accepted_raw_count'],p['qualified_initialization_count'],p['qualified_committed_count'],p['unresolved_accepted_count'])==(6,1,5,0)
assert p['scope']=='NATIVE_ONLY' and p['metric_receipt_count'] is None and new['schema_status']=='MISSING'
assert not (out/'metrics.json').exists() and not (out/'external-metrics.json').exists()
assert p['chains']==old['canonical_chain']['chains']
for key in ['status','source_stable','artifacts_stable','critical_loss','statistical_loss','identity_errors','clock','terminal','final_snapshot_sha256','product_errors','artifact_identity','schema_status','schema_reason']:assert new[key]==old[key],key
repair_runner.require_canonical_chain(new,require_metrics=False)
worker=load(raw/'worker-result.json');assert worker['correctness_status']=='QUALIFIED'
assert len(worker['correctness'])==4 and all(e['comparison']['status']=='QUALIFIED' and e['comparison']['interior_mismatched_pixels']==0 for e in worker['correctness'])
assert sum(e.get('applicability',{}).get('status')=='QUALIFIED' for e in worker['correctness'] if e.get('applicability'))==2
rows.append({'run_id':'actual-r5-lower-z','result':'CORRECTED_NATIVE_INITIALIZATION_LIFECYCLE','mode':new['analysis']['mode'],'original_mode':old['analysis']['mode'],'recording_source_tree_sha256':new['analysis']['input_source_tree_sha256'],'original_input_refs':inputs['r5_worker_inputs'],'derived_summary_ref':ref(out/'compiled-summary.json'),'derived_proof_ref':ref(out/'canonical-chain.json'),'counts':{k:p[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','qualified_decoded_count','metric_receipt_count','unresolved_accepted_count']},'five_committed_chains_identical':True,'pixel_and_subject_results_unchanged':True,'trace_scope':p['initialization_chains'][0]['initialization_basis']['trace_scope'],'metrics_not_applicable':True})
for entry in rows:
 for r in entry['original_input_refs']:assert sha(ROOT/r['path'])==r['sha256']
assert sha(ROOT/inputs['original_r5_result_ref']['path'])==inputs['original_r5_result_ref']['sha256']
write(D/'reverification.json',{'status':'18_OFFLINE_DERIVATIONS_COMPLETE','rows':rows,'new_browser_workers':0,'new_builds':0,'original_recordings_mutated':False,'old_windows':'CLOSED_UNCHANGED','historical_starts':34,'historical_seconds':1264})
write(D/'reuse-assessment.json',{'status':'ELIGIBLE_PENDING_INDEPENDENT_CONTROL_REVIEW','actual_case':'correctness-lower_z_translucent_drag','actual_case_ref':rows[-1],'old17_invariant_records':[r['run_id'] for r in rows[:-1]],'producer_accepts_reuse':False,'new_sampling_workers':0,'old_r5_admission':'REJECTED_UNCHANGED','old_r5_window':'CLOSED_UNCHANGED','remaining_if_independently_accepted':'5 correctness plus30 main; no starts authorized by this package','performance_or_gate_PASS':False})
print(json.dumps({'status':'18_OFFLINE_DERIVATIONS_COMPLETE','actual_r5_counts':rows[-1]['counts']}),flush=True)
