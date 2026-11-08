"""Recompile immutable P36-r1 raw; never collect or rewrite a browser run."""
import hashlib,importlib.util,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent
BASE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b/590b1565f9f69b52ed3ca8cdd19f8ca798ded09c9025917cbc47ba90b501e4f2/p36-r1'
manifest=json.loads((BASE/'manifest.json').read_text())
assert hashlib.sha256((BASE/'manifest.json').read_bytes()).hexdigest()=='ec20501acf288828177293e9661c7fdbb4ed90c34f31233d3d022cd469754300'
spec=importlib.util.spec_from_file_location('repair',ROOT/'verification/runners/g5/ur0/repair_runner.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
results=[]
for name in ['diagnostic-OFF','diagnostic-ON','qualification-p1-A','qualification-p1-B','qualification-p2-B']:
 raw=BASE/'execution/raw-runs'/name;out=ROOT/'out/ur0b/p36-r2/reverification'/name;out.mkdir(parents=True,exist_ok=True)
 with (out/'compiler.log').open('w') as log:
  r=subprocess.run(['node',str(ROOT/'verification/runners/g5/ur0/compile_run.mjs'),str(raw),str(out)],stdout=log,stderr=subprocess.STDOUT,cwd=ROOT)
 assert r.returncode==0,name
 summary=json.loads((out/'compiled-summary.json').read_text());proof=summary['canonical_chain']
 records=[json.loads(s) for s in (raw/'raw-trace.jsonl').read_text().splitlines()]
 accepted=[r for r in records if r['kind']==12 and r['tag']==29 and r['values'][0]=='1']
 assert summary['schema_status']=='VALID' and not summary['identity_errors']
 assert proof['accepted_raw_count']==proof['qualified_decoded_count']==proof['metric_receipt_count']==len(accepted),(name,proof)
 assert proof['status']==('QUALIFIED' if accepted else 'NOT_OBSERVED')
 m.require_canonical_chain(summary,require_positive=bool(accepted))
 assert summary['analysis']['mode']=='OFFLINE_REVERIFICATION'
 assert summary['analysis']['input_source_tree_sha256']=='590b1565f9f69b52ed3ca8cdd19f8ca798ded09c9025917cbc47ba90b501e4f2'
 try:m.require_bound_compilation(out,summary,{'source_tree_sha256':summary['analysis']['input_source_tree_sha256']})
 except RuntimeError as e:assert 'offline reverification' in str(e)
 else:raise AssertionError('offline result was accepted as fresh sampling')
 previous=json.loads((raw/'compiled-summary.json').read_text())
 for field in ['common','terminal','final_snapshot_sha256','product_errors','artifact_identity']:assert summary[field]==previous[field],(name,field)
 for p in raw.iterdir():
  if p.is_file():assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest['files'][str(p.relative_to(BASE))]['sha256'],p
 results.append({'run_id':name,'schema_status':summary['schema_status'],'canonical_chain_status':proof['status'],'accepted_raw_count':len(accepted),'accepted_raw_sequences':[r['seq'] for r in accepted],
  'qualified_decoded_count':proof['qualified_decoded_count'],'metric_receipt_count':proof['metric_receipt_count'],'attempt_job_counts':[len(c['attempt_jobs']) for c in proof['chains']],
  'old_common_terminal_snapshot_product_artifacts_unchanged':True,'original_raw_directory':str(raw),'recompiled_directory':str(out),'primary_timing_adopted':False,'sampling_promotion_rejected':True})
value={'status':'OFFLINE_REVERIFICATION_VERIFIED','fresh_browser_workers':0,'population_rule':'actual accepted raw records per dataset, never a constant inferred from another run','rows':results}
(RESULT/'offline-reverification.json').write_text(json.dumps(value,indent=2)+'\n');print(json.dumps(value))
