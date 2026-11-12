import unittest,importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('complete',Path(__file__).resolve().parents[1]/'runners/g5/cpu_complete_runner.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class CompleteLedger(unittest.TestCase):
 def ledger(self):return {'diagnostics':{'max_starts':6,'cap_seconds':900,'charged_seconds':185,'attempts':[{'id':f'G5CWC-D{i}','status':'COMPLETED'}for i in range(1,6)]},'final':{'max_starts':3,'cap_seconds':720,'charged_seconds':0,'attempts':[]}}
 def test_last_diagnostic_cannot_repeat_or_add_seventh(self):
  l=self.ledger();q={'id':'G5CWC-D6','cases':[{'id':'dpr4-focused-fault'}]};m.admit(l,q);l['diagnostics']['attempts'][-1]['status']='COMPLETED'
  with self.assertRaises(ValueError):m.admit(l,q)
 def test_finals_order_exact_roster_and_stop(self):
  l=self.ledger();q={'id':'G5CWC-PAIRS','cases':[{'id':v}for v in m.ROSTERS['G5CWC-PAIRS']]};m.admit(l,q);l['final']['attempts'][-1].update(status='STOPPED',hard_stop=True)
  with self.assertRaises(ValueError):m.admit(l,{'id':'G5CWC-MIXED','cases':[{'id':'mixed-all-dpr2'}]})
 def test_no_full_limit_silently_borrowed_from_old_runs(self):
  l=self.ledger();l['final']['charged_seconds']=421
  with self.assertRaises(ValueError):m.admit(l,{'id':'G5CWC-PAIRS','cases':[{'id':v}for v in m.ROSTERS['G5CWC-PAIRS']]})
if __name__=='__main__':unittest.main()
