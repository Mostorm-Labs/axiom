import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("geometry", ROOT / "verification/fixture-author/compile_geometry_accounting_boundary.py")
assert SPEC and SPEC.loader
geometry = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(geometry)


class GeometryAccountingBoundaryTest(unittest.TestCase):
    def test_independent_oracle_reproduces_all_boundary_totals(self):
        self.assertEqual(geometry.validate(ROOT), [])
        catalog = json.loads((ROOT / "verification/fixture-author/geometry_accounting_boundary_v1.json").read_text())
        self.assertEqual([case["expectedUnits"] for case in catalog["cases"]], [1_999_999, 2_000_000, 2_000_001])

    def test_fixture_does_not_import_production_runtime(self):
        result = geometry.verify_independence(ROOT)
        self.assertTrue(result["ok"], result)


if __name__ == "__main__":
    unittest.main()
