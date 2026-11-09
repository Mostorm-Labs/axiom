import importlib.util
from pathlib import Path
import unittest

path=Path(__file__).resolve().parents[1]/'runners/g5/prepared_body_index_runner.py'
spec=importlib.util.spec_from_file_location('prepared_body_index_runner',path)
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

class FrozenWindow(unittest.TestCase):
    def test_history_and_exact_two_serial_ids(self):
        ledger=runner.new_ledger()
        self.assertEqual((ledger['historical_starts'],ledger['historical_charged_seconds']),(46,1527))
        self.assertEqual(runner.IDS,('UR1C1-Q1','UR1C1-N1'))
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR1C1-N1')
        for identity in runner.IDS:
            runner.reserve(ledger,identity)
            runner.complete(ledger,{'elapsed_s':1.1,'reason':'completed','cleanup_complete':True},{'status':'OBSERVED'})
        self.assertEqual(ledger['remaining_status'],'NONE')
        self.assertEqual([r['charged_seconds'] for r in ledger['attempts']],[2,2])
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR1C1-Q1')

    def test_guard_closes_remaining_and_product_failure_stays_recorded(self):
        ledger=runner.new_ledger();runner.reserve(ledger,'UR1C1-Q1')
        runner.complete(ledger,{'elapsed_s':180,'reason':'timeout','cleanup_complete':True},None)
        self.assertEqual(ledger['remaining_status'],'MISSING')
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR1C1-N1')
        ledger=runner.new_ledger();runner.reserve(ledger,'UR1C1-Q1')
        runner.complete(ledger,{'elapsed_s':2.2,'reason':'completed','cleanup_complete':True},{'status':'PRODUCT_FAILURE_OBSERVED'})
        self.assertEqual(ledger['attempts'][0]['worker_status'],'PRODUCT_FAILURE_OBSERVED')
        runner.reserve(ledger,'UR1C1-N1')

if __name__=='__main__':unittest.main()
