"""Negative controls for the input/profile audit; not Axiom product tests."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('amendment_audit', ROOT / 'tools/fc08/audit_input_amendment.py')
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)
NEW = '.aegis/materialized/FC-08/attempts/v0.3-control-remediation-02'
OLD = '.aegis/materialized/FC-08/attempts/v0.2-obs01-corpus-bound'


class AmendmentAuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.old = AUDIT.load(ROOT, OLD + '/corpus-manifest.json')['variants']
        cls.ids = AUDIT.load(ROOT, AUDIT.SEED + '/fixtures/identity-map.json')

    def test_candidate02_independent_audit(self):
        result = AUDIT.audit(ROOT)
        self.assertEqual(result['counts']['corrected_inputs'], 16)
        self.assertEqual(result['SUT_invocations'], [])
        self.assertFalse(result['authority_acceptance'])

    def test_candidate01_exposes_empty_group_mismatch(self):
        with self.assertRaisesRegex(AssertionError, 'FC08-S05/empty-Group-paint: Selection graph'):
            AUDIT.audit(ROOT, '.aegis/packages/FC-08-P20-P31-INPUT-AMENDMENT-v0.1/package.json')

    def test_original_ellipse_disagreement_detected(self):
        row = next(r for r in self.old if r['case_id'] == 'FC08-S02')
        with self.assertRaises(AssertionError):
            AUDIT.geometry_agrees(AUDIT.load(ROOT, row['input']['path']), self.ids)

    def test_original_empty_group_disagreement_detected(self):
        row = next(r for r in self.old if r['variant_id'] == 'FC08-S05/empty-Group-paint')
        with self.assertRaisesRegex(AssertionError, 'different objects'):
            AUDIT.geometry_agrees(AUDIT.load(ROOT, row['input']['path']), self.ids)

    def test_duplicate_matrix_variant_rejected(self):
        original_load = AUDIT.load
        def corrupted(root, path):
            value = original_load(root, path)
            if path == NEW + '/profile-applicability.json':
                value['variants'][1] = deepcopy(value['variants'][0])
            return value
        with patch.object(AUDIT, 'load', side_effect=corrupted):
            with self.assertRaises(AssertionError):
                AUDIT.audit(ROOT)

    def test_schema_distinguishes_blocked_environment_from_not_applicable(self):
        schema = AUDIT.load(ROOT, NEW + '/profile-applicability.schema.json')
        matrix = AUDIT.load(ROOT, NEW + '/profile-applicability.json')
        cell = matrix['variants'][0]['profiles']['web-wasm']
        cell.update(result='BLOCKED_ENVIRONMENT', environment='MISSING')
        jsonschema.validate(matrix, schema)
        cell['applicability'] = 'NOT_APPLICABLE'
        with self.assertRaises(jsonschema.ValidationError):
            jsonschema.validate(matrix, schema)

    def test_missing_profile_rejected(self):
        schema = AUDIT.load(ROOT, NEW + '/profile-applicability.schema.json')
        matrix = AUDIT.load(ROOT, NEW + '/profile-applicability.json')
        del matrix['variants'][0]['profiles']['headless']
        with self.assertRaises(jsonschema.ValidationError):
            jsonschema.validate(matrix, schema)

    def test_full_command_budget_not_substituted_with_raw_size(self):
        results = []
        for row in self.old:
            if row['case_id'] == 'FC08-H04':
                results.append(AUDIT.headless_expected(row, AUDIT.load(ROOT, row['input']['path']), AUDIT.load(ROOT, row['expected']['path'])))
        self.assertEqual(results, [False,False,False,True,True,True,True,True,True,True,True])


if __name__ == '__main__':
    unittest.main()
