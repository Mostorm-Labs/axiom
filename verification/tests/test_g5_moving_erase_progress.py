import importlib.util,unittest
from pathlib import Path
P=Path(__file__).resolve().parents[1]/'runners/g5/moving_erase_progress_runner.py';s=importlib.util.spec_from_file_location('moving_runner',P);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class MovingLedger(unittest.TestCase):
 def test_serial_four_windows_and_exact_ceil_accounting(self):
  l=m.new_ledger()
  for i,window in enumerate(m.IDS):
   m.reserve(l,window);self.assertEqual(l['attempts'][-1]['reserved_seconds'],[120,240,360,600][i]);self.assertTrue(m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.1},{'status':'EXPECTED_RED_OBSERVED'if i==0 else'OBSERVED'},True))
  self.assertEqual(l['charged_seconds'],8);self.assertEqual(l['remaining_status'],'NONE')
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[0])
 def test_unplanned_old_parallel_and_failed_id_cannot_restart(self):
  for name in ['smoke','UR2CLP4-N1',m.IDS[1]]:
   with self.assertRaises(ValueError):m.reserve(m.new_ledger(),name)
  l=m.new_ledger();m.reserve(l,m.IDS[0])
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[0])
  m.complete(l,{'reason':'rss_limit','cleanup_complete':True,'elapsed_s':2},None,True)
  with self.assertRaises(ValueError):m.reserve(l,m.IDS[1])
 def test_unknown_time_charges_reservation_and_old_red_must_be_red(self):
  for elapsed in [None,False,-1,float('nan'),10000]:
   l=m.new_ledger();m.reserve(l,m.IDS[0]);self.assertFalse(m.complete(l,{'reason':'timeout','elapsed_s':elapsed,'cleanup_complete':False},None,False));self.assertEqual(l['charged_seconds'],120)
  l=m.new_ledger();m.reserve(l,m.IDS[0]);self.assertFalse(m.complete(l,{'reason':'completed','elapsed_s':1,'cleanup_complete':True},{'status':'OBSERVED'},True))
 def test_identity_and_cleanup_are_required(self):
  for clean,stable in [(False,True),(True,False)]:
   l=m.new_ledger();m.reserve(l,m.IDS[0]);self.assertFalse(m.complete(l,{'reason':'completed','cleanup_complete':clean,'elapsed_s':1},{'status':'EXPECTED_RED_OBSERVED'},stable))
if __name__=='__main__':unittest.main()
