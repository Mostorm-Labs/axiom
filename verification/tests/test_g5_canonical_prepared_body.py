import importlib.util,tempfile,subprocess,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('c2',ROOT/'verification/runners/g5/canonical_prepared_body_runner.py');c2=importlib.util.module_from_spec(spec);spec.loader.exec_module(c2)
class Window(unittest.TestCase):
 def test_exact_serial_window_and_ceil(self):
  ledger=c2.new_ledger();self.assertEqual(ledger['historical_starts'],50);self.assertEqual(ledger['historical_charged_seconds'],1881)
  c2.reserve(ledger,'UR1C2-Q1')
  with self.assertRaises(ValueError):c2.reserve(ledger,'UR1C2-N1')
  c2.complete(ledger,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.01},{'status':'OBSERVED'})
  self.assertEqual(ledger['attempts'][0]['charged_seconds'],2)
  c2.reserve(ledger,'UR1C2-N1');c2.complete(ledger,{'reason':'completed','cleanup_complete':True,'elapsed_s':.01},{'status':'PRODUCT_FAILURE_OBSERVED'})
  self.assertEqual(ledger['remaining_status'],'NONE')
  with self.assertRaises(ValueError):c2.reserve(ledger,'UR1C2-N1')
 def test_guard_or_cleanup_failure_closes_remaining(self):
  for guard in [{'reason':'timeout','cleanup_complete':True,'elapsed_s':180.1},{'reason':'completed','cleanup_complete':False,'elapsed_s':2}]:
   ledger=c2.new_ledger();c2.reserve(ledger,'UR1C2-Q1');c2.complete(ledger,guard,{'status':'OBSERVED'})
   self.assertEqual(ledger['remaining_status'],'MISSING')
   with self.assertRaises(ValueError):c2.reserve(ledger,'UR1C2-N1')
 def test_only_new_self_outputs_are_excluded(self):
  with tempfile.TemporaryDirectory() as d:
   r=Path(d);subprocess.run(['git','init','-q',d],check=True)
   old=r/'.aegis/results/G5-UR1c1-web-consumer-reverify-r1';old.mkdir(parents=True);(old/'proof').write_text('accepted')
   new=r/'.aegis/results/G5-UR1c2-canonical-prepared-body';new.mkdir(parents=True);(new/'proof').write_text('one')
   (r/'product.cpp').write_text('one');a=c2.source_identity(root=r)
   (new/'proof').write_text('two');self.assertEqual(a['source_tree_sha256'],c2.source_identity(root=r)['source_tree_sha256'])
   (old/'proof').write_text('changed');self.assertNotEqual(a['source_tree_sha256'],c2.source_identity(root=r)['source_tree_sha256'])
if __name__=='__main__':unittest.main()
