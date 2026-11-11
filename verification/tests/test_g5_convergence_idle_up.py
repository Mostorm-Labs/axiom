from pathlib import Path
import importlib.util
import unittest
class ConvergenceGuards(unittest.TestCase):
 def test_bound_windows_and_no_concurrent_or_repeated_start(self):
  p=Path(__file__).parents[1]/'runners/g5/convergence_idle_up_runner.py';s=importlib.util.spec_from_file_location('cv',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
  ledger={'charged_seconds':0,'attempts':[]};m.reserve(ledger,'G5CV1-IDLE-CONTEXT');self.assertEqual(ledger['attempts'][0]['limit_seconds'],240)
  for name in ['G5CV1-IDLE-CONTEXT','G5CV1-UP-NORMAL','unknown']:
   with self.assertRaises(ValueError):m.reserve(ledger,name)
