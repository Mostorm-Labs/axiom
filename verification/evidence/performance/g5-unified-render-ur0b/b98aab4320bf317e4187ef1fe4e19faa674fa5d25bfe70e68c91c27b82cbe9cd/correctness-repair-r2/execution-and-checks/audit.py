"""Scope, timing, profile and historical-reuse checks; no execution window."""
import hashlib,json,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent;HERE=ROOT/'verification/runners/g5/ur0'
sys.path.insert(0,str(HERE));import source_identity,remaining_runner
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def ref(p):return {'path':str(p.relative_to(ROOT)),'sha256':sha(p)}
baseline=load(D/'starting-source-identity.json');current=source_identity.identity(ROOT)
allowed=load(ROOT/'.aegis/packages/G5-UR0b-correctness-repair-r2/package.json')['authorization']['effective_source_paths']
changed=sorted(n for n in baseline['files'].keys()|current['files'].keys() if baseline['files'].get(n)!=current['files'].get(n))
assert set(changed)<=set(allowed),changed
old=(ROOT/'out/ur0b/correctness-repair-r2/baseline/verification/runners/g5/ur0/worker.mjs').read_text();new=(HERE/'worker.mjs').read_text()
def section(text,start,end):return text.split(start,1)[1].split(end,1)[0]
checks={}
for name,start,end in [
 ('server_factory_shim_clock_init',' server=http.createServer',' const capture=async label=>'),
 ('primary_modes_and_measurement'," else if(mode==='realtime')",' if(result.measurement?.clock_before)'),
 ('trace_and_pressure_collection'," if(toggle!=='NORMAL'){\n  stage({stage:'cold-trace-begin'})",' result.close='),
 ('cleanup_trace_and_finalization'," if(toggle!=='NORMAL'){\n  const stream=await fs.open(path.join(out,'raw-cleanup.jsonl')",None),
]:
 if name=='server_factory_shim_clock_init':
  # Only the newly imported, cold helper declarations were inserted after the
  # unchanged init call, before capture. The server/clock/createRuntime prefix
  # itself remains exact.
  end=" result.init=await page.evaluate(()=>ur0.init());await write('init.json',result.init);stage({stage:'setup'});"
 a=old.split(start,1)[1];b=new.split(start,1)[1]
 if end:a=a.split(end,1)[0];b=b.split(end,1)[0]
 assert a==b,name;checks[name]='BYTE_IDENTICAL'
arm="const arm=(phase,limit_ms)=>";assert section(old,arm,"arm('setup'")==section(new,arm,"arm('setup'")
assert re.findall(r"arm\('[^']+'[^;]*;",old)==re.findall(r"arm\('[^']+'[^;]*;",new)
assert new.count("arm('correctness-only',30000)")==1
assert "result.setup=await page.evaluate(kind=>ur0.correctnessSetup(kind,false),branch);arm('correctness-only',30000);" in new
normal=section(old,' const capture=async label=>{'," if(mode==='correctness'){")
actual=section(new,' const capture=async label=>{'," if(mode==='correctness'){").replace("\n  if(mode==='correctness')return correctnessCapture(label);",'')
assert normal==actual,'non-correctness snapshot transfer changed'
frozen_oracle=HERE/'fixture_oracle.cpp';assert baseline['files'][str(frozen_oracle.relative_to(ROOT))]==current['files'][str(frozen_oracle.relative_to(ROOT))]
oracle=ROOT/'out/host/runtime/render/axiom_ur0_fixture_oracle';assert sha(oracle)=='8605c7fd45d118a4cdd8b9bee3ba5c8a8c64b9eb7dbc09c4e74a81058ab06fa0'
profile=load(D/'repaired-profile.json')['profile']
for n,h in profile.items():assert sha(ROOT/n)==h,(n,h,sha(ROOT/n))
assert profile==remaining_runner.repair.REPAIRED_INITIALIZATION_PROFILE
subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
write(D/'source-scope-audit.json',{'status':'PASS','source_before_sha256':baseline['source_tree_sha256'],'source_after_sha256':current['source_tree_sha256'],'changed_source_paths':changed,'unchanged_outside_allowance':True,'worker_static_checks':checks,'primary_capture_unchanged':True,'phase_arm_function_calls_and_placement_unchanged':True,'phase_ms':30000,'outer_worker_seconds':90,'static_not_new_browser_timing':True,'native_oracle_sha256':sha(oracle),'native_oracle_cpp_ref':ref(frozen_oracle),'profile_literal_ref':ref(D/'repaired-profile.json'),'source_identity_exclusions_unchanged':True})
write(D/'full-source-identity.json',current)
reuse=load(remaining_runner.PACKAGE.parent/'reuse-inputs.json')
delta={name:{'accepted_sha256':h,'current_sha256':sha(HERE/name)} for name,h in reuse['compiler_file_sha256'].items() if sha(HERE/name)!=h}
assert set(delta)=={'initialization_basis.mjs'},delta
try:remaining_runner.accepted_reuse(reuse)
except RuntimeError as e:assert str(e)=='current compiler differs from accepted reuse compiler';rejection=str(e)
else:raise AssertionError('old authority silently admitted changed compiler')
write(D/'reuse-assessment.json',{'status':'RECORDED_OBSERVATIONS_INVARIANT_CURRENT_COMPILER_REACCEPTANCE_REQUIRED','accepted_reuse_packet_ref':ref(remaining_runner.PACKAGE.parent/'reuse-inputs.json'),'changed_compiler_bindings':delta,'old_fresh_admission_status':'REJECTED_AS_REQUIRED','reader_error':rejection,'reverification_ref':ref(D/'reverification.json'),'eligibility':'Exact old diagnostic/cohort/60/120/240 recordings remain invariant; independent control must accept current compiler/proof identities before future continuation. No fresh relabeling or automatic sampling permission.','old_acceptance_not_rewritten':True,'rerun_cohort':False,'new_sampling_workers':0})
failure={'status':'FROZEN_VERIFICATION_FAILURE','test':'test_g5_ur0_remaining_runner.RemainingRunnerTest.test_explicit_accepted_reuse_keeps_old_modes_and_sources','test_source_ref':ref(ROOT/'verification/tests/test_g5_ur0_remaining_runner.py'),'enforcing_reader_ref':ref(HERE/'remaining_runner.py'),'root_cause':'TEST_CONTRACT_DRIFT_AFTER_AUTHORIZED_INITIALIZATION_VERIFIER_CHANGE','changed_compiler_bindings':delta,'reason':'The unchanged test expects accepted_reuse(r4 packet) to succeed against the current compiler. The frozen packet pins the previous initialization_basis.mjs; rejection is correct after this authorized repair. Do not weaken the reader or edit the old packet.','out_of_scope_paths':['verification/tests/test_g5_ur0_remaining_runner.py'],'requested_control_action':'Classify this stale positive test and authorize its narrowly scoped update; independently reaccept the byte-invariant recordings under the new compiler in a future continuation package. Keep exact compiler/input/profile checks and old windows closed.','full_python_log_ref':ref(D/'python.log')}
write(D/'frozen-failure.json',failure)
future=[r for r in remaining_runner.plan() if r['run_id'] not in ['realtime-120','realtime-240']]
assert len(future)==36 and sum(r.get('mode')=='correctness' for r in future)==6
write(D/'future-continuation-position.json',{'status':'PLAN_ONLY_NOT_EXECUTION_AUTHORITY','pending_rows':future,'realtime120_and240':'accepted original recordings retained, not rerun','old_first_correctness_attempt':'incomplete; original timeout and partial OFF unchanged','prerequisites':['Independent review of repaired harness and this test-contract failure','Exact current compiler/profile and old-recording reuse acceptance','Separate future sampling authorization with a new bounded window'],'historical_starts':33,'historical_seconds':1251,'new_workers':0,'new_window_opened':False})
print(json.dumps({'scope':'PASS','changed_paths':len(changed),'historical_reuse':'correctly rejected under changed compiler','mandatory_test_failure':failure['test']}))
