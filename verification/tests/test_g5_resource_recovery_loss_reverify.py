import importlib.util
import unittest
from pathlib import Path

P=Path(__file__).resolve().parents[1]/'runners/g5/resource_recovery_loss_reverify_runner.py'
spec=importlib.util.spec_from_file_location('loss_reverify',P)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def observed(**changes):
    return dict(reason='completed',returncode=0,cleanup_complete=True,remaining_owned_pids=[],elapsed_s=1.01,**changes)

class LossReverificationWindow(unittest.TestCase):
    def test_only_new_recovery_ids_and_original_cap(self):
        d=m.new_ledger()
        self.assertEqual(m.IDS,('UR1DLOSSR1-Q1','UR1DLOSSR1-N1'))
        self.assertEqual(m.LIMITS,(240,240))
        self.assertEqual((d['historical_starts'],d['historical_charged_seconds'],d['new_nominal_seconds_max'],d['original_cap_seconds']),(58,2234,480,8370))
    def test_serial_reservations_charge_ceil_and_cannot_restart(self):
        d=m.new_ledger()
        for identity in m.IDS:
            m.reserve(d,identity)
            with self.assertRaises(ValueError):m.reserve(d,identity)
            m.complete(d,observed(),{'status':'OBSERVED'})
        self.assertEqual((d['remaining_status'],d['cumulative_starts'],d['cumulative_charged_seconds']),('NONE',60,2238))
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[0])
    def test_unobserved_guard_keeps_reserved_ceiling(self):
        for guard in [None,{'elapsed_s':0.01}]:
            d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,guard,None)
            self.assertEqual(d['attempts'][0]['charged_seconds'],240)
            self.assertEqual(d['remaining_status'],'MISSING')
    def test_guard_cleanup_and_crash_stop_remaining_id(self):
        for guard,worker in [({**observed(),'reason':'timeout'},{'status':'OBSERVED'}),({**observed(),'cleanup_complete':False},{'status':'OBSERVED'}),({**observed(),'remaining_owned_pids':[12]},{'status':'OBSERVED'}),(observed(),{'status':'HARD_FAILURE'}),(observed(),{'status':'OBSERVED','crash':True})]:
            d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,guard,worker)
            self.assertEqual(d['remaining_status'],'MISSING')
            with self.assertRaises(ValueError):m.reserve(d,m.IDS[1])
    def test_safe_failed_classification_may_finish_normal_control_only(self):
        d=m.new_ledger();m.reserve(d,m.IDS[0])
        m.complete(d,observed(),{'status':'PRODUCT_FAILURE_OBSERVED','cases':[{'failures':['actual resource/context facts missing']}]})
        self.assertEqual(d['remaining_status'],'PENDING');m.reserve(d,m.IDS[1]);m.complete(d,observed(),{'status':'OBSERVED'})
        self.assertEqual(d['remaining_status'],'NONE');self.assertTrue(d['functional_failure_observed'])
    def test_false_receipt_and_invalid_retirement_stop_next_row(self):
        for case in [{'failures':['PRODUCT_CHECK: lost context did not create presentation receipt']},{'queues':{'status':'INVALID'},'failures':[]}]:
            d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,observed(),{'status':'PRODUCT_FAILURE_OBSERVED','cases':[case]})
            self.assertEqual(d['remaining_status'],'MISSING')
    def test_old_ids_and_reordered_start_never_consume_a_slot(self):
        d=m.new_ledger()
        for identity in ['UR1D-CORE-Q1','UR1D-REC-Q1',m.IDS[1]]:
            with self.assertRaises(ValueError):m.reserve(d,identity)
        self.assertEqual(d['attempts'],[])
