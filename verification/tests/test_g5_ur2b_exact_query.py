import importlib.util
from pathlib import Path
import unittest

P = Path(__file__).resolve().parents[1] / 'runners/g5/ur2b_exact_query_runner.py'
S = importlib.util.spec_from_file_location('ur2b', P)
M = importlib.util.module_from_spec(S)
S.loader.exec_module(M)


class ClosedAllowance(unittest.TestCase):
    def test_no_retry_or_concurrent_start(self):
        v = M.new_ledger()
        M.reserve(v, M.IDS[0])
        with self.assertRaises(ValueError):
            M.reserve(v, M.IDS[0])
        with self.assertRaises(ValueError):
            M.reserve(v, M.IDS[1])

    def test_actual_ceil_and_history(self):
        v = M.new_ledger()
        M.reserve(v, M.IDS[0])
        self.assertTrue(M.complete(v, {'reason': 'completed', 'cleanup_complete': True, 'elapsed_s': 1.01}, {'status': 'OBSERVED'}, True))
        self.assertEqual(v['cumulative_charged_seconds'], 3120)
        self.assertEqual(v['cumulative_starts'], 65)

    def test_product_failure_or_identity_loss_stops_remaining(self):
        for status, stable in [('PRODUCT_FAILURE_OBSERVED', True), ('OBSERVED', False)]:
            v = M.new_ledger()
            M.reserve(v, M.IDS[0])
            self.assertFalse(M.complete(v, {'reason': 'completed', 'cleanup_complete': True, 'elapsed_s': 3.2}, {'status': status}, stable))
            self.assertEqual(v['remaining_status'], 'MISSING')
            with self.assertRaises(ValueError):
                M.reserve(v, M.IDS[1])

    def test_exact_eight_order_and_limits(self):
        v = M.new_ledger()
        for i, id in enumerate(M.IDS):
            M.reserve(v, id)
            self.assertEqual(v['attempts'][-1]['reserved_seconds'], 360 if i < 2 else 180)
            self.assertTrue(M.complete(v, {'reason': 'completed', 'cleanup_complete': True, 'elapsed_s': 1}, {'status': 'OBSERVED'}, True))
        self.assertEqual(v['remaining_status'], 'NONE')
        self.assertEqual(v['cumulative_starts'], 72)


if __name__ == '__main__':
    unittest.main()
