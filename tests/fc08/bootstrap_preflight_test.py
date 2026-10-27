"""Unit tests for package auditing only; these are not Axiom/product RED tests."""
from copy import deepcopy
import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]

class BootstrapPreflightTests(unittest.TestCase):
    def tool(self):
        path=ROOT/'tools/fc08/bootstrap_preflight.py'
        self.assertTrue(path.is_file(),'bounded product-package preflight checker is missing')
        spec=importlib.util.spec_from_file_location('bootstrap_preflight',path)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        return module

    def test_frozen_shape_geometry_contradiction_is_rejected(self):
        tool=self.tool()
        scenario={'graph':{'b':{'kind':'Shape'}},'kind_geometry':{'b':'ellipse'},
                  'records':[{'id':{'value':'0000000000000000000000000000000a'},'kind_id':1,'content':{'shape':{'shape_kind':1}}}]}
        findings=tool.shape_conflicts(scenario,{'b':'0000000000000000000000000000000a'},{1:'rectangle',2:'ellipse'})
        self.assertEqual(findings[0]['object_name'],'b')
        self.assertEqual(findings[0]['canonical_geometry'],'rectangle')
        self.assertEqual(findings[0]['reference_geometry'],'ellipse')

    def test_consistent_scratch_geometry_is_not_rejected(self):
        tool=self.tool()
        scenario={'kind_geometry':{'b':'ellipse'},'records':[{'id':{'value':'b'},'kind_id':1,'content':{'shape':{'shape_kind':2}}}]}
        self.assertEqual(tool.shape_conflicts(scenario,{'b':'b'},{1:'rectangle',2:'ellipse'}),[])

    def test_wire_presence_is_not_an_applicability_contract(self):
        tool=self.tool()
        rows=[{'variant_id':'case/a','wire':[{'root':'Operation'}]}, {'variant_id':'case/b','wire':[]}]
        self.assertEqual(tool.explicit_profile_bindings(rows),{})

    def test_explicit_bindings_are_retained_without_guessing(self):
        tool=self.tool()
        value={'windows-native':'required','web-wasm':'required','headless':'not_applicable'}
        self.assertEqual(tool.explicit_profile_bindings([{'variant_id':'case/a','profiles':value}]),{'case/a':value})

    def test_package_path_cannot_escape_root(self):
        tool=self.tool()
        with self.assertRaises(ValueError):tool.safe_ref(ROOT,'../outside')

    def test_changed_frozen_bytes_are_not_accepted(self):
        tool=self.tool()
        self.assertFalse(tool.matches_digest(b'changed',tool.digest(b'frozen')))

if __name__=='__main__':unittest.main()
