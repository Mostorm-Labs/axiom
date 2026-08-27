"""GT-G1-02R leaf-golden production differential tests."""

from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
FIXTURE_TOOL = ROOT / "verification/fixture-author/compile_g1_02r_leaf_golden.py"
DIFFERENTIAL_TOOL = ROOT / "verification/tools/run_g1_02r_semantic_differential.py"
PROBE = Path(os.environ.get(
    "CANVAS_SEMANTIC_GOLDEN_PROBE",
    str(ROOT / "out/g1-02r-local/runtime/semantic/tools/canvas_semantic_golden_probe"),
))


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class G102RSemanticDifferentialTest(unittest.TestCase):
    def test_leaf_fixture_differential_compares_wire_and_semantic_projection(self) -> None:
        fixture_author = load_module("g1_02r_fixture_author_for_differential", FIXTURE_TOOL)
        differential = load_module("g1_02r_differential", DIFFERENTIAL_TOOL)
        self.assertTrue(PROBE.is_file(), "build canvas_semantic_golden_probe before running this test")
        with tempfile.TemporaryDirectory() as directory:
            corpus = Path(directory) / "semantic-v1"
            fixture_author.materialize(ROOT, corpus, compiler_commit="f" * 40)
            result = differential.run_differential(ROOT, PROBE, corpus)
        self.assertEqual("PASS", result["status"])
        self.assertEqual(23, result["caseCount"])
        self.assertIsNone(result["firstDivergence"])
        self.assertTrue(all(case["wireMatch"] and case["semanticMatch"] for case in result["cases"]))

    def test_first_divergence_identifies_wire_or_semantic_dimension(self) -> None:
        expected = {"accepted": True, "canonicalHex": "aa", "semanticProjection": {"scalarCount": 5}}
        actual = {"accepted": True, "canonicalHex": "aa", "semanticProjection": {"endScalar": 5}}
        differential = load_module("g1_02r_differential_for_divergence", DIFFERENTIAL_TOOL)
        divergence = differential.first_divergence("RTW-003", expected, actual)
        self.assertEqual({"caseId": "RTW-003", "dimension": "semanticProjection", "expected": {"scalarCount": 5}, "actual": {"endScalar": 5}}, divergence)


if __name__ == "__main__":
    unittest.main()
