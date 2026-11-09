import importlib.util
import unittest
from pathlib import Path

P=Path(__file__).resolve().parents[1]/'runners/g5/resource_recovery_runner.py'
spec=importlib.util.spec_from_file_location('ur1d',P)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ResourceWindow(unittest.TestCase):
    def test_exact_history_and_nominal_authority(self):
        d=m.new_ledger()
        self.assertEqual((d['historical_starts'],d['historical_charged_seconds']),(54,2107))
        self.assertEqual((sum(m.LIMITS),d['new_nominal_seconds_max'],d['original_cap_seconds']),(840,840,8370))
    def test_four_serial_slots_actual_ceil_and_no_restart(self):
        d=m.new_ledger()
        for i,identity in enumerate(m.IDS):
            m.reserve(d,identity)
            self.assertEqual(d['attempts'][-1]['reserved_seconds'],m.LIMITS[i])
            with self.assertRaises(ValueError):m.reserve(d,identity)
            m.complete(d,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.01},{'status':'OBSERVED'})
            self.assertEqual(d['attempts'][-1]['charged_seconds'],2)
        self.assertEqual(d['remaining_status'],'NONE')
        self.assertEqual(d['cumulative_charged_seconds'],2115)
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[0])
    def test_guard_stops_all_remaining_rows(self):
        for reason,cleanup in [('timeout',True),('rss_limit',True),('completed',False)]:
            d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,{'reason':reason,'cleanup_complete':cleanup,'elapsed_s':2.2},{'status':'OBSERVED'})
            self.assertEqual(d['remaining_status'],'MISSING')
            with self.assertRaises(ValueError):m.reserve(d,m.IDS[1])
    def test_recovery_missing_observation_never_uses_core_reservation(self):
        d=m.new_ledger()
        for identity in m.IDS[:2]:
            m.reserve(d,identity);m.complete(d,{'reason':'completed','cleanup_complete':True,'elapsed_s':1},{'status':'OBSERVED'})
        m.reserve(d,m.IDS[2]);m.complete(d,None,None)
        self.assertEqual(d['attempts'][-1]['charged_seconds'],240)
        self.assertEqual(d['remaining_status'],'MISSING')
    def test_missing_worker_or_identity_failure_stops(self):
        for worker in [None,{'status':'HARD_FAILURE','identity_failure':True}]:
            d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,{'reason':'completed','cleanup_complete':True,'elapsed_s':1},worker)
            self.assertEqual(d['remaining_status'],'MISSING')
