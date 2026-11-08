"""Seventeen immutable recordings, new output only; never sample or build."""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'))
import repair_runner as reader
import source_identity
load=lambda p:json.loads(p.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
initial=load(ROOT/'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json')
accepted=ROOT/'verification/evidence/performance/g5-unified-render-ur0b/2ca47a17ae94034a574379e32ae118b95b5f7c35fee9706be91dc21c46477ebb/init-repair-r1/offline-execution/reverification-final'
rows=[{'id':v['run_id'],'source':ROOT/v['input_directory'],'baseline':accepted/v['run_id'],'immutable':v['immutable_inputs'],'expected':v['expected_v2']} for v in initial['inputs']]
rt=ROOT/'verification/evidence/performance/g5-unified-render-ur0b/d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a/resume-r4/execution-and-checks/execution/raw-runs'
refs=load(ROOT/'.aegis/verification/G5-UR0b-correctness-harness-v1/evidence-inputs.json')['refs']
for hz in [120,240]:
 source=rt/f'realtime-{hz}';rows.append({'id':f'realtime-{hz}','source':source,'baseline':source,'immutable':[v for v in refs if v['path'].startswith(str(source.relative_to(ROOT))+'/')],'expected':None})
assert len(rows)==17
destination=ROOT/'out/ur0b/correctness-repair-r2/recompiled';destination.mkdir(parents=True,exist_ok=False)
identity=source_identity.identity(ROOT);results=[]
for row in rows:
 for binding in row['immutable']:assert sha(ROOT/binding['path'])==binding['sha256'],binding['path']
 source=row['source'];baseline=row['baseline'];target=destination/row['id'];target.mkdir()
 with (target/'compiler.log').open('w') as log:subprocess.run(['node','verification/runners/g5/ur0/compile_run.mjs',str(source),str(target)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
 before=load(baseline/'compiled-summary.json');after=load(target/'compiled-summary.json')
 assert {k:v for k,v in after.items() if k!='analysis'}=={k:v for k,v in before.items() if k!='analysis'},(row['id'],'summary/metrics/native populations changed')
 for name in ['canonical-chain.json','decoded.json','metrics.json','external-metrics.json']:
  assert (target/name).read_bytes()==(baseline/name).read_bytes(),(row['id'],'changed artifact bytes',name)
 assert after['analysis']['mode']=='OFFLINE_REVERIFICATION'
 a={k:v for k,v in after['analysis'].items() if k not in ['mode','compiler_files']};b={k:v for k,v in before['analysis'].items() if k not in ['mode','compiler_files']};assert a==b,(row['id'],'recorded provenance changed')
 for name,digest in after['analysis']['compiler_files'].items():assert sha(ROOT/'verification/runners/g5/ur0'/name)==digest
 proof=after['canonical_chain'];reader.require_canonical_chain(after,require_positive=proof['qualified_committed_count']>0)
 try:reader.require_bound_compilation(target,after,{'source_tree_sha256':identity['source_tree_sha256']})
 except RuntimeError as error:assert 'offline' in str(error)
 else:raise AssertionError('offline recompile admitted as fresh')
 if row['expected']:
  for key,value in row['expected'].items():assert (after['schema_status'] if key=='metrics_schema_status' else proof[key])==value
 run=load(source/'worker-result.json');assert sha(source/run['final']['snapshot']['path'])==run['final']['snapshot']['sha256']
 for binding in row['immutable']:assert sha(ROOT/binding['path'])==binding['sha256']
 result={'run_id':row['id'],'input_refs':row['immutable'],'baseline_summary_ref':ref(baseline/'compiled-summary.json'),'derived_summary_ref':ref(target/'compiled-summary.json'),'canonical_chain_ref':ref(target/'canonical-chain.json'),'recording_source_sha256':after['analysis']['input_source_tree_sha256'],'mode':'OFFLINE_REVERIFICATION','byte_identical':['canonical-chain.json','decoded.json','metrics.json','external-metrics.json'],'all_summary_fields_except_explicit_compiler_provenance_identical':True,'snapshot_terminal_artifact_and_source_identity_unchanged':True,'offline_promotion_rejected':True,'counts':{k:proof[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','metric_receipt_count','unresolved_accepted_count']}}
 results.append(result);print(json.dumps({'run_id':row['id'],'status':'INVARIANT',**result['counts']}),flush=True)
assert source_identity.identity(ROOT)['source_tree_sha256']==identity['source_tree_sha256']
write(RESULT/'reverification.json',{'status':'ALL_17_IMMUTABLE_RECORDINGS_INVARIANT','rows':results,'source_sha256':identity['source_tree_sha256'],'new_sampling_workers':0,'new_builds':0,'native_oracle_invocations':0})
print('ALL_17_IMMUTABLE_RECORDINGS_INVARIANT',flush=True)
