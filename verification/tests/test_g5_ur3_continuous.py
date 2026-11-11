import importlib.util
from pathlib import Path
import unittest
class GuardTest(unittest.TestCase):
    def test_frozen_primary_and_correctness_windows_only(self):
        p=Path(__file__).parents[1]/'runners/g5/ur3_continuous_runner.py';spec=importlib.util.spec_from_file_location('u3guard',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        ledger={'charged_seconds':0,'attempts':[]};m.reserve(ledger,'G5U3C1-PRIMARY');self.assertEqual(ledger['attempts'][0]['limit_seconds'],600)
        for name in ['G5U3C1-PRIMARY','G5U3C1-CORRECTNESS','foreign']:
            with self.assertRaises(ValueError):m.reserve(ledger,name)
