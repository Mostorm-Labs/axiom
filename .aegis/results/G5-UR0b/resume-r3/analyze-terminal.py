"""Read-only analysis of the frozen r3 return; never recompile or resample."""
import hashlib,json,math,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
RESULT=Path(__file__).resolve().parent
OUT=ROOT/'out/ur0b/resume-r3/execution'
def load(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(p):return {'path':str(Path(p).relative_to(ROOT)),'sha256':sha(p)}
def write(p,v):Path(p).write_text(json.dumps(v,indent=2)+'\n')
window=load(RESULT/'window.json');rows=window['rows']
assert window['status']=='CLOSED' and window['terminal_status']=='FROZEN_VERIFICATION_FAILURE'
assert len(rows)==53 and len({r['run_id'] for r in rows})==53
executed=[r for r in rows if 'outcome' in r];missing=[r for r in rows if r['status']=='MISSING']
assert len(executed)==15 and len(missing)==38 and not any(r['status'] in ['RUNNING','PLANNED','CLAIMED'] for r in rows)
assert sum(math.ceil(r['outcome']['guard']['elapsed_s']) for r in executed)==536==window['new_spent_seconds']
assert window['charged_seconds']==1133 and window['reserved_seconds']==0
assert all(r['outcome']['guard']['cleanup_complete'] and r['outcome']['guard']['returncode']==0 and r['outcome']['guard']['reason']=='completed' for r in executed)
observed=set();rss_truncations=[]
for p in sorted((OUT/'guards').glob('*.rss.jsonl')):
 with p.open() as stream:
  for line in stream:
   event=json.loads(line);assert not event['attribution_truncated']
   observed.update(r['pid'] for r in event['owned_processes'])
 metadata=load(p.with_suffix('').with_suffix('.rss-metadata.json'))
 if metadata['truncated']:rss_truncations.append(str(p))
table=subprocess.check_output(['ps','-axo','pid=,ppid=,stat=,command='],text=True)
alive=[]
for line in table.splitlines():
 fields=line.strip().split(None,3)
 if len(fields)==4 and int(fields[0]) in observed and not fields[2].startswith('Z'):alive.append({'pid':int(fields[0]),'ppid':int(fields[1]),'state':fields[2],'command':fields[3]})
assert not alive and not rss_truncations
write(RESULT/'cleanup-verification.json',{'status':'ALL_OBSERVED_PROCESSES_GONE','workers':len(executed),'guard_cleanup_complete':True,'rss_sidecars_untruncated':True,'observed_pid_count':len(observed),'observed_pids':sorted(observed),'alive_observed_processes':alive,'scope':'Only PIDs observed in this continuation; no unrelated processes signalled'})

directory=OUT/'raw-runs/realtime-60';summary=load(directory/'compiled-summary.json');decoded=load(directory/'decoded.json')
with (directory/'raw-trace.jsonl').open() as stream:raw=[json.loads(line) for line in stream]
accepted=[r for r in raw if r['kind']==12 and r['tag']==29 and r['values'][0]=='1']
bootstrap=[c for c in decoded['unqualified_completions'] if c['qualification']=='MISSING_SETUP_SCENE_OBSERVATION' and c['semanticGeneration']=='0' and c['sceneGeneration']=='0' and c['commitRevision']=='0' and c['operation'] is None]
assert len(accepted)==2 and len(bootstrap)==1 and len(decoded['qualified_completions'])==1 and len(decoded['scenes'])==1
initial=bootstrap[0];initial_record=next(r for r in accepted if r['seq'] in initial['provenance'])
assert initial_record['seq']=='47' and initial['capture_seq']=='42' and int(initial_record['at_ns'])<int(decoded['commits'][0]['at_ns'])
assert summary['critical_loss']==0 and summary['identity_errors']==[] and summary['schema_status']=='VALID'
proof=summary['canonical_chain'];assert proof['status']=='MISSING' and proof['accepted_raw_count']==2 and proof['qualified_decoded_count']==proof['metric_receipt_count']==1
selected={initial['draw_seq'],initial['provider_seq'],initial['capture_seq'],initial_record['seq']}
witness={'status':'INITIAL_GENERATION_ZERO_COMPLETION_NOT_QUALIFIABLE_BY_CURRENT_COMMIT_SCENE_CHAIN','run_id':'realtime-60',
 'raw_ref':ref(directory/'raw-trace.jsonl'),'decoded_ref':ref(directory/'decoded.json'),'chain_ref':ref(directory/'canonical-chain.json'),'summary_ref':ref(directory/'compiled-summary.json'),
 'actual_raw_records':[r for r in raw if r['seq'] in selected],'unqualified_initial_completion':initial,'first_observed_semantic_commit':decoded['commits'][0],
 'qualified_later_completion':decoded['qualified_completions'][0],'canonical_population':{'raw_accepted':2,'decoded_qualified':1,'metric_receipts':1},
 'loss':decoded['loss'],'identity_errors':decoded['errors'],'compiler_files':summary['analysis']['compiler_files'],
 'finding':'The initial accepted provider frame has generation/revision zero and no Operation identity or observed semantic Scene publication. The current positive-chain compiler requires a unique committed Scene link for every raw accepted completion, so raw2 cannot reconcile with decoded/metrics1.',
 'classification':'FROZEN_VERIFICATION_FAILURE; initialization/setup-frame observation and admission closure gap',
 'owner_action':'Independent control/Verification review must define a justified initialization basis or scope treatment before any compiler/collector repair. Preserve this raw result; do not invent an Operation, discard the frame, or change this window to retry.',
 'root_cause_limit':'Evidence proves the unmatched initialization frame. It does not establish a new product rendering/performance defect or authorize its repair.'}
write(RESULT/'terminal-witness.json',witness)
phase_counts={p:{'planned':sum(r['phase']==p for r in rows),'executed':sum(r['phase']==p for r in executed),'missing':sum(r['phase']==p for r in missing)} for p in ['diagnostic','qualification','separate','main']}
write(RESULT/'terminal-analysis.json',{'status':'FROZEN_VERIFICATION_FAILURE','stop_reason':window['reason'],'witness_ref':ref(RESULT/'terminal-witness.json'),'cleanup_ref':ref(RESULT/'cleanup-verification.json'),
 'diagnostic':load(OUT/'diagnostic-result.json'),'overhead_qualification':load(OUT/'overhead-qualification.json'),'phase_counts':phase_counts,
 'accounting':{'historical_starts':15,'historical_charged_seconds':597,'new_planned':53,'new_started':15,'new_completed_guard':15,'new_admitted':14,'new_proof_blocked':1,'new_missing':38,'new_charged_seconds':536,'cumulative_charged_seconds':1133,'cumulative_authorized_max_seconds':8067,'original_cap_seconds':8370,'reserved_seconds':0,'remaining_budget_is_retry_authorization':False},
 'guard_summary':{'timeouts':0,'rss_stops':0,'nonzero_exits':0,'cleanup_failures':0,'maximum_process_family_rss_kib':max(r['outcome']['guard']['peak_process_family_rss_kib'] for r in executed),'rss_limit_kib':3145728},
 'no_retry':True,'no_new_compiler_product_or_authority_change_after_sampling':True,'official_gate_pass':False,'performance_pass':False,'continue_execution':False})
print(json.dumps({'status':'TERMINAL_WITNESS_AND_ACCOUNTING_VERIFIED','new_workers':15,'missing':38,'charged_seconds':1133,'observed_pids_gone':len(observed),'witness_sha256':sha(RESULT/'terminal-witness.json')}))
