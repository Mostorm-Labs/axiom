import copy
import importlib.util
import json
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('sdk01_p20',HERE/'validate_p20_cases.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class P20CasesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((HERE/'p20-cases-v1.json').read_text(encoding='utf-8'))

    def test_case_structure_and_all_twelve_families(self):
        self.assertEqual([],module.validate(self.data,HERE.parents[1]))

    def test_missing_family_and_duplicate_id_are_rejected(self):
        value=copy.deepcopy(self.data)
        value['cases']=[c for c in value['cases'] if c['family']!='API12']
        value['case_count']=len(value['cases'])
        self.assertTrue(module.validate(value,HERE.parents[1]))
        value=copy.deepcopy(self.data);value['cases'][1]['id']=value['cases'][0]['id']
        self.assertTrue(module.validate(value,HERE.parents[1]))

    def test_missing_oracle_is_not_coverage(self):
        value=copy.deepcopy(self.data);value['cases'][0]['expected']=''
        self.assertTrue(module.validate(value,HERE.parents[1]))

    def test_nonexistent_fixture_fails_closed(self):
        value=copy.deepcopy(self.data);value['cases'][0]['source_anchor']='does/not/exist'
        self.assertTrue(module.validate(value,HERE.parents[1]))

    def test_draft_case_cannot_be_labeled_pass(self):
        value=copy.deepcopy(self.data);value['cases'][0]['planned_status']='PASS'
        self.assertTrue(module.validate(value,HERE.parents[1]))

if __name__=='__main__': unittest.main()
