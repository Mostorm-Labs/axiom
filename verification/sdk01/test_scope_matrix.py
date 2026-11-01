import copy
import importlib.util
import json
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('sdk01_scope_matrix',HERE/'validate_scope_matrix.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class ScopeMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header=(HERE.parents[1]/'docs/api/canvas_runtime_api_v1.h').read_text(encoding='utf-8')
        cls.matrix=json.loads((HERE/'scope-matrix-v1.json').read_text(encoding='utf-8'))

    def test_actual_header_has_complete_unique_classification(self):
        self.assertEqual([],module.validate(self.header,self.matrix))

    def test_missing_symbol_is_not_coverage(self):
        value=copy.deepcopy(self.matrix); value['functions'].pop()
        self.assertTrue(module.validate(self.header,value))

    def test_duplicate_and_extra_symbol_fail(self):
        value=copy.deepcopy(self.matrix); value['functions'].append(copy.deepcopy(value['functions'][0]))
        self.assertTrue(module.validate(self.header,value))
        value=copy.deepcopy(self.matrix); value['functions'][0]['symbol']='canvas_not_real'
        self.assertTrue(module.validate(self.header,value))

    def test_required_function_cannot_be_deferred_without_audit(self):
        value=copy.deepcopy(self.matrix)
        next(x for x in value['functions'] if x['symbol']=='canvas_runtime_create')['disposition']='deferred'
        self.assertTrue(module.validate(self.header,value))

    def test_future_features_cannot_claim_success(self):
        value=copy.deepcopy(self.matrix)
        next(x for x in value['functions'] if x['disposition']=='deferred')['unimplemented_behavior']='success'
        self.assertTrue(module.validate(self.header,value))

    def test_all_twelve_families_explicit(self):
        value=copy.deepcopy(self.matrix); value['families'].pop('API12')
        self.assertTrue(module.validate(self.header,value))

    def test_future_scope_needs_owner(self):
        value=copy.deepcopy(self.matrix); value['families']['API12']['future_owner']=''
        self.assertTrue(module.validate(self.header,value))

if __name__=='__main__': unittest.main()
