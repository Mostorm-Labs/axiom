"""One authorized P36 continuation. No retry selection and no product changes."""
import argparse,hashlib,importlib.util,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import source_identity,observed_worker,run_matrix
AUTH=ROOT/'.aegis/results/G5-UR0b/control-reverification-r1/repair-authorization.json'
AUTH_SHA='007f4a09e625b4d1c1999c50577498c0cbf9570ceae190158559db8540b202f7'
CASES=['lower_z_translucent_drag','permanent_live_mask','transparent_color_bg','warm_miss_pressure','dpr_switch','feedback_canonical']
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,v):p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
def plan():
 rows=[dict(run_id='archived-original-'+str(n),phase='original',limit_seconds=90,status='ARCHIVED_IMMUTABLE') for n in range(1,4)]
 rows += [dict(run_id='diagnostic-'+m,phase='diagnostic',toggle=m,limit_seconds=90,status='PLANNED') for m in ['OFF','ON']]
 for n,order in enumerate(['AB','BA','BA','AB','AB','BA'],1):
  rows += [dict(run_id=f'qualification-p{n}-{m}',pair=n,phase='qualification',toggle='OFF' if m=='A' else 'ON',limit_seconds=90,status='PLANNED') for m in order]
 rows += [dict(run_id='realtime-'+str(hz),phase='separate',mode='realtime',branch=str(hz),limit_seconds=90,status='PLANNED') for hz in [60,120,240]]
 rows += [dict(run_id='correctness-'+case,phase='separate',mode='correctness',branch=case,limit_seconds=90,status='PLANNED') for case in CASES]
 matrix=load(ROOT/'docs/planning/g5-unified-render/ur0a/matrix.json')
 rows += [dict(r,phase='main',limit_seconds=180) for r in run_matrix.plan_rows(matrix)]
 assert len(rows)==56 and sum(r['limit_seconds'] for r in rows)==7740
 return rows
def claim(path,rows,phase):
 path=Path(path);rows=load(path) if path.exists() else rows
 if any(r['phase']==phase and r['status']!='PLANNED' for r in rows):raise RuntimeError('phase already claimed; preserve outcomes, no retry')
 for r in rows:
  if r['phase']==phase:r['status']='CLAIMED'
 write(path,rows);return rows
def hard_stop(guard):return guard['reason'] in ['timeout','rss_limit','observer_failure'] or guard['returncode']==124 or not guard['cleanup_complete']
def assert_sampling_authority_open(previous_return,authorization_sha):
 previous_return=Path(previous_return)
 if previous_return.is_file():
  previous=load(previous_return)
  if previous.get('authorization_ref',{}).get('sha256')==authorization_sha and previous.get('continue_execution') is False:
   raise RuntimeError('sampling authorization is closed by the preserved return; control review must explicitly reopen it, even for a fresh output directory')
def require_canonical_chain(summary,require_positive=False,require_metrics=True):
 chain=summary.get('canonical_chain')
 if not chain or chain.get('schema')!='g5-ur0b-canonical-chain-proof-v1':raise RuntimeError('canonical positive-chain proof missing')
 if chain.get('scope') not in ['NATIVE_AND_METRICS','NATIVE_ONLY']:raise RuntimeError('canonical proof scope unknown')
 if summary.get('critical_loss')!=0 or summary.get('identity_errors')!=[] or chain.get('unresolved')!=[]:raise RuntimeError('canonical observation has loss, identity error or unresolved links')
 n=chain.get('accepted_raw_count');qualified=chain.get('qualified_decoded_count');metric=chain.get('metric_receipt_count');links=chain.get('chains')
 if type(n) is not int or n<0 or type(qualified) is not int or qualified!=n or not isinstance(links,list) or len(links)!=n:raise RuntimeError('canonical raw/decoded/chain populations disagree')
 if require_metrics and (chain.get('scope')!='NATIVE_AND_METRICS' or type(metric) is not int or metric!=n):raise RuntimeError('canonical metric receipt population missing')
 if chain.get('status')!=('QUALIFIED' if n else 'NOT_OBSERVED') or require_positive and not n:raise RuntimeError('positive canonical completion chain not observed')
 uint=lambda s:isinstance(s,str) and re.fullmatch(r'\d+',s) is not None and int(s)<2**64
 numeric=['runtime_instance','lifetime_generation','attachment_epoch','document_epoch','resource_generation','view_id','committed_generation','scene_generation','scene_revision','camera_generation','surface_generation','metrics_generation','frame_id','commit_revision']
 seen=set()
 for link in links:
  basis=link.get('basis',{});sequences=[link.get(k) for k in ['draw_seq','provider_seq','capture_seq','completion_seq']]
  if any(not uint(basis.get(k)) for k in numeric) or any(not isinstance(basis.get(k),str) or re.fullmatch(r'[0-9a-f]{32}',basis[k]) is None for k in ['document_id','operation_id']):raise RuntimeError('canonical full identity missing or lossy')
  if any(not uint(v) for v in sequences) or not all(int(a)<int(b) for a,b in zip(sequences,sequences[1:])):raise RuntimeError('canonical draw/submit/capture/completion sequence invalid')
  if not uint(link.get('draw_job')) or not uint(link.get('draw_basis')) or not uint(link.get('work_id')) or link['draw_job'] not in link.get('attempt_jobs',[]):raise RuntimeError('canonical successful attempt provenance missing')
  key=(tuple(sorted(basis.items())),link['work_id'],link['completion_seq'])
  if key in seen:raise RuntimeError('duplicate canonical completion chain')
  seen.add(key)
def require_bound_compilation(directory,summary,proof):
 directory=Path(directory);analysis=summary.get('analysis',{});ref=summary.get('canonical_chain_ref',{})
 if analysis.get('mode')!='WORKER_COMPILE':raise RuntimeError('canonical offline reverification is not a fresh sampling admission')
 if analysis.get('input_source_tree_sha256')!=proof.get('source_tree_sha256'):raise RuntimeError('canonical diagnostic source tree differs from current mechanism proof')
 for name,key in [('raw-trace.jsonl','raw_trace_sha256'),('worker-result.json','worker_result_sha256'),('run-inputs.json','run_inputs_sha256')]:
  if not (directory/name).is_file() or analysis.get(key)!=sha(directory/name):raise RuntimeError('canonical diagnostic input binding changed: '+name)
 if ref.get('path')!='canonical-chain.json' or not (directory/'canonical-chain.json').is_file() or ref.get('sha256')!=sha(directory/'canonical-chain.json') or load(directory/'canonical-chain.json')!=summary.get('canonical_chain'):raise RuntimeError('canonical proof bytes do not match compiled summary')
 expected=['compile_run.mjs','decode_trace.mjs','metrics_compiler.mjs','canonical_chain.mjs','external_metrics.mjs','schedule.mjs','validate_metrics.py']
 if analysis.get('compiler_files')!={name:sha(HERE/name) for name in expected}:raise RuntimeError('canonical compiler source binding changed')
def require_qualified_diagnostic(out,proof):
 out=Path(out);decision=out/'diagnostic-result.json';summary_path=out/'raw-runs/diagnostic-ON/compiled-summary.json'
 if not decision.is_file() or load(decision).get('status')!='QUALIFIED' or not summary_path.is_file():raise RuntimeError('positive canonical diagnostic prerequisite missing')
 summary=load(summary_path);require_canonical_chain(summary,require_positive=True);require_bound_compilation(summary_path.parent,summary,proof)
 if summary.get('schema_status')!='VALID' or summary.get('artifact_identity')!=proof.get('qualified_sdk_artifacts'):raise RuntimeError('canonical diagnostic schema/artifact prerequisite differs')
def verify(proofs):
 if sha(AUTH)!=AUTH_SHA:raise RuntimeError('changed P36 authorization')
 auth=load(AUTH)
 for key in ['parent_package_ref','parent_activation_ref','independent_review_ref']:
  ref=auth[key]
  if sha(ROOT/ref['path'])!=ref['sha256']:raise RuntimeError('changed authority '+key)
 package=load(ROOT/auth['parent_package_ref']['path'])
 for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','evidence_contract_ref','scope_contract_ref']:
  ref=package[key]
  if sha(ROOT/'.aegis/packages/G5-UR0b'/ref['path'])!=ref['sha256']:raise RuntimeError('changed parent frozen '+key)
 if Path.cwd().resolve()!=ROOT or subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip()!=auth['execution_branch']:raise RuntimeError('wrong controlled worktree/branch')
 if 'Mostorm-Labs/axiom' not in subprocess.check_output(['git','remote','get-url','origin'],cwd=ROOT,text=True):raise RuntimeError('wrong repository')
 assert_sampling_authority_open(ROOT/'.aegis/results/G5-UR0b/p36-r1/result.json',AUTH_SHA)
 proof=load(proofs)
 if proof['status']!='MECHANISMS_VERIFIED' or proof['source_tree_sha256']!=source_identity.identity(ROOT)['source_tree_sha256']:raise RuntimeError('mechanism proof does not bind current source')
 if any(sha(ROOT/r['path'])!=r['sha256'] for r in proof['files']):raise RuntimeError('mechanism log/artifact changed')
 return proof
def execute(row,sdk,out):
 target=out/'raw-runs'/row['run_id'];prefix=out/'guards'/row['run_id']
 if target.exists() or prefix.with_suffix('.worker.json').exists():raise RuntimeError('refuse replacing an observed outcome')
 mode=row.get('mode','fixture');branch=row.get('branch','navigation')
 if row['phase']=='main':
  mode='legacy' if row['case'].startswith('legacy') else 'main';branch=row['case'].removeprefix('immediate_')
 print(json.dumps({'stage':row['run_id']}),flush=True)
 guard=observed_worker.run(['node',str(HERE/'worker.mjs'),str(Path(sdk).resolve()),str(target.resolve()),mode,branch,row.get('toggle','ON')],prefix,row['limit_seconds'],3072*1024)
 outcome={'guard':guard,'raw_directory':str(target),'guard_record':str(prefix.with_suffix('.worker.json'))}
 row.update(status=guard['reason'].upper(),outcome=outcome)
 if (target/'worker-result.json').is_file():
  result=load(target/'worker-result.json');outcome['worker_status']=result['status'];outcome['correctness_status']=result.get('correctness_status')
  try:
   subprocess.run(['node',str(HERE/'compile_run.mjs'),str(target)],check=True,cwd=ROOT,timeout=120,stdout=(target/'compiler.log').open('w'),stderr=subprocess.STDOUT)
   outcome['summary']=load(target/'compiled-summary.json')
  except (subprocess.SubprocessError,OSError) as error:outcome['compiler_error']=str(error)
 return outcome
def paired(rows,phase):
 pairs=[]
 numbers=[0] if phase=='diagnostic' else range(1,7)
 for n in numbers:
  found={r['toggle']:r.get('outcome',{}).get('summary') for r in rows if r['phase']==phase and r.get('pair',0)==n}
  a,b=found.get('OFF'),found.get('ON');p={'pair':n}
  if a and b:
   p.update(off={**(a['common'] or {}),'critical_loss':a['critical_loss']},on={**(b['common'] or {}),'critical_loss':b['critical_loss']},
    same_artifact=a['artifact_identity']==b['artifact_identity'],same_document=a['final_snapshot_sha256']==b['final_snapshot_sha256'],
    same_terminal=a['terminal']['activeContacts']==b['terminal']['activeContacts']==0,
    same_product_error_codes=[e['error'] for e in a['product_errors']]==[e['error'] for e in b['product_errors']])
  pairs.append(p)
 return pairs
def stop(out,rows,reason,status='FROZEN_VERIFICATION_FAILURE'):
 for r in rows:
  if r['status'] in ['PLANNED','CLAIMED']:r.update(status='MISSING',reason=reason)
 write(out/'worker-ledger.json',rows);write(out/'execution-stop.json',{'status':status,'reason':reason,'primary_timing_adopted':False,'automatic_rerun':False,'official_gate_pass':False})
 return 2
def phase(a,which):
 proof=verify(a.proofs);out=a.output;out.mkdir(parents=True,exist_ok=True)
 if (out/'execution-stop.json').exists():raise RuntimeError('frozen stop already exists; no automatic retry')
 ledger=out/'worker-ledger.json';rows=load(ledger) if ledger.exists() else plan()
 prerequisite={'qualification':'diagnostic-result.json','separate':'overhead-qualification.json','main':'separate-result.json'}.get(which)
 if prerequisite and (not (out/prerequisite).exists() or load(out/prerequisite)['status']!='QUALIFIED'):raise RuntimeError('phase prerequisite not qualified: '+prerequisite)
 if which!='diagnostic':require_qualified_diagnostic(out,proof)
 rows=claim(ledger,rows,which)
 for row in rows:
  if row['phase']!=which:continue
  sdk=a.sdk32 if which=='main' and row['budget_mib']['navigation']==32 else a.sdk
  if not sdk:return stop(out,rows,'missing exact 32/96 SDK artifact','BLOCKED_EVIDENCE')
  outcome=execute(row,sdk,out);write(ledger,rows)
  if hard_stop(outcome['guard']):return stop(out,rows,'frozen guard/cleanup stop at '+row['run_id'])
  summary=outcome.get('summary')
  if outcome.get('worker_status')!='OBSERVED' or not summary or not summary['source_stable'] or not summary['artifacts_stable']:return stop(out,rows,'execution/artifact/source mechanism unresolved at '+row['run_id'],'BLOCKED_EVIDENCE')
  if row.get('toggle','ON')=='ON' and (summary['critical_loss'] or summary['identity_errors']):return stop(out,rows,'critical/foreign observation at '+row['run_id'],'BLOCKED_EVIDENCE')
  if row.get('toggle','ON')=='ON' and any(not summary['actual_observations'][k] for k in ['raw_records','owners','leaves']):return stop(out,rows,'actual observed owner/target links missing at '+row['run_id'],'BLOCKED_EVIDENCE')
  if row.get('mode')!='correctness' and summary['schema_status']!='VALID':return stop(out,rows,'frozen metrics schema unresolved at '+row['run_id'],'BLOCKED_EVIDENCE')
  if row.get('toggle','ON')=='ON':
   try:
    require_canonical_chain(summary,require_positive=which=='diagnostic',require_metrics=row.get('mode')!='correctness')
    require_bound_compilation(Path(outcome['raw_directory']),summary,proof)
   except RuntimeError as error:return stop(out,rows,str(error)+' at '+row['run_id'],'BLOCKED_EVIDENCE')
  expected=proof.get('qualified_sdk32_artifacts') if which=='main' and row['budget_mib']['navigation']==32 else proof['qualified_sdk_artifacts']
  if summary['artifact_identity']!=expected:return stop(out,rows,'worker artifacts differ from exact mechanism build','BLOCKED_EVIDENCE')
 if which in ['diagnostic','qualification']:
  pairs=paired(rows,which);write(out/(which+'-pairs.json'),pairs)
  if any(not p.get(k) for p in pairs for k in ['same_artifact','same_document','same_terminal','same_product_error_codes']):return stop(out,rows,'same-binary document/contact/product outcome mismatch','BLOCKED_EVIDENCE')
  if which=='qualification':
   script="import{qualifyOverhead}from'./verification/runners/g5/ur0/fixture_plan.mjs';import fs from'node:fs';console.log(JSON.stringify(qualifyOverhead(JSON.parse(fs.readFileSync(process.argv[1])))))"
   decision=json.loads(subprocess.check_output(['node','--input-type=module','-e',script,str(out/(which+'-pairs.json'))],cwd=ROOT,text=True))
  else:decision={'status':'QUALIFIED','scope':'mechanism/guard diagnostic pair only; does not qualify overhead or primary timings'}
  write(out/('overhead-qualification.json' if which=='qualification' else 'diagnostic-result.json'),decision)
  if decision['status']!='QUALIFIED':return stop(out,rows,'fresh overhead cohort '+decision['status']+'; no noise retry authorized')
 elif which=='separate':
  bad=[r['run_id'] for r in rows if r['phase']=='separate' and r.get('mode')=='correctness' and r['outcome'].get('correctness_status') not in ['QUALIFIED','PRODUCT_RED_IN_BOTH_OFF_ON']]
  write(out/'separate-result.json',{'status':'BLOCKED_EVIDENCE' if bad else 'QUALIFIED','unresolved_cases':bad,'product_RED_preserved':True})
  if bad:return stop(out,rows,'independent correctness/pressure proof incomplete','BLOCKED_EVIDENCE')
 else:write(out/'main-result.json',{'status':'READY_FOR_CONTROL_REVIEW','all_planned_rows_accounted':True,'performance_pass':False})
 return 0
def main():
 p=argparse.ArgumentParser();p.add_argument('--phase',choices=['diagnostic','qualification','separate','main'],required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--sdk32',type=Path);p.add_argument('--proofs',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();return phase(a,a.phase)
if __name__=='__main__':raise SystemExit(main())
