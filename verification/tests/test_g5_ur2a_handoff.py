import importlib.util, unittest
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'runners/g5/ur2a_handoff_runner.py';s=importlib.util.spec_from_file_location('ur2a',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class Ledger(unittest.TestCase):
 def test_owned_processes_exclude_existing_browser_and_retain_observed_children(self):
  row=lambda parent,command:{'parent':parent,'command':command,'rss':1,'state':'S'}
  known={};m.observe_owned_processes({11:row(1,'node /driver /output/Q1'),12:row(11,'Chrome owned'),13:row(12,'GPU owned'),20:row(1,'Chrome existing')},'/driver','/output/Q1',known)
  self.assertEqual(set(known),{11,12,13})
  m.observe_owned_processes({13:row(1,'GPU owned'),20:row(1,'Chrome existing')},'/driver','/output/Q1',known)
  self.assertEqual(set(known),{11,12,13})
 def test_no_repeat_concurrent_or_foreign(self):
  l=m.new_ledger()
  with self.assertRaises(ValueError):m.reserve(l,'old')
  m.reserve(l,'UR2A2-Q1')
  with self.assertRaises(ValueError):m.reserve(l,'UR2A2-N1')
 def test_ceiling_and_history_are_preserved(self):
  l=m.new_ledger();m.reserve(l,'UR2A2-Q1');m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.01},{'status':'OBSERVED'})
  m.reserve(l,'UR2A2-N1');m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':2.01},{'status':'PRODUCT_FAILURE_OBSERVED'})
  self.assertEqual((l['cumulative_starts'],l['cumulative_charged_seconds'],l['remaining_status']),(62,2281,'NONE'))
 def test_guard_or_missing_result_stops_and_charges_reservation(self):
  for g,w in[(None,None),({'reason':'rss','cleanup_complete':False,'elapsed_s':1},None)]:
   l=m.new_ledger();m.reserve(l,'UR2A2-Q1');m.complete(l,g,w);self.assertEqual(l['remaining_status'],'MISSING');self.assertEqual(l['attempts'][0]['charged_seconds'],360)
   with self.assertRaises(ValueError):m.reserve(l,'UR2A2-N1')
 def test_cleanup_does_not_turn_hard_failure_into_pass(self):
  l=m.new_ledger();m.reserve(l,'UR2A2-Q1');m.complete(l,{'reason':'completed','cleanup_complete':True,'elapsed_s':3.1},{'status':'HARD_FAILURE'});self.assertEqual(l['remaining_status'],'MISSING')
if __name__=='__main__':unittest.main()
