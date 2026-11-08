import importlib.util
from pathlib import Path
import unittest

P=Path(__file__).resolve().parents[1]/'runners/g5/transient_attribution_runner.py'
spec=importlib.util.spec_from_file_location('attribution',P)
runner=importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)

class AttributionAccounting(unittest.TestCase):
    def test_product_failure_stays_red_and_next_independent_variant_can_run(self):
        ledger=runner.new_ledger()
        runner.reserve(ledger,runner.IDS[0])
        guard={'elapsed_s':3.01,'reason':'completed','cleanup_complete':True}
        runner.complete(ledger,guard,{'status':'PRODUCT_FAILURE_OBSERVED'})
        self.assertEqual(ledger['attempts'][0]['worker_status'],'PRODUCT_FAILURE_OBSERVED')
        self.assertEqual(ledger['attempts'][0]['charged_seconds'],4)
        runner.reserve(ledger,runner.IDS[1])
        with self.assertRaises(ValueError):runner.reserve(ledger,runner.IDS[1])

    def test_hard_guard_and_source_failure_close_window(self):
        for guard,worker in [(None,None),({'elapsed_s':180,'reason':'timeout','cleanup_complete':True},None),
                             ({'elapsed_s':3,'reason':'completed','cleanup_complete':False},{'status':'OBSERVED'}),
                             ({'elapsed_s':3,'reason':'completed','cleanup_complete':True},{'status':'HARD_FAILURE'})]:
            ledger=runner.new_ledger();runner.reserve(ledger,runner.IDS[0]);runner.complete(ledger,guard,worker)
            self.assertEqual(ledger['remaining_status'],'MISSING')
            with self.assertRaises(ValueError):runner.reserve(ledger,runner.IDS[1])

    def test_exact_five_ordered_attempts_and_history_not_reset(self):
        ledger=runner.new_ledger()
        self.assertEqual((ledger['historical_starts'],ledger['historical_charged_seconds']),(36,1304))
        for identity in runner.IDS:
            runner.reserve(ledger,identity);runner.complete(ledger,{'elapsed_s':1.1,'reason':'completed','cleanup_complete':True},{'status':'OBSERVED'})
        self.assertEqual(ledger['remaining_status'],'NONE')
        with self.assertRaises(ValueError):runner.reserve(ledger,runner.IDS[0])

class RestoreEnvelope(unittest.TestCase):
    def setUp(self):
        spec=importlib.util.spec_from_file_location('attribution_resume',P.with_name('transient_attribution_resume.py'))
        self.resume=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.resume)

    def test_explicit_issuer_allocates1_without_rewriting_source0(self):
        allocation=self.resume.custody_allocation({'S0':{'sha256':'a'*64,'state':{'documentId':'9'*32}}})
        self.assertEqual((allocation['counter_before'],allocation['binding']),('0','1'))
        source={'name':'S0','sha256':'a'*64,'state':{'documentId':'9'*32},'metadata':{'resourceBinding':'0','documentRevision':'16','checkpointDigest':[1],'recoveryFrontier':[8,1]}}
        envelope=self.resume.derive_envelope(source,allocation,{'family':1})
        self.assertEqual(source['metadata']['resourceBinding'],'0')
        self.assertEqual(envelope['metadata']['resourceBinding'],'1')
        self.assertEqual(envelope['source_capture_metadata'],source['metadata'])
        self.assertEqual({**envelope['metadata'],'resourceBinding':'0'},source['metadata'])

    def test_r2_has_only_four_new_ids_and_previous_accounting(self):
        ids=self.resume.IDS
        self.assertEqual(ids,('UR1ATTR2-A1','UR1ATTR2-B1','UR1ATTR2-B2','UR1ATTR2-A2'))
        ledger=runner.new_ledger(ids,38,1328)
        self.assertEqual((ledger['historical_starts'],ledger['historical_charged_seconds'],ledger['new_nominal_seconds_max']),(38,1328,720))
        self.assertEqual([runner.variant_for(i) for i in ids],['A','B','B','A'])
        with self.assertRaises(ValueError):runner.reserve(ledger,'UR1ATTR-SEED-A0')
        for identity in ids:
            runner.reserve(ledger,identity);runner.complete(ledger,{'elapsed_s':1,'reason':'completed','cleanup_complete':True},{'status':'OBSERVED'})
        with self.assertRaises(ValueError):runner.reserve(ledger,ids[0])

if __name__=='__main__':unittest.main()
