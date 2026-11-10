from pathlib import Path
import importlib.util
import unittest
import tempfile

P=Path(__file__).resolve().parents[1]/'runners/g5/ur2d_selection_up_runner.py'
spec=importlib.util.spec_from_file_location('ur2d_runner',P)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class LedgerTests(unittest.TestCase):
    def test_both_compare_sdk_roots_bound_and_foreign_variant_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            a=Path(directory)/'old';b=Path(directory)/'candidate';a.mkdir();b.mkdir()
            (a/'engine.wasm').write_bytes(b'old');(b/'engine.wasm').write_bytes(b'candidate')
            data=dict(id='U2D1-COMPARE',variants=[dict(name='old',sdk=str(a)),dict(name='candidate',sdk=str(b))])
            before=m.artifact_maps(data);(a/'engine.wasm').write_bytes(b'changed')
            self.assertNotEqual(m.artifact_maps(data),before)
            data['variants'][1]['name']='foreign'
            with self.assertRaises(ValueError):m.sdk_roots(data)
    def test_completion_requires_guard_worker_identity_and_cleanup(self):
        guard=dict(reason='completed',returncode=0,cleanup_complete=True,remaining_owned_pids=[])
        self.assertEqual(m.classify(guard,dict(status='OBSERVED'),True),'COMPLETED')
        for patch in [dict(reason='rss_limit'),dict(reason='timeout'),dict(returncode=1),dict(cleanup_complete=False),dict(remaining_owned_pids=[12])]:
            self.assertEqual(m.classify(guard|patch,dict(status='OBSERVED'),True),'STOPPED')
        for worker in [None,{},dict(status='FUNCTIONAL_OR_HARNESS_FAILURE')]:
            self.assertEqual(m.classify(guard,worker,True),'STOPPED')
        self.assertEqual(m.classify(guard,dict(status='OBSERVED'),False),'STOPPED')
    def test_elapsed_ceil_and_untrusted_full_reservation(self):
        self.assertEqual(m.charge(dict(elapsed_s=39.99468554195482)),40)
        for value in [None,True,-1,float('nan'),float('inf'),246]:
            self.assertEqual(m.charge(dict(elapsed_s=value)),240)
    def test_frozen_ids_no_repeat_or_concurrent_start(self):
        ledger=dict(attempts=[],charged_seconds=0)
        with self.assertRaises(ValueError):m.reserve(ledger,'smoke')
        m.reserve(ledger,m.IDS[0])
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[1])
        ledger['attempts'][0]['status']='COMPLETED'
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[0])
        ledger['charged_seconds']=800
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[1])
    def test_prior_ledger_is_read_only_and_failure_requires_control_continuation(self):
        ledger=dict(attempts=[dict(id=m.IDS[0],status='STOPPED')],charged_seconds=50)
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[1])
        source=P.read_text()
        self.assertIn("RESULT=ROOT/'.aegis/results/G5-UR2d-selection-up-continue-r1'",source)
        self.assertNotIn("RESULT=ROOT/'.aegis/results/G5-UR2d-selection-up-r1'",source)
    def test_compare_only_can_cross_closed_bf_with_exact_control_authorization(self):
        ledger=dict(attempts=[dict(id=m.IDS[0],status='COMPLETED'),dict(id=m.IDS[1],status='STOPPED')],charged_seconds=53)
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[2])
        ledger['compare_continuation']=dict(status='AUTHORIZED_SINGLE_REMAINING_COMPARE')
        m.reserve(ledger,m.IDS[2]);self.assertEqual(ledger['attempts'][1]['status'],'STOPPED')
        with self.assertRaises(ValueError):m.reserve(ledger,m.IDS[2])

if __name__=='__main__':unittest.main()
