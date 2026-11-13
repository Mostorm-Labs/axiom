from pathlib import Path
import importlib.util
import unittest
spec=importlib.util.spec_from_file_location('runner',Path(__file__).parents[1]/'runners/g5/cpu_worker_repair_runner.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class RepairGuard(unittest.TestCase):
 def request(self,name='G5CWR1-SCHED'):return {'id':name,'cases':[{'id':x}for x in m.ROSTERS[name]]}
 def test_roster_order_and_no_retry(self):
  ledger={'charged_seconds':0,'attempts':[]};m.reserve(ledger,self.request());self.assertEqual(ledger['attempts'][0]['limit_seconds'],180)
  for req in [self.request(),self.request('G5CWR1-LIFE'),{'id':'foreign','cases':[]}]:
   with self.assertRaises(ValueError):m.reserve(ledger,req)
 def test_invalid_case_and_exhausted_allowance(self):
  ledger={'charged_seconds':241,'attempts':[]}
  with self.assertRaises(ValueError):m.reserve(ledger,self.request())
  req=self.request();req['cases'][0]['id']='wrong'
  with self.assertRaises(ValueError):m.reserve({'charged_seconds':0,'attempts':[]},req)
 def test_hard_stop_blocks_remaining(self):
  ledger={'charged_seconds':10,'attempts':[{'id':'G5CWR1-SCHED','status':'STOPPED','hard_stop':True}]}
  with self.assertRaises(ValueError):m.reserve(ledger,self.request('G5CWR1-LIFE'))
if __name__=='__main__':unittest.main()
