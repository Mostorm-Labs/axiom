"""GT-G1-02 verification-only canonical codec fixture compiler tests."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "verification/fixture-author/compile_codec_golden.py"


def fixture_author_module():
    spec = importlib.util.spec_from_file_location("codec_fixture_author", TOOL)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class CodecFixtureAuthorTest(unittest.TestCase):
    def test_independence_check_does_not_require_an_output_directory(self) -> None:
        result = subprocess.run(
            [sys.executable, str(TOOL), "--root", str(ROOT), "--verify-independence"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["ok"])

    def test_authoring_catalog_has_exactly_the_authority_closed_eighteen_cases(self) -> None:
        module = fixture_author_module()
        catalog = module.load_authoring_catalog(ROOT)
        self.assertEqual(
            [item["id"] for item in catalog],
            [
                "BG-001", "BG-002", "BG-003", "BG-004", "BG-005",
                "BG-006", "BG-007", "BG-008", "BG-009", "BG-010",
                "BG-N01", "BG-N02", "BG-N03", "BG-N04", "BG-N05",
                "BG-N06", "BG-N07", "BG-N08",
            ],
        )

    def test_two_independent_materializations_are_byte_identical_and_match_authority_hex(self) -> None:
        module = fixture_author_module()
        with tempfile.TemporaryDirectory() as temporary:
            temporary_root = Path(temporary)
            first = temporary_root / "first"
            second = temporary_root / "second"
            first_manifest = module.materialize(ROOT, first)
            second_manifest = module.materialize(ROOT, second)
            self.assertEqual(first_manifest["inventorySha256"], second_manifest["inventorySha256"])
            first_files = sorted(path.relative_to(first) for path in first.rglob("*") if path.is_file())
            second_files = sorted(path.relative_to(second) for path in second.rglob("*") if path.is_file())
            self.assertEqual(first_files, second_files)
            for relative in first_files:
                self.assertEqual((first / relative).read_bytes(), (second / relative).read_bytes())
            self.assertEqual(
                (first / "wire/bg/BG-003-vec2/expected/canonical.pb").read_bytes().hex(),
                "09000000000000f03f110000000000000040",
            )

    def test_fixture_author_has_no_production_semantic_dependency(self) -> None:
        module = fixture_author_module()
        result = module.verify_independence(ROOT)
        self.assertTrue(result["ok"], result["violations"])

    def test_materialization_can_reproduce_a_reviewed_compiler_commit_binding(self) -> None:
        module = fixture_author_module()
        with tempfile.TemporaryDirectory() as temporary:
            manifest = module.materialize(ROOT, Path(temporary), compiler_commit="f" * 40)
            first = manifest["cases"][0]
            provenance = json.loads((Path(temporary) / "wire/bg" / first["path"].split("/")[-1] / "provenance.json").read_text(encoding="utf-8"))
        self.assertEqual(provenance["fixtureCompiler"]["sourceCommit"], "f" * 40)

    def test_materialization_is_idempotent_in_the_same_output_directory(self) -> None:
        module = fixture_author_module()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            module.materialize(ROOT, output, compiler_commit="f" * 40)
            first = {path.relative_to(output): path.read_bytes() for path in output.rglob("*") if path.is_file()}
            module.materialize(ROOT, output, compiler_commit="f" * 40)
            second = {path.relative_to(output): path.read_bytes() for path in output.rglob("*") if path.is_file()}
        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
