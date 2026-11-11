import importlib.util
from pathlib import Path
import unittest
class GuardTest(unittest.TestCase):
    def test_only_two_distinct_owned_windows_with_fixed_caps(self):
        p=Path(__file__).parents[1]/'runners/g5/exact_fixed_scene_runner.py'
        spec=importlib.util.spec_from_file_location('fs_guard',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
        ledger={'charged_seconds':0,'attempts':[]};m.reserve(ledger,'G5FS3-PILOT-PAIRS')
        self.assertEqual(ledger['attempts'][0]['limit_seconds'],900)
        for name in ['G5FS3-PILOT-PAIRS','G5FS3-ZOOM-TERMINAL','foreign']:
            with self.assertRaises(ValueError):m.reserve(ledger,name)
        ledger['attempts'][0]['status']='COMPLETED';ledger['charged_seconds']=899
        m.reserve(ledger,'G5FS3-ZOOM-TERMINAL');self.assertEqual(ledger['attempts'][1]['limit_seconds'],120)
