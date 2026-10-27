"""Authoring obligations, separate from any claim about FC08 product behavior."""
import importlib.util
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/fc08/reference'))
from fixtures import budget_recipe, cross_parent, nested, numeric_sample
from oracles import valid_forest

class CorpusContracts(unittest.TestCase):
    def test_numeric_poses_are_constructed_not_arbitrary_error_samples(self):
        self.assertTrue(numeric_sample(12)['quality_ok'])
        self.assertFalse(numeric_sample(24)['quality_ok'])

    def test_complete_size_recipes_are_real_encoded_bytes(self):
        for name in ['key-32','key-33','string-1048576','string-1048577','record-16777216','record-16777217',
                     'operation-33554431','operation-33554432','operation-33554433','restore-under-limit','restore-over-limit']:
            blobs,recipe=budget_recipe(name)
            self.assertTrue(all(len(b)>0 for _,_,b in blobs))
            if name.startswith(('record-','operation-')):self.assertEqual(len(blobs[0][2]),int(name.rsplit('-',1)[1]))
            if name.startswith('restore-'):self.assertEqual(len(blobs[1][2]),recipe['actual_restore_bytes'])

    def test_literal_preimages_and_fresh_ids(self):
        for fixture in [cross_parent(),nested()]:
            self.assertEqual(fixture['undo_after'],fixture['before'])
            self.assertEqual(len({fixture[x]['operation_id']['value'] for x in ['forward','inverse','redo']}),3)

if __name__=='__main__':unittest.main()
