"""GT-G1-02R leaf wire fixture-author trust-boundary tests."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "verification/fixture-author/compile_g1_02r_leaf_golden.py"
CASE_IDS = (
    "RTW-001", "RTW-002", "RTW-003", "RTW-004", "RTW-005", "RTW-006", "RTW-007", "RTW-008", "RTW-009",
    "STW-001", "STW-002", "STW-003", "STW-004", "STW-005", "STW-006", "STW-007",
    "RTW-N01", "RTW-N02", "STW-N01", "STW-N02", "STW-N03", "STW-N04", "STW-N05",
)


def fixture_author_module():
    spec = importlib.util.spec_from_file_location("g1_02r_leaf_fixture_author", TOOL)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class G102RLeafFixtureAuthorTest(unittest.TestCase):
    def test_authoring_catalog_has_every_required_refreeze_case_once(self) -> None:
        module = fixture_author_module()
        self.assertEqual([case["id"] for case in module.load_authoring_catalog(ROOT)], list(CASE_IDS))

    def test_fixture_author_is_verification_only(self) -> None:
        module = fixture_author_module()
        result = module.verify_independence(ROOT)
        self.assertTrue(result["ok"], result["violations"])

    def test_two_materializations_are_byte_identical_and_include_semantic_oracles(self) -> None:
        module = fixture_author_module()
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            first, second = temporary / "first", temporary / "second"
            first_manifest = module.materialize(ROOT, first, compiler_commit="f" * 40)
            second_manifest = module.materialize(ROOT, second, compiler_commit="f" * 40)
            self.assertEqual(first_manifest, second_manifest)
            first_files = sorted(path.relative_to(first) for path in first.rglob("*") if path.is_file())
            self.assertEqual(first_files, sorted(path.relative_to(second) for path in second.rglob("*") if path.is_file()))
            for relative in first_files:
                self.assertEqual((first / relative).read_bytes(), (second / relative).read_bytes(), relative)
            positive = first / "wire/g1-02r/richtext/RTW-003-delete-text-count"
            self.assertTrue((positive / "expected/canonical.pb").is_file())
            self.assertEqual(
                {"deltaVersion": 1, "steps": [{"kind": "DeleteText", "paragraphId": "000102030405060708090a0b0c0d0e0f", "startScalar": 3, "scalarCount": 5}]},
                json.loads((positive / "expected/semantic.projection.json").read_text(encoding="utf-8")),
            )
            negative = first / "wire/g1-02r/brush-stroke/STW-N05-legacy-seed-varint"
            self.assertEqual(
                {"accepted": False, "stage": "WIRE_PREFLIGHT", "category": "UNKNOWN_WIRE_FIELD"},
                json.loads((negative / "expected/outcome.json").read_text(encoding="utf-8")),
            )

    def test_independence_cli_does_not_require_production_runtime(self) -> None:
        result = subprocess.run(
            [sys.executable, str(TOOL), "--root", str(ROOT), "--verify-independence"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["ok"])


if __name__ == "__main__":
    unittest.main()
