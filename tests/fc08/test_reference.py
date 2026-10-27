from pathlib import Path
import importlib.util
import json
import unittest

ROOT = Path(__file__).resolve().parents[2]
SEED = ROOT / '.aegis/materialized/FC-08/sources/accepted-seed/verification'


class FixedReferenceControls(unittest.TestCase):
    def reference(self):
        path = ROOT / 'tests/fc08/reference/oracles.py'
        self.assertTrue(path.exists(), 'independent predicates not implemented')
        spec = importlib.util.spec_from_file_location('fc08_oracles', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_r02_checks_each_boundary_and_requires_completion(self):
        ref = self.reference()
        controls = json.loads((SEED/'temporal-controls-CR01.json').read_text())
        self.assertIsNone(ref.c01(controls['C01']['good_trace']))
        self.assertEqual(ref.c01(controls['C01']['bad_trace'])['first_failure'], 1)
        self.assertIsNotNone(ref.c01([]))
        self.assertIsNotNone(ref.c01([{'unavailable':True}]))
        self.assertEqual(ref.c02(controls['C02']['bad_trace'])['first_failure'], 2)
        good = [dict(e) for e in controls['C02']['bad_trace']]
        good[2]['selected'] = ['T']
        self.assertIsNone(ref.c02(good))

    def test_literal_affine_and_concave_geometry(self):
        ref = self.reference()
        f = json.loads((SEED/'fixtures/affine-reparent.json').read_text())
        self.assertEqual(ref.compose(f['before']['G']['local'],f['before']['S']['local']),f['after']['S']['local'])
        self.assertEqual(ref.point(f['after']['S']['local'],[3,4]),[82,76])
        f = json.loads((SEED/'fixtures/concave-region.json').read_text())
        self.assertTrue(all(ref.inside(p,f['polygon_even_odd']) for p in [(0,0),(100,0),(100,100),(0,100)]))
        self.assertFalse(ref.rect_contained(f['section_boundary'],f['polygon_even_odd']))
        self.assertTrue(ref.inside([10,10], f['polygon_even_odd']))
        self.assertFalse(ref.inside([50,50], f['polygon_even_odd']))


if __name__ == '__main__':
    unittest.main()
