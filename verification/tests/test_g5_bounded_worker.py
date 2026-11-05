import importlib.util,sys,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('bounded',Path(__file__).parents[1]/'runners/g5/bounded_worker.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class BoundedWorker(unittest.TestCase):
 def test_success_retains_output_and_returncode(self):
  with tempfile.TemporaryDirectory() as d:
   r=m.run([sys.executable,'-c','print(\'{"stage":"done","objects":7}\')'],Path(d)/'run',3,256*1024)
   self.assertEqual(r['reason'],'completed');self.assertEqual(r['returncode'],0)
   self.assertEqual(r['last_stage']['objects'],7)
 def test_timeout_keeps_partial_stage_and_stops_owned_child(self):
  with tempfile.TemporaryDirectory() as d:
   r=m.run([sys.executable,'-u','-c','import time;print(\'{"stage":"restore","objects":3}\');time.sleep(10)'],Path(d)/'run',.25,256*1024)
   self.assertEqual(r['reason'],'timeout');self.assertEqual(r['last_stage']['stage'],'restore')
   self.assertTrue(r['cleanup_complete'])
 def test_resource_cap_is_not_a_successful_partial_run(self):
  with tempfile.TemporaryDirectory() as d:
   r=m.run([sys.executable,'-u','-c','import time;print(\'{"stage":"loading"}\');x=bytearray(32*1024*1024);time.sleep(10)'],Path(d)/'run',3,16*1024)
   self.assertEqual(r['reason'],'rss_limit');self.assertTrue(r['cleanup_complete'])
if __name__=='__main__':unittest.main()
