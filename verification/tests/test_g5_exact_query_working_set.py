import importlib.util
from pathlib import Path
import unittest

class WindowGuardTest(unittest.TestCase):
    def test_frozen_windows_reject_repeat_foreign_and_concurrent(self):
        path=Path(__file__).parents[1]/'runners/g5/exact_query_working_set_runner.py'
        spec=importlib.util.spec_from_file_location('working_set_guard',path)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        ledger={'charged_seconds':0,'attempts':[]}
        module.reserve(ledger,'G5QW2-FINAL-PAIRS')
        self.assertEqual(ledger['attempts'][0]['limit_seconds'],720)
        for name in ['G5QW2-FINAL-PAIRS','G5QW2-CONTINUOUS','foreign']:
            with self.assertRaises(ValueError):module.reserve(ledger,name)
