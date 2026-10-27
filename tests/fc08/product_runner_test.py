"""Independent result-state sensitivity tests, not product behavior evidence."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
SPEC=importlib.util.spec_from_file_location('product_runner',ROOT/'tools/fc08/product_runner.py')
RUNNER=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(RUNNER)


class ProductRunnerTest(unittest.TestCase):
    def test_real_positive_mismatch_is_fail(self):
        result=RUNNER.evaluate_observation([{'op':'equal','path':'accepted','expected':True}],{'accepted':False})
        self.assertEqual(result['result'],'FAIL')

    def test_missing_is_not_run(self):
        self.assertEqual(RUNNER.evaluate_observation([{'op':'equal','path':'history.size','expected':1}],{})['result'],'NOT_RUN')

    def test_negative_with_missing_zero_mutation_is_not_pass(self):
        predicates=[{'op':'equal','path':'accepted','expected':False},{'op':'unchanged','before':'before','after':'after'}]
        self.assertEqual(RUNNER.evaluate_observation(predicates,{'accepted':False})['result'],'NOT_RUN')

    def test_missing_field_cannot_hide_known_mismatch(self):
        predicates=[{'op':'equal','path':'history.size','expected':1},{'op':'equal','path':'accepted','expected':True}]
        self.assertEqual(RUNNER.evaluate_observation(predicates,{'accepted':False})['result'],'FAIL')

    def test_incorrect_temporary_trace_is_fail_even_correct_end(self):
        controls=json.loads((ROOT/'.aegis/materialized/FC-08/sources/accepted-seed/verification/fixtures/handoff-traces.json').read_text(encoding='utf-8'))
        self.assertEqual(RUNNER.evaluate_observation([{'op':'c01','path':'trace'}],{'trace':controls['C01']['bad_trace']})['result'],'FAIL')

    def test_unexpected_absent_positive_pass_is_design_defect(self):
        self.assertEqual(RUNNER.classify_red({'accepted':True,'normal_ingress_ok':True},0,True)['classification'],'VERIFICATION_DESIGN_DEFECT')

    def test_program_error_is_environment_not_red(self):
        self.assertEqual(RUNNER.classify_red({'accepted':False},3)['result'],'BLOCKED_ENVIRONMENT')

    def test_wrong_root_is_not_valid_red(self):
        result=RUNNER.classify_red({'accepted':False,'category':'unsupported-root'},0,True)
        self.assertFalse(result['valid_product_RED'])

    def test_normal_codec_feature_rejection_is_red(self):
        result=RUNNER.classify_red({'accepted':False,'normal_ingress_ok':False,'category':'unsupported-operation'},0,True)
        self.assertTrue(result['valid_product_RED'])

    def test_no_field_from_expected_is_added_to_observation(self):
        observed={'accepted':False};before=observed.copy()
        RUNNER.evaluate_observation([{'op':'equal','path':'canonical_commit_delta','expected':0}],observed)
        self.assertEqual(observed,before)

    def test_normal_ingress_acceptance_cannot_be_called_red(self):
        result=RUNNER.classify_red({'accepted':False,'normal_ingress_ok':True},0,True)
        self.assertFalse(result['valid_product_RED'])

    def test_unverified_malformed_fixture_is_not_red(self):
        self.assertFalse(RUNNER.classify_red({'accepted':False,'category':'MALFORMED_WIRE'},0)['valid_product_RED'])

    def test_snapshot_red_uses_normal_ingress_not_golden_acceptance(self):
        result=RUNNER.classify_red({'accepted':True,'normal_ingress_ok':False,'semantic_error':8},0,True)
        self.assertTrue(result['valid_product_RED'])

    def test_ingress_predicate_reads_actual_normal_decoder(self):
        observed=RUNNER.observation_for({'mode':'Ingress'},{'accepted':True,'normal_ingress_ok':False},{})
        self.assertFalse(observed['accepted'])

    def test_legacy_record_comparison_ignores_observer_envelope_ids(self):
        snapshot=json.loads((ROOT/'tests/fc08/corpus/wire/legacy-snapshot1.input.json').read_text(encoding='utf-8'))['value']
        obj=snapshot['objects'][0]
        carrier={'operation_id':{'value':'ff'*16},'document_id':{'value':'ee'*16},'schema_version':1,'payload_version':1,
                 'payload':{'insert_objects':{'objects':[obj]}}}
        actual={'ok':True,'records':[RUNNER.Writer().encode('Operation',carrier).hex()]}
        result=RUNNER.observation_for({'mode':'LegacyImport'},actual,{'wire_model':snapshot})
        self.assertTrue(result['legacy_fields_preserved'])

    def test_model_writer_is_not_replaced_by_wire_decoder(self):
        self.assertIsNone(RUNNER.baseline_observation_mode({'direction':'model-to-canonical-writer'},{'root':'Operation'}))

    def test_requested_capability_reader_is_not_replaced_by_legacy_reader(self):
        self.assertIsNone(RUNNER.baseline_observation_mode({'runtime_capabilities':['section-v1']},{'root':'DocumentSnapshot'}))

    def test_explicit_legacy_import_uses_actual_bootstrap(self):
        self.assertEqual(RUNNER.baseline_observation_mode({'explicit_legacy_import':True},{'root':'DocumentSnapshot'}),'LegacyImport')

    def test_stateful_applicability_is_not_replaced_by_context_free_codec(self):
        self.assertIsNone(RUNNER.baseline_observation_mode({'before':[{'kind_id':10}],'direction':'normal-wire-ingress'},{'root':'Operation'}))


if __name__=='__main__':unittest.main()
