import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("projection", ROOT / "verification/tools/validate_geometry_accounting_projection.py")
assert SPEC and SPEC.loader
projection = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(projection)


class GeometryAccountingProjectionTest(unittest.TestCase):
    def test_projection_matches_authority_and_hard_limit(self):
        self.assertEqual(projection.validate(ROOT), [])


if __name__ == "__main__":
    unittest.main()
