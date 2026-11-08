"""Independent read-only archive replay and reader checks. No browser/build."""
from pathlib import Path
import json,hashlib,subprocess,sys,copy
R=Path(__file__).resolve().parents[4];C=Path(__file__).resolve().parent;D=R/'.aegis/results/G5-UR0b/init-repair-r1'
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
frozen=load(R/'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json');producer=load(D/'offline-reverification.json');byrun={r['run_id']:r for r in producer['rows']}
sys.path.insert(0,str(R/'verification/runners/g5/ur0'));import repair_runner as a
rows=[]
for entry in frozen['inputs']:
 source=R/entry['input_directory'];target=C/'recompiled'/entry['run_id'];target.mkdir(parents=True,exist_ok=True)
 for r in entry['immutable_inputs']:assert sha(R/r['path'])==r['sha256']
 with (target/'compiler.log').open('w') as log:subprocess.run(['node','verification/runners/g5/ur0/compile_run.mjs',str(source),str(target)],cwd=R,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
 summary=load(target/'compiled-summary.json');proof=summary['canonical_chain'];old=load(source/'compiled-summary.json');raw=[json.loads(x) for x in (source/'raw-trace.jsonl').read_text().splitlines() if x];actual=[r for r in raw if r['kind']==12 and r['tag']==29 and r['values'][0]=='1']
 expected=entry['expected_v2']
 for k,v in expected.items():assert (summary['schema_status'] if k=='metrics_schema_status' else proof[k])==v,(entry['run_id'],k)
 assert proof['schema']=='g5-ur0b-canonical-chain-proof-v2';assert len(actual)==proof['accepted_raw_count']
 assert {x['seq'] for x in actual}=={x['completion_seq'] for x in proof['accepted_records']};assert len(actual)==len({x['seq'] for x in actual})
 assert proof['initialization_chains']+proof['chains'] and actual or not actual
 for filename in ['metrics.json','external-metrics.json']:assert (target/filename).read_bytes()==(source/filename).read_bytes(),(entry['run_id'],filename)
 for key in ['common','critical_loss','statistical_loss','identity_errors','clock','terminal','final_snapshot_sha256','product_errors','artifact_identity','source_stable','artifacts_stable']:assert summary[key]==old[key],(entry['run_id'],key)
 assert proof['chains']==old['canonical_chain']['chains']
 for k in ['commits','scenes','frames','qualified_completions','render_frames','provider_submissions','owner_bindings','decisions','leaves','spans','gpu','counters','errors','loss']:assert load(target/'decoded.json')[k]==load(source/'decoded.json')[k],(entry['run_id'],k)
 observed=load(R/byrun[entry['run_id']]['derived_summary_ref']['path'])
 for k in ['canonical_chain','analysis','schema_status','common','initialization_lifecycle']:assert observed[k]==summary[k],(entry['run_id'],k)
 assert summary['analysis']['mode']=='OFFLINE_REVERIFICATION'
 a.require_canonical_chain(summary,require_positive=proof['qualified_committed_count']>0)
 try:a.require_bound_compilation(target,summary,{'source_tree_sha256':'2ca47a17ae94034a574379e32ae118b95b5f7c35fee9706be91dc21c46477ebb'})
 except RuntimeError as e:assert 'offline' in str(e)
 else:raise AssertionError('offline admitted as fresh')
 rows.append({'run_id':entry['run_id'],'accepted_raw_count':len(actual),'initialization':proof['qualified_initialization_count'],'committed':proof['qualified_committed_count'],'unresolved':proof['unresolved_accepted_count'],'mutation_receipts':proof['metric_receipt_count'],'status':proof['status'],'metrics_bytes_equal':True,'producer_output_equal':True})
rt=C/'recompiled/realtime-60';s=load(rt/'compiled-summary.json');p=s['canonical_chain'];initial=p['initialization_chains'][0];original=R/next(x for x in frozen['inputs'] if x['run_id']=='realtime-60')['input_directory'];run=load(original/'worker-result.json');raw=[json.loads(x) for x in (original/'raw-trace.jsonl').read_text().splitlines() if x]
record=lambda seq:next(r for r in raw if r['seq']==seq)
assert (initial['draw_seq'],initial['provider_seq'],initial['capture_seq'],initial['completion_seq'])==('15','32','42','47');assert initial['basis']['operation_id'] is None
public=initial['initialization_basis']['public_state'];assert public['documentId']==run['init']['state']['documentId'];assert public['target']=={k:run['init']['state']['target'][k] for k in ['runtimeEpoch','attachmentEpoch','viewId']}
assert public['target']['runtimeEpoch']==initial['basis']['document_epoch']=='1';assert run['init']['state']['lastCommit']['runtimeEpoch']=='0'
assert initial['draw_job']==record('15')['job']==record('32')['job'];assert int(record('47')['at_ns'])<min(int(r['at_ns']) for r in raw if r['kind']==6 and r['tag']==27)
# Actual v2 reader: initialization-only valid populations still cannot meet positive committed admission.
i=copy.deepcopy(s);q=i['canonical_chain'];q.update(accepted_raw_count=1,qualified_committed_count=0,qualified_decoded_count=0,metric_receipt_count=0,chains=[],accepted_records=[m for m in q['accepted_records'] if m['classification']=='INITIALIZATION'])
a.require_canonical_chain(i)
try:a.require_canonical_chain(i,require_positive=True)
except RuntimeError as e:assert 'committed' in str(e)
else:raise AssertionError('initialization met positive committed requirement')
# Reproduce known old v1 behavior from its immutable source revision, without reverting this worktree.
legacy=C/'legacy-canonical.mjs';oldcode=subprocess.check_output(['git','show','f651523a:verification/runners/g5/ur0/canonical_chain.mjs'],cwd=R);assert hashlib.sha256(oldcode).hexdigest()=='756920ea04c282324dc80ff75f8bdbe78990d23c5fb95526c7d5bf88f9ed4e32';legacy.write_bytes(oldcode)
script="import fs from 'node:fs';import{canonicalChainProof}from'./.aegis/results/G5-UR0b/control-review-init-repair-r1/legacy-canonical.mjs';const d=process.argv[1],r=(n)=>JSON.parse(fs.readFileSync(d+'/'+n));const raw=fs.readFileSync(d+'/raw-trace.jsonl','utf8').trim().split('\\n').map(JSON.parse);console.log(JSON.stringify(canonicalChainProof(raw,r('decoded.json'),r('metrics.json'))));"
legacyproof=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(original)],cwd=R,text=True));assert legacyproof['status']=='MISSING' and legacyproof['accepted_raw_count']==2 and legacyproof['qualified_decoded_count']==1 and legacyproof['metric_receipt_count']==1
result={'status':'INDEPENDENT_OFFLINE_REPLAY_AND_ISOLATION_VERIFIED','rows':rows,'totals':{k:sum(x[k] for x in rows) for k in ['accepted_raw_count','initialization','committed','unresolved','mutation_receipts']},'all_metrics_external_bytes_unchanged':True,'all_committed_native_chains_unchanged':True,'all_offline_promotions_rejected':True,'initialization_only_positive_gate':'REJECTED','legacy_actual_behavior':'MISSING_raw2_decoded1_receipt1','new_actual_behavior':'QUALIFIED_raw2_init1_committed1_unresolved0_receipt1','public_document_epoch_is_target_not_lastCommit':True,'new_browser_workers':0}
(C/'offline-check.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='rows'}))
