"""P35/P36 geometry production-to-projection binding checks."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tools/check_g1_04_a_geometry_binding.py"


def load_checker():
    spec = importlib.util.spec_from_file_location("g1_04_geometry_binding", CHECKER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class GeometryProductionBindingTest(unittest.TestCase):
    def test_current_production_mapping_matches_projection(self):
        checker = load_checker()
        self.assertEqual([], checker.validate(ROOT))

    def test_dab_weight_drift_is_rejected(self):
        checker = load_checker()
        with tempfile.TemporaryDirectory() as directory:
            mutated = Path(directory) / "geometry_accounting_v1.hpp"
            source = (ROOT / "runtime/semantic/include/canvas/semantic/geometry_accounting_v1.hpp").read_text()
            mutated.write_text(source.replace("kDabInstance = 3", "kDabInstance = 2"), encoding="utf-8")
            self.assertTrue(checker.validate(ROOT, generated_header=mutated))

    def test_cubic_weight_drift_is_rejected(self):
        checker = load_checker()
        with tempfile.TemporaryDirectory() as directory:
            mutated = Path(directory) / "geometry_accounting_v1.hpp"
            source = (ROOT / "runtime/semantic/include/canvas/semantic/geometry_accounting_v1.hpp").read_text()
            mutated.write_text(source.replace("kCubicTo = 3", "kCubicTo = 2"), encoding="utf-8")
            self.assertTrue(checker.validate(ROOT, generated_header=mutated))


if __name__ == "__main__":
    unittest.main()
