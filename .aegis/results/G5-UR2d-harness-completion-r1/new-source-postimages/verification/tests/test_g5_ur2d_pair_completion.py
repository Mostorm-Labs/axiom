import importlib.util
from pathlib import Path
import tempfile
import unittest

P=Path(__file__).resolve().parents[1]/'runners/g5/ur2d_pair_completion_runner.py'
spec=importlib.util.spec_from_file_location('ur2d_pair_completion',P)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class CompletionTests(unittest.TestCase):
    def test_only_single_new_id_and_prior105_ceil_accounting(self):
        ledger=dict(attempts=[],charged_seconds=105)
        for name in ['U2D1-OLD-F','U2D1-COMPARE','smoke']:
            with self.assertRaises(ValueError):m.reserve(ledger,name)
        m.reserve(ledger,m.WINDOW)
        with self.assertRaises(ValueError):m.reserve(ledger,m.WINDOW)
        self.assertEqual(m.charge(dict(elapsed_s=51.940357)),52)
        for value in [None,-1,True,float('nan'),float('inf'),246]:
            self.assertEqual(m.charge(dict(elapsed_s=value)),240)
    def test_request_requires_missing_pairs_and_ordered_distinct_SDKs(self):
        with tempfile.TemporaryDirectory() as directory:
            a=Path(directory)/'old';b=Path(directory)/'candidate';a.mkdir();b.mkdir()
            request=dict(id=m.WINDOW,repetitions=[1,2],variants=[dict(name='old',sdk=str(a)),dict(name='candidate',sdk=str(b))],fixtures=[dict(name='S0'),dict(name='S2')])
            self.assertEqual(set(m.request_roots(request)),{'old','candidate'})
            for patch in [dict(id='U2D1-COMPARE'),dict(repetitions=[0,1,2]),dict(variants=list(reversed(request['variants']))),dict(fixtures=[])]:
                with self.assertRaises(ValueError):m.request_roots(request|patch)
    def test_completion_requires_all_four_runs_both_fixtures_and_original_pixel_proof(self):
        guard=dict(reason='completed',returncode=0,cleanup_complete=True,remaining_owned_pids=[])
        runs=[dict(repetition=i,variant=v,rows=[dict(fixture=f,errors=[],roundtrip=dict(status='QUALIFIED')) for f in ['S0','S2']]) for i in [1,2] for v in ['old','candidate']]
        worker=dict(status='OBSERVED',runs=runs)
        self.assertEqual(m.classify(guard,worker,True),'COMPLETED')
        for patch in [dict(reason='rss_limit'),dict(reason='timeout'),dict(returncode=1),dict(cleanup_complete=False),dict(remaining_owned_pids=[1])]:
            self.assertEqual(m.classify(guard|patch,worker,True),'STOPPED')
        self.assertEqual(m.classify(guard,worker,False),'STOPPED')
        self.assertEqual(m.classify(guard,worker|dict(runs=runs[:3]),True),'STOPPED')
        runs[0]['rows'][1]['roundtrip']['status']='FAILED'
        self.assertEqual(m.classify(guard,worker,True),'STOPPED')
    def test_prior_results_are_only_read_and_frozen_product_excludes_exact_two_authorized_files(self):
        source=P.read_text()
        self.assertIn("RESULT=ROOT/'.aegis/results/G5-UR2d-harness-completion-r1'",source)
        self.assertEqual(m.ALLOWED_CHANGED,{'verification/runners/g5/ur2d_selection_up.mjs','verification/tests/g5_ur2d_selection_up.test.mjs'})
        self.assertIn("if name not in ALLOWED_CHANGED",source)
        self.assertNotIn("write(parent/",source)

if __name__=='__main__':unittest.main()
