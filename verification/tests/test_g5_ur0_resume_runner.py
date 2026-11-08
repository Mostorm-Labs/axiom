import copy,importlib.util,json,multiprocessing,tempfile,unittest
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
if __name__=='__main__':unittest.main()
