import importlib.util
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('sdk01_abi_scope',HERE/'validate_candidate_abi.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class CandidateAbiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root=HERE.parents[1]
        cls.base=(root/'docs/api/canvas_runtime_api_v1.h').read_text()
        cls.extension=(root/'docs/planning/sdk/candidate-multi-selection-v1.h').read_text()

    def test_base_and_extension_are_disjoint(self):
        self.assertEqual([],module.validate(self.base,self.extension))

    def test_missing_required_extension_symbol_fails(self):
        self.assertTrue(module.validate(self.base,self.extension.replace('canvas_sdk_multi_release_token(', 'canvas_sdk_multi_missing(')))

    def test_duplicate_and_v1_collision_fail(self):
        self.assertTrue(module.validate(self.base,self.extension+'\nCANVAS_RUNTIME_API CanvasStatus canvas_runtime_create(void);\n'))
        self.assertTrue(module.validate(self.base,self.extension+'\nCANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_release_token(void);\n'))

    def test_no_v1_header_is_rejected(self):
        self.assertTrue(module.validate(self.base,self.extension.replace('#include "canvas_runtime_api_v1.h"','')))

if __name__=='__main__':unittest.main()
