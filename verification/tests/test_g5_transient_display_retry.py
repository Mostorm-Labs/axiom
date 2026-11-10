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
R4=P.with_name('live_reference_alignment_runner.py');s4=importlib.util.spec_from_file_location('live_reference_runner',R4);r4=importlib.util.module_from_spec(s4);s4.loader.exec_module(r4)
class LiveReferenceWindow(unittest.TestCase):
 def observed(self):return {'status':'OBSERVED','cases':[{'status':'COMPLETED'}for _ in range(16)]}
 def test_one_normal_window_preserves_history_and_ceil_charge(self):
  l=r4.new_ledger();r4.reserve(l,'UR2CLP4-N1');self.assertEqual(l['attempts'][0]['reserved_seconds'],600);self.assertEqual(l['attempts'][0]['rss_limit_mib'],3072)
  self.assertTrue(r4.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.01},self.observed(),True));self.assertEqual(l['cumulative_starts'],91);self.assertEqual(l['cumulative_charged_seconds'],6253);self.assertEqual(l['remaining_seconds'],2117)
  with self.assertRaises(ValueError):r4.reserve(l,'UR2CLP4-N1')
 def test_old_ids_smoke_and_duplicate_window_are_rejected(self):
  for name in ['UR2CRB3-N1','UR2CLP4-S1','smoke']:
   with self.assertRaises(ValueError):r4.reserve(r4.new_ledger(),name)
  l=r4.new_ledger();r4.reserve(l,'UR2CLP4-N1')
  with self.assertRaises(ValueError):r4.reserve(l,'UR2CLP4-N1')
 def test_incomplete_cases_identity_cleanup_and_guard_fail_closed(self):
  for guard,worker,stable in [({'reason':'completed','cleanup_complete':True,'elapsed_s':3},{'status':'OBSERVED','cases':[]},True),({'reason':'rss_limit','cleanup_complete':True,'elapsed_s':3},self.observed(),True),({'reason':'completed','cleanup_complete':False,'elapsed_s':3},self.observed(),True),({'reason':'completed','cleanup_complete':True,'elapsed_s':3},self.observed(),False)]:
   l=r4.new_ledger();r4.reserve(l,'UR2CLP4-N1');self.assertFalse(r4.complete(l,guard,worker,stable));self.assertEqual(l['remaining_status'],'MISSING')
 def test_untrusted_elapsed_charges_full_reservation(self):
  for elapsed in [None,-1,float('nan'),float('inf'),False,99999]:
   l=r4.new_ledger();r4.reserve(l,'UR2CLP4-N1');r4.complete(l,{'reason':'timeout','cleanup_complete':False,'elapsed_s':elapsed},None,False);self.assertEqual(l['cumulative_charged_seconds'],6851);self.assertEqual(l['remaining_seconds'],1519)
 def test_cap_cannot_be_reset(self):
  l=r4.new_ledger();l['cumulative_charged_seconds']=8360
  with self.assertRaises(ValueError):r4.reserve(l,'UR2CLP4-N1')
if __name__=='__main__':unittest.main()
