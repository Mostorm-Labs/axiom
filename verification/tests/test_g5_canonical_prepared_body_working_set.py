import importlib.util,unittest
from pathlib import Path
P=Path(__file__).resolve().parents[1]/'runners/g5/canonical_prepared_body_working_set_runner.py'
spec=importlib.util.spec_from_file_location('c2r1',P);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class WorkingSetRunner(unittest.TestCase):
    def test_new_window_retains_closed_history(self):
        d=m.new_ledger()
        self.assertEqual(m.IDS,('UR1C2R1-Q1','UR1C2R1-N1'))
        self.assertEqual(d['historical_charged_seconds'],2004)
        self.assertEqual(d['historical_starts'],52)
        self.assertEqual(d['original_cap_seconds'],8370)
    def test_source_includes_package_excludes_only_self_outputs(self):
        s=P.read_text()
        self.assertIn('.aegis/results/G5-UR1c2-canonical-prepared-body/',s)
        self.assertIn('g5-ur1c2-working-set-repair-r1/',s)
        self.assertNotIn('G5-UR1c1-prepared-body-index/\'',s.split('def source_identity')[1].split('def artifact_identity')[0])

    def test_no_duplicate_old_or_concurrent_slot(self):
        d=m.new_ledger()
        with self.assertRaises(ValueError):m.reserve(d,'UR1C2-Q1')
        m.reserve(d,m.IDS[0])
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[1])
        m.complete(d,{'reason':'completed','cleanup_complete':True,'elapsed_s':1.2},{'status':'OBSERVED'})
        self.assertEqual(d['attempts'][0]['charged_seconds'],2)
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[0])
        m.reserve(d,m.IDS[1]);m.complete(d,{'reason':'completed','cleanup_complete':True,'elapsed_s':2.01},{'status':'OBSERVED'})
        self.assertEqual(d['remaining_status'],'NONE')
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[1])
    def test_guard_stops_remaining(self):
        d=m.new_ledger();m.reserve(d,m.IDS[0]);m.complete(d,{'reason':'rss_limit','cleanup_complete':True,'elapsed_s':.1},None)
        self.assertEqual(d['remaining_status'],'MISSING')
        with self.assertRaises(ValueError):m.reserve(d,m.IDS[1])
