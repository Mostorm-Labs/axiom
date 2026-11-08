import copy,hashlib,importlib.util,json,multiprocessing,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def module():
 path=ROOT/'verification/runners/g5/ur0/resume_runner.py'
 if not path.is_file():raise AssertionError('r3 continuation adapter missing')
 spec=importlib.util.spec_from_file_location('resume_runner',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
def competing_claim(state,out,queue):
 try:
  m=module()
  with m.Window(Path(state),Path(out),'package','proof',m.new_plan()):pass
 except RuntimeError:queue.put('rejected')
 else:queue.put('accepted')
class ResumeRunnerTest(unittest.TestCase):
 def test_frozen_new_plan_contains_only_53_ids_and_7470_seconds(self):
  m=module();rows=m.new_plan();self.assertEqual(len(rows),53);self.assertEqual(sum(r['limit_seconds'] for r in rows),7470);self.assertEqual(len({r['run_id'] for r in rows}),53)
  self.assertEqual([r['toggle'] for r in rows if r['phase']=='qualification'],['OFF','ON','ON','OFF','ON','OFF','OFF','ON','OFF','ON','ON','OFF'])
 def test_historical_debit_includes_preflights_and_missing_guard(self):
  m=module();history=json.loads((ROOT/'.aegis/results/G5-UR0b/control-continuation-r3/historical-budget.json').read_text())
  self.assertEqual(m.verify_history(history),597)
  bad=copy.deepcopy(history);bad['rows']=bad['rows'][1:]
  with self.assertRaises(RuntimeError):m.verify_history(bad)
  self.assertEqual(m.charge(None,90),90);self.assertEqual(m.charge({'elapsed_s':32.01},90),33)
 def test_concurrent_or_relocated_window_is_not_a_new_allowance(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state';out=Path(tmp)/'out'
   with m.Window(state,out,'package','proof',m.new_plan()):
    context=multiprocessing.get_context('spawn');queue=context.Queue();p=context.Process(target=competing_claim,args=(str(state),str(out.parent/'other'),queue));p.start();p.join(10);self.assertEqual(p.exitcode,0);self.assertEqual(queue.get(timeout=1),'rejected')
   with self.assertRaises(RuntimeError):
    with m.Window(state,out.parent/'relocated','package','proof',m.new_plan()):pass
 def test_reservation_precedes_execution_and_interruption_cannot_restart(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   state=Path(tmp)/'state'
   with m.Window(state,Path(tmp)/'out','package','proof',m.new_plan()) as w:
    w.begin('diagnostic-OFF');on_disk=json.loads((state/'window.json').read_text());self.assertEqual(on_disk['rows'][0]['status'],'RUNNING');self.assertEqual(on_disk['reserved_seconds'],90)
    with self.assertRaises(RuntimeError):w.begin('diagnostic-ON')
   stopped=json.loads((state/'window.json').read_text());self.assertEqual(stopped['status'],'CLOSED');self.assertEqual(stopped['rows'][0]['charged_seconds'],90);self.assertEqual(stopped['charged_seconds'],597+90)
   with self.assertRaises(RuntimeError):
    with m.Window(state,Path(tmp)/'new','package','proof',m.new_plan()):pass
 def test_overbudget_reservation_and_duplicate_run_rejected_before_spawn(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   with m.Window(Path(tmp)/'state',Path(tmp)/'out','package','proof',m.new_plan()) as w:
    w.data['new_spent_seconds']=7470
    with self.assertRaises(RuntimeError):w.begin('diagnostic-OFF')
    self.assertEqual(w.data['rows'][0]['status'],'PLANNED')
    w.data['new_spent_seconds']=0;w.begin('diagnostic-OFF');w.finish('diagnostic-OFF',{'guard':{'elapsed_s':1.2,'reason':'completed','cleanup_complete':True}})
    with self.assertRaises(RuntimeError):w.begin('diagnostic-OFF')
 def test_missing_terminal_guard_is_durably_charged_at_full_limit(self):
  m=module()
  for outcome in [{},{'guard':None}]:
   with self.subTest(outcome=outcome),tempfile.TemporaryDirectory() as tmp:
    state=Path(tmp)/'state'
    with m.Window(state,Path(tmp)/'out','package','proof',m.new_plan()) as w:
     w.begin('diagnostic-OFF');w.finish('diagnostic-OFF',outcome)
     saved=json.loads((state/'window.json').read_text())
     self.assertEqual(saved['charged_seconds'],687);self.assertEqual(saved['reserved_seconds'],0)
     self.assertEqual(saved['rows'][0]['status'],'UNKNOWN_GUARD')
 def test_worker_nonzero_exit_cannot_admit_an_observed_summary(self):
  m=module();row={'run_id':'diagnostic-OFF','phase':'diagnostic','toggle':'OFF'}
  summary={'source_stable':True,'artifacts_stable':True,'artifact_identity':{'axiom.wasm':'same'},'schema_status':'VALID'}
  for reason,code in [('nonzero_exit',1),('completed',1)]:
   outcome={'guard':{'reason':reason,'returncode':code,'cleanup_complete':True},'worker_status':'OBSERVED','summary':summary}
   with self.subTest(reason=reason),self.assertRaisesRegex(RuntimeError,'guard|exit|stop'):
    m.check_worker(row,outcome,{'qualified_sdk_artifacts':{'axiom.wasm':'same'}})
 def test_changed_proof_product_or_either_artifact_tier_rejected_before_claim(self):
  m=module()
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);subprocess.run(['git','init','-q',str(root)],check=True)
   m.ROOT=root;m.PACKAGE=root/'.aegis/packages/G5-UR0b-resume-r3/package.json'
   m.PACKAGE.parent.mkdir(parents=True);m.PACKAGE.write_text('{}')
   product=root/'runtime/product.hpp';product.parent.mkdir();product.write_text('fixed product')
   compiler=root/'verification/runners/g5/ur0/compile_run.mjs';compiler.parent.mkdir(parents=True);compiler.write_text('fixed compiler')
   state=root/'.aegis/results/G5-UR0b';state.mkdir(parents=True)
   accepted=state/'accepted';(accepted/'source').mkdir(parents=True)
   snapshot=m.source_identity.identity(root)
   (accepted/'source/full-source-identity.json').write_text(json.dumps(snapshot))
   (state/'p36-r2').mkdir();(state/'p36-r2/result.json').write_text(json.dumps({'materialized_ref':{'path':str(accepted)}}))
   sdk64=state/'sdk64';sdk32=state/'sdk32'
   for sdk in [sdk64,sdk32]:sdk.mkdir();(sdk/'axiom.wasm').write_bytes(b'accepted wasm')
   log=state/'mechanisms.log';log.write_text('verified')
   digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
   proof={'status':'MECHANISMS_VERIFIED','source_tree_sha256':snapshot['source_tree_sha256'],'package_sha256':m.PACKAGE_SHA,
    'files':[{'path':str(log.relative_to(root)),'sha256':digest(log)},{'path':str(compiler.relative_to(root)),'sha256':digest(compiler)}],
    'qualified_sdk_artifacts':{'axiom.wasm':digest(sdk64/'axiom.wasm')},'qualified_sdk32_artifacts':{'axiom.wasm':digest(sdk32/'axiom.wasm')}}
   path=state/'proof.json';path.write_text(json.dumps(proof));contract={'authorized_source_paths':[]}
   m.verify_proof(path,{},contract,sdk64,sdk32)
   for key in ['source_tree_sha256','package_sha256']:
    bad=copy.deepcopy(proof);bad[key]='changed';path.write_text(json.dumps(bad))
    with self.subTest(key=key),self.assertRaises(RuntimeError):m.verify_proof(path,{},contract,sdk64,sdk32)
   path.write_text(json.dumps(proof))
   for target in [log,compiler,product,sdk64/'axiom.wasm',sdk32/'axiom.wasm']:
    before=target.read_bytes();target.write_bytes(b'changed')
    with self.subTest(target=target),self.assertRaises(RuntimeError):m.verify_proof(path,{},contract,sdk64,sdk32)
    target.write_bytes(before)
   self.assertFalse((state/'resume-r3/window.json').exists())
if __name__=='__main__':unittest.main()
