import importlib.util
from pathlib import Path
import unittest
class GuardTest(unittest.TestCase):
    def test_two_owned_windows_cannot_repeat_or_overlap(self):
        p=Path(__file__).parents[1]/'runners/g5/exact_owner_residency_runner.py';spec=importlib.util.spec_from_file_location('r4',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        ledger={'charged_seconds':0,'attempts':[]};m.reserve(ledger,'G5OR4-FINAL-COMPARE')
        self.assertEqual(ledger['attempts'][0]['limit_seconds'],900)
        for name in ['G5OR4-FINAL-COMPARE','G5OR4-CONTINUITY','foreign']:
            with self.assertRaises(ValueError):m.reserve(ledger,name)
