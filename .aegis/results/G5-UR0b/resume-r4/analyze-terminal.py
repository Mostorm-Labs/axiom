"""Read-only extraction of actual guards and partial correctness artifacts."""
import hashlib,json,math,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b/resume-r4/execution'
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
w=load(RESULT/'window.json');assert w['status']=='CLOSED' and w['terminal_status']=='FROZEN_VERIFICATION_FAILURE'
started=[r for r in w['rows'] if 'outcome' in r];missing=[r for r in w['rows'] if r['status']=='MISSING'];admitted=[r for r in started if r.get('admission_status')=='QUALIFIED']
assert len(w['rows'])==38 and len(started)==3 and len(admitted)==2 and len(missing)==35
assert sum(math.ceil(r['outcome']['guard']['elapsed_s']) for r in started)==118==w['new_spent_seconds'] and w['charged_seconds']==1251 and w['reserved_seconds']==0
failed=next(r for r in started if r['admission_status']=='REJECTED');guard=failed['outcome']['guard']
assert failed['run_id']=='correctness-lower_z_translucent_drag' and guard['returncode']==124 and guard['last_stage']=={'stage':'phase-timeout','phase':'correctness-only','limit_ms':30000}
assert guard['cleanup_complete'] and all(r['outcome']['guard']['cleanup_complete'] for r in started)
directory=OUT/'raw-runs'/failed['run_id'];off=[load(directory/name) for name in ['off-0.correctness.json','off-1.correctness.json']]
assert all(r['toggle']=='OFF' and r['comparison']['status']=='FAILED' for r in off)
assert not (directory/'worker-result.json').exists() and not (directory/'raw-trace.jsonl').exists() and not list(directory.glob('on-*.correctness.json'))
partial=[{'case':r['case_id'],'toggle':r['toggle'],'step':r['step'],'status':r['comparison']['status'],'interior_mismatched_pixels':r['comparison']['interior_mismatched_pixels'],'max_interior_delta':r['comparison']['max_interior_delta'],'comparison':r['comparison'],'candidate_sha256':r['candidate_sha256'],'reference_snapshot_sha256':r['reference_snapshot_sha256'],'artifact_ref':ref(directory/('off-'+str(r['step'])+'.correctness.json'))} for r in off]
observed=set();truncated=[]
for p in (OUT/'guards').glob('*.rss.jsonl'):
 with p.open() as stream:
  for line in stream:
   event=json.loads(line);assert not event['attribution_truncated'];observed.update(r['pid'] for r in event['owned_processes'])
 if load(p.with_suffix('').with_suffix('.rss-metadata.json'))['truncated']:truncated.append(str(p))
live=[]
for line in subprocess.check_output(['ps','-axo','pid=,ppid=,stat=,command='],text=True).splitlines():
 fields=line.strip().split(None,3)
 if len(fields)==4 and int(fields[0]) in observed and not fields[2].startswith('Z'):live.append({'pid':int(fields[0]),'state':fields[2],'command':fields[3]})
assert not live and not truncated
write(RESULT/'cleanup-verification.json',{'status':'ALL_OBSERVED_PROCESSES_GONE','workers':3,'observed_pid_count':len(observed),'observed_pids':sorted(observed),'alive_observed_processes':live,'all_guards_cleanup_complete':True,'rss_sidecars_untruncated':True,'scope':'Only this r4 process family; no unrelated process signalled'})
fresh=[]
for row in admitted:
 source=OUT/'raw-runs'/row['run_id'];s=load(source/'compiled-summary.json');p=s['canonical_chain'];g=row['outcome']['guard']
 assert s['analysis']['mode']=='WORKER_COMPILE' and s['analysis']['input_source_tree_sha256']==w['source_tree_sha256'] and s['schema_status']=='VALID'
 assert (p['accepted_raw_count'],p['qualified_initialization_count'],p['qualified_committed_count'],p['unresolved_accepted_count'])==(2,1,1,0) and s['critical_loss']==0
 run=load(source/'worker-result.json');measurement=run['measurement']
 fresh.append({'run_id':row['run_id'],'summary_ref':ref(source/'compiled-summary.json'),'proof_ref':ref(source/'canonical-chain.json'),'mode':s['analysis']['mode'],'source_tree_sha256':s['analysis']['input_source_tree_sha256'],'guard_reason':g['reason'],'guard_elapsed_s':g['elapsed_s'],'clock':s['clock'],'planned_events':len(measurement['planned']),'handler_events':len(measurement['handlers']),'phase_lifecycle':{'active_contacts_after':measurement['after']['activeContacts']},'counts':{k:p[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','metric_receipt_count','unresolved_accepted_count']}})
analysis={'status':'FROZEN_VERIFICATION_FAILURE','stop_run_id':failed['run_id'],'actual_guard_failure':{'classification':'CORRECTNESS_PHASE_DEADLINE','guard_ref':ref(OUT/'guards/correctness-lower_z_translucent_drag.worker.json'),'reason':guard['reason'],'returncode':124,'phase':guard['last_stage'],'elapsed_s':guard['elapsed_s'],'rss_peak_kib':guard['peak_process_family_rss_kib'],'rss_limit_kib':guard['rss_limit_kib'],'cleanup_complete':True},
 'retained_runner_stop_reason':w['reason'],'stop_reason_interpretation':'Fresh-summary admission reported missing WORKER_COMPILE because the phase timer exited before worker-result/trace/compiler output. This was a real frozen guard deadline, not a rejected archived reuse input. Original ledger/message bytes retained.',
 'last_completed_outputs':'OFF0 and OFF1 comparisons persisted; no ON comparison, worker-result, raw trace or final correctness_status. Existing stage markers do not identify the precise pending await during transition to the ON pair.',
 'partial_off_pixel_results':partial,'paired_correctness_verdict':'INCOMPLETE_NOT_PRODUCT_RED_IN_BOTH_OFF_ON','product_root_cause':'UNRESOLVED; do not relax AA/oracle or infer matching ON behavior','fresh_admitted_runs':fresh,
 'reused_prerequisites_ref':ref(RESULT/'reused-prerequisites.json'),'history':{'prior_starts':30,'prior_seconds':1133,'new_starts':3,'new_charged_seconds':118,'cumulative_starts':33,'cumulative_charged_seconds':1251,'new_planned':38,'new_admitted':2,'new_guard_stopped':1,'new_missing':35,'new_nominal_max_seconds':6120,'original_cap_seconds':8370,'reserved_seconds':0,'remaining_budget_is_retry_authorization':False},
 'guard_totals':{'completed':2,'phase_timeout_exit124':1,'rss_stops':0,'cleanup_failures':0,'max_process_family_rss_kib':max(r['outcome']['guard']['peak_process_family_rss_kib'] for r in started),'rss_limit_kib':3145728},'remaining_ids':[r['run_id'] for r in missing],
 'owner_followup':'Independent control review of original correctness worker phase/reload/setup/oracle evidence. Existing harness/compiler changes and new sampling are outside this package. No retry or deadline increase performed.','continue_execution':False,'automatic_retry':False,'official_gate_pass':False,'performance_pass':False}
write(RESULT/'terminal-analysis.json',analysis)
print(json.dumps({'status':'VERIFIED_FROZEN_GUARD_RETURN','new_starts':3,'missing':35,'cumulative_seconds':1251,'observed_pids_gone':len(observed),'partial_OFF_interior_mismatches':[r['interior_mismatched_pixels'] for r in partial]}))
