import importlib.util
from pathlib import Path
import unittest

P=Path(__file__).resolve().parents[1]/'runners/g5/eraser_readmission_runner.py'
spec=importlib.util.spec_from_file_location('post_up_runner',P)
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

class CandidateWindow(unittest.TestCase):
    def test_only_two_candidates_no_old_window_or_retry(self):
        ledger=runner.new_ledger()
        self.assertEqual(ledger['planned_ids'],['UR2R1-C1','UR2R1-C2'])
        self.assertEqual((ledger['historical_starts'],ledger['historical_charged_seconds']),(44,1433))
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR1ATTR2-A1')
        runner.reserve(ledger,'UR2R1-C1')
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR2R1-C2')
        runner.complete(ledger,{'reason':'completed','elapsed_s':1.2,'cleanup_complete':True},{'status':'OBSERVED'})
        self.assertEqual(ledger['attempts'][0]['charged_seconds'],2)
        runner.reserve(ledger,'UR2R1-C2');runner.complete(ledger,{'reason':'completed','elapsed_s':1,'cleanup_complete':True},{'status':'OBSERVED'})
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR2R1-C2')

    def test_guard_failure_stops_and_unknown_is_fully_charged(self):
        ledger=runner.new_ledger();runner.reserve(ledger,'UR2R1-C1');runner.complete(ledger,None,None)
        self.assertEqual(ledger['attempts'][0]['charged_seconds'],180)
        self.assertEqual(ledger['remaining_status'],'MISSING')
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR2R1-C2')

if __name__=='__main__':unittest.main()
