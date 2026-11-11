from pathlib import Path
import importlib.util
import unittest
class HistoryGuard(unittest.TestCase):
 def test_three_frozen_starts_and_no_retry(self):
  p=Path(__file__).parents[1]/'runners/g5/exact_history_path_runner.py';s=importlib.util.spec_from_file_location('hp',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
  l={'charged_seconds':0,'attempts':[]};m.reserve(l,'G5HP1-PAIRS');self.assertEqual(l['attempts'][0]['limit_seconds'],480)
  for name in ['G5HP1-PAIRS','G5HP1-DIAGNOSTIC','unknown']:
   with self.assertRaises(ValueError):m.reserve(l,name)
