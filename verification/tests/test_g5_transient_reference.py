import importlib.util
from pathlib import Path
import unittest

P = Path(__file__).resolve().parents[1] / 'runners/g5/transient_reference_runner.py'
spec = importlib.util.spec_from_file_location('transient_runner', P)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class LocalAccounting(unittest.TestCase):
    def test_attempt_order_is_abba_and_closed_attempts_cannot_be_retried(self):
        ledger = runner.new_ledger()
        runner.reserve(ledger, 'UR1A-A1')
        self.assertEqual(ledger['attempts'][0]['reserved_seconds'], 180)
        with self.assertRaises(ValueError):
            runner.reserve(ledger, 'UR1A-B1')
        runner.complete(ledger, {'elapsed_s': 3.01, 'reason': 'completed', 'cleanup_complete': True})
        self.assertEqual(ledger['attempts'][0]['charged_seconds'], 4)
        with self.assertRaises(ValueError):
            runner.reserve(ledger, 'UR1A-A1')
        runner.reserve(ledger, 'UR1A-B1')
        self.assertEqual(ledger['historical_charged_seconds'], 1276)

    def test_interrupted_worker_is_fully_charged_and_remaining_rows_are_missing(self):
        ledger = runner.new_ledger()
        runner.reserve(ledger, 'UR1A-A1')
        runner.complete(ledger, None)
        self.assertEqual(ledger['attempts'][0]['charged_seconds'], 180)
        self.assertEqual(ledger['remaining_status'], 'MISSING')
        with self.assertRaises(ValueError):
            runner.reserve(ledger, 'UR1A-B1')


if __name__ == '__main__':
    unittest.main()
