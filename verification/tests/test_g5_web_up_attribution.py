from pathlib import Path
import importlib.util, unittest
class GuardTests(unittest.TestCase):
 def test_exact_four_ids_and_no_duplicate(self):
  p=Path(__file__).resolve().parents[1]/'runners/g5/web_up_attribution_runner.py';s=importlib.util.spec_from_file_location('wa',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
  self.assertEqual(sum(v for k,v in m.LIMITS.items()if k!='G5WA1-DIAGNOSTIC-UP-ONLY-R1'),1080);self.assertEqual(m.LIMITS['G5WA1-DIAGNOSTIC-UP-ONLY-R1'],236);l=dict(attempts=[],charged_seconds=0);m.reserve(l,'G5WA1-BASELINE-WITNESS')
  for n in ['G5WA1-BASELINE-WITNESS','smoke']:
   with self.assertRaises(ValueError):m.reserve(l,n)
  with self.assertRaises(ValueError):m.reserve(dict(attempts=[],charged_seconds=1000),'G5WA1-OVERHEAD')
