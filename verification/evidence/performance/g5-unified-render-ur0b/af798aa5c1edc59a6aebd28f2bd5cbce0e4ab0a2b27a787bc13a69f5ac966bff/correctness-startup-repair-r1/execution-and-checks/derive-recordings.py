"""Offline invariance only; original recordings and failed r6 remain untouched."""
import copy,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'verification/runners/g5/ur0'));import repair_runner
load=lambda p:json.loads(p.read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
accept=ROOT/'.aegis/results/G5-UR0b/control-review-trace-lifecycle-r1/reuse-acceptance.json';assert sha(accept)=='f76c8b5249efc4b1e3a908e2ee19cc94461cd5592f9856518b9073c289404135';packet=load(accept)
entries=[{'id':r['run_id'],'inputs':r['recorded_input_refs'],'baseline':r['summary_ref'],'kind':'METRICS_PREREQUISITE'} for r in packet['records']]
c=packet['recorded_correctness_cases'][0];entries.append({'id':'recorded-lower-z','inputs':c['input_refs'],'baseline':c['summary_ref'],'kind':'NATIVE_ONLY_CORRECTNESS'})
r6=load(ROOT/'.aegis/results/G5-UR0b/resume-r6/result.json');original=Path(r6['compiled_summary_ref']['path']).parent
entries.append({'id':'actual-r6-preserved-missing','inputs':[ref(ROOT/original/n) for n in ['raw-trace.jsonl','worker-result.json','run-inputs.json']],'baseline':r6['compiled_summary_ref'],'kind':'PRESERVED_MISSING'})
rows=[]
for entry in entries:
 for item in entry['inputs']:
  assert sha(ROOT/item['path'])==item['sha256']
 raw=(ROOT/entry['inputs'][0]['path']).parent;out=D/'recompiled-recordings'/entry['id'];assert not out.exists()
 with (D/(entry['id']+'-compile.log')).open('w') as log:subprocess.run(['node',str(ROOT/'verification/runners/g5/ur0/compile_run.mjs'),str(raw),str(out)],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
 new=load(out/'compiled-summary.json');baseline=ROOT/entry['baseline']['path'];assert sha(baseline)==entry['baseline']['sha256'];old=load(baseline)
 a=copy.deepcopy(old);b=copy.deepcopy(new)
 a['analysis'].pop('compiler_files');b['analysis'].pop('compiler_files');a['analysis'].pop('mode');b['analysis'].pop('mode');assert a==b,entry['id']
 assert new['analysis']['mode']=='OFFLINE_REVERIFICATION';checked={}
 for n in ['decoded.json','canonical-chain.json','metrics.json','external-metrics.json','compiler-provenance.json']:
  previous=baseline.parent/n;current=out/n;assert previous.exists()==current.exists(),(entry['id'],n)
  if current.exists():assert sha(previous)==sha(current),(entry['id'],n);checked[n]=sha(current)
 for item in entry['inputs']:assert sha(ROOT/item['path'])==item['sha256']
 p=new['canonical_chain']
 if entry['kind']=='PRESERVED_MISSING':
  assert (p['accepted_raw_count'],p['qualified_initialization_count'],p['qualified_committed_count'],p['unresolved_accepted_count'])==(7,1,5,1)
  assert p['status']=='MISSING' and p['accepted_records'][0]['completion_seq']=='22' and p['accepted_records'][0]['classification']=='UNRESOLVED'
  try:repair_runner.require_canonical_chain(new,require_metrics=False)
  except RuntimeError:pass
  else:raise AssertionError('r6 missing native work was improperly qualified')
 else:repair_runner.require_canonical_chain(new,require_metrics=entry['kind']=='METRICS_PREREQUISITE')
 rows.append({'record_id':entry['id'],'kind':entry['kind'],'status':'PRESERVED_MISSING' if entry['kind']=='PRESERVED_MISSING' else 'INVARIANT_EXCEPT_COMPILER_PROVENANCE','original_input_refs':entry['inputs'],'original_mode':old['analysis']['mode'],'derived_mode':new['analysis']['mode'],'recording_source_tree_sha256':new['analysis']['input_source_tree_sha256'],'baseline_summary_ref':entry['baseline'],'derived_summary_ref':ref(out/'compiled-summary.json'),'derived_proof_ref':ref(out/'canonical-chain.json'),'unchanged_file_sha256':checked,'counts':{k:p[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','metric_receipt_count','unresolved_accepted_count']}})
 print(json.dumps({'id':entry['id'],'status':rows[-1]['status'],'completed':len(rows)}),flush=True)
write(D/'reverification.json',{'status':'17_PLUS_ONE_INVARIANT_AND_R6_STILL_MISSING','rows':rows,'new_browser_starts':0,'new_builds':0,'old_windows':'CLOSED_UNCHANGED','historical_starts':35,'historical_seconds':1276,'new_profile_sampling_qualification':'NOT_PERFORMED_OR_AUTHORIZED'})
