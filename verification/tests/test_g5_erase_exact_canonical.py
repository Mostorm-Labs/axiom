from pathlib import Path
import importlib.util
import unittest


class WindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(__file__).resolve().parents[1] / 'runners/g5/erase_exact_canonical_runner.py'
        spec = importlib.util.spec_from_file_location('ec_runner', path)
        cls.runner = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.runner)

    def test_unique_window_and_total_reservation(self):
        m = self.runner
        ledger = {'attempts': [], 'charged_seconds': 0}
        m.reserve(ledger, 'G5EC1-BASELINE')
        with self.assertRaises(ValueError):
            m.reserve(ledger, 'G5EC1-BASELINE')
        with self.assertRaises(ValueError):
            m.reserve(ledger, 'smoke')
        with self.assertRaises(ValueError):
            m.reserve({'attempts': [], 'charged_seconds': 800}, 'G5EC1-BASELINE')
        self.assertEqual(m.charge({'elapsed_s': 16.01}, 180), 17)
        self.assertEqual(m.charge({'elapsed_s': float('nan')}, 180), 180)

    def test_failed_guard_or_source_never_qualifies(self):
        m = self.runner
        guard = dict(reason='completed', returncode=0, cleanup_complete=True, remaining_owned_pids=[])
        worker = dict(status='OBSERVED', rows=[dict(status='QUALIFIED')])
        self.assertEqual(m.classify(guard, worker, True, 1), 'COMPLETED')
        self.assertEqual(m.classify(guard, worker, False, 1), 'STOPPED')
        self.assertEqual(m.classify(guard | dict(reason='timeout'), worker, True, 1), 'STOPPED')
        self.assertEqual(m.classify(guard, worker, True, 6), 'STOPPED')
