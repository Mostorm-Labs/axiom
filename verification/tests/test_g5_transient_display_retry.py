import importlib.util,unittest,copy
from pathlib import Path
P=Path(__file__).resolve().parents[1]/'runners/g5/transient_display_retry_runner.py';spec=importlib.util.spec_from_file_location('display_runner',P);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class DisplayLedger(unittest.TestCase):
 def test_exact_windows_and_limits(self):
  self.assertEqual(m.IDS,('UR2CRB3-S1','UR2CRB3-N1'));l=m.new_ledger();self.assertEqual(l['cumulative_starts'],88);self.assertEqual(l['cumulative_charged_seconds'],5907);m.reserve(l,m.IDS[0]);self.assertEqual(l['attempts'][0]['reserved_seconds'],120)
  self.assertTrue(m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.01},{'status':'OBSERVED'},True));self.assertEqual(l['cumulative_charged_seconds'],5909)
  m.reserve(l,m.IDS[1]);self.assertEqual(l['attempts'][1]['reserved_seconds'],600)
  self.assertTrue(m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':2.5},{'status':'OBSERVED'},True));self.assertEqual(l['remaining_status'],'NONE')
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[0])
 def test_serial_once_only(self):
  for first in ['smoke',m.IDS[1]]:
   with self.assertRaises(ValueError):m.reserve(m.new_ledger(),first)
  l=m.new_ledger();m.reserve(l,m.IDS[0])
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[0])
 def test_all_failure_kinds_stop_next_window(self):
  for guard,worker,stable in [(None,None,False),({'reason':'rss_limit','elapsed_s':3,'cleanup_complete':True},{'status':'OBSERVED'},True),({'reason':'completed','elapsed_s':3,'cleanup_complete':True},{'status':'PRODUCT_FAILURE_OBSERVED'},True),({'reason':'completed','elapsed_s':3,'cleanup_complete':False},{'status':'OBSERVED'},True),({'reason':'completed','elapsed_s':3,'cleanup_complete':True},{'status':'OBSERVED'},False)]:
   l=m.new_ledger();m.reserve(l,m.IDS[0]);self.assertFalse(m.complete(l,guard,worker,stable));self.assertEqual(l['remaining_status'],'MISSING')
   with self.assertRaises(ValueError):m.reserve(l,m.IDS[1])
 def test_missing_or_untrusted_elapsed_charges_full(self):
  for value in [None,-1,float('nan'),float('inf'),False,99999]:
   l=m.new_ledger();m.reserve(l,m.IDS[0]);m.complete(l,{'reason':'completed','elapsed_s':value,'cleanup_complete':False},None,False);self.assertEqual(l['attempts'][0]['charged_seconds'],120)
 def test_historical_cap_cannot_be_reset(self):
  l=m.new_ledger();l['cumulative_charged_seconds']=8360
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[0])
if __name__=='__main__':unittest.main()
