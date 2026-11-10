from pathlib import Path
import importlib.util
import unittest


class ReadinessTests(unittest.TestCase):
    def test_additional_window_is_single_bounded_with_shared_cap(self):
        path = Path(__file__).resolve().parents[1] / 'runners/g5/post_erase_interaction_readiness_runner.py'
        spec = importlib.util.spec_from_file_location('ready', path)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        for used in [0, 72, 900]:
            self.assertTrue(m.admit([], used))
        self.assertFalse(m.admit([{'id': 'G5EC1-READINESS-ALL'}], 72))
        self.assertFalse(m.admit([], 901))
