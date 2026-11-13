import importlib.util,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('runner',Path(__file__).parents[1]/'runners/g5/cpu_pending_runner.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Guard(unittest.TestCase):
 def request(self,n='G5CPUW2-PAIRS'):return dict(id=n,cases=[dict(id=x)for x in m.ROSTERS[n]])
 def test_frozen_order_no_retry(self):
  l=dict(charged_seconds=0,attempts=[]);m.reserve(l,self.request());self.assertEqual(l['attempts'][0]['limit_seconds'],300)
  for req in [self.request(),self.request('G5CPUW2-FEEDBACK')]:
   with self.assertRaises(ValueError):m.reserve(l,req)
 def test_cap_hardstop(self):
  for l in [dict(charged_seconds=301,attempts=[]),dict(charged_seconds=10,attempts=[dict(id='G5CPUW2-PAIRS',status='STOPPED',hard_stop=True)])]:
   with self.assertRaises(ValueError):m.reserve(l,self.request('G5CPUW2-FEEDBACK'))
if __name__=='__main__':unittest.main()
