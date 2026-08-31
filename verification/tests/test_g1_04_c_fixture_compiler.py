"""Trust-boundary and reproducibility tests for the C2 fixture compiler."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "verification/fixture-author/compile_g1_04_c.py"
CASES = ROOT / "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json"
EXPECTED = ROOT / "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json"
CORE = ROOT / "verification/corpus/semantic/v1/g1-04-c/suites/core.json"
GENERATED = ROOT / "verification/corpus/semantic/v1/g1-04-c/generated"
ANCHOR = "e1b3e1dbc897a4b77385e7a01f8e348af2796610"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")


def walk_values(value: object):
    if isinstance(value, dict):
        for key, nested in value.items():
            yield key
            yield from walk_values(nested)
    elif isinstance(value, list):
        for nested in value:
            yield from walk_values(nested)


class G104CFixtureCompilerTest(unittest.TestCase):
    def load_compiler(self):
        self.assertTrue(TOOL.is_file(), f"missing C2 compiler: {TOOL}")
        spec = importlib.util.spec_from_file_location("g1_04_c_fixture_compiler", TOOL)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(module)
        return module

    def run_cli(self, output: Path, root: Path = ROOT, check: bool = False) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(TOOL),
                "--root",
                str(root),
                "--cases",
                str(root / "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json"),
                "--expected",
                str(root / "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json"),
                "--suite",
                str(root / "verification/corpus/semantic/v1/g1-04-c/suites/core.json"),
                "--output",
                str(output),
            ],
            check=check,
            capture_output=True,
            text=True,
        )

    def test_compiler_exists(self) -> None:
        self.assertTrue(TOOL.is_file(), f"missing C2 compiler: {TOOL}")

    def test_basic_generation_covers_every_accepted_case(self) -> None:
        module = self.load_compiler()
        cases = json.loads(CASES.read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            manifest = module.materialize(ROOT, output, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            self.assertEqual(90, manifest["caseCount"])
            self.assertEqual(90, len(manifest["entries"]))
            self.assertEqual(sorted(case["id"] for case in cases), [entry["caseId"] for entry in manifest["entries"]])
            self.assertEqual(90, len(list((output / "inputs").glob("*.json"))))
            self.assertEqual(90, len(list((output / "provenance").glob("*.json"))))
            for case in cases:
                input_path = output / "inputs" / f"{case['id']}.json"
                provenance_path = output / "provenance" / f"{case['id']}.json"
                self.assertTrue(input_path.is_file())
                self.assertTrue(provenance_path.is_file())
                self.assertEqual("DERIVED_GENERATED", json.loads(input_path.read_text(encoding="utf-8"))["provenance"])
                self.assertEqual("DERIVED_GENERATED", json.loads(provenance_path.read_text(encoding="utf-8"))["provenance"])

    def test_two_clean_materializations_are_byte_identical(self) -> None:
        module = self.load_compiler()
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            first = temporary / "first"
            second = temporary / "second"
            module.materialize(ROOT, first, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            module.materialize(ROOT, second, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            first_files = sorted(path.relative_to(first) for path in first.rglob("*") if path.is_file())
            second_files = sorted(path.relative_to(second) for path in second.rglob("*") if path.is_file())
            self.assertEqual(first_files, second_files)
            for relative in first_files:
                self.assertEqual((first / relative).read_bytes(), (second / relative).read_bytes(), relative)

    def test_clean_regeneration_matches_committed_tree(self) -> None:
        self.assertTrue((GENERATED / "manifest.json").is_file(), "committed generated tree is missing")
        module = self.load_compiler()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            module.materialize(ROOT, output, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            generated_files = sorted(path.relative_to(output) for path in output.rglob("*") if path.is_file())
            committed_files = sorted(path.relative_to(GENERATED) for path in GENERATED.rglob("*") if path.is_file())
            self.assertEqual(committed_files, generated_files)
            for relative in generated_files:
                self.assertEqual((GENERATED / relative).read_bytes(), (output / relative).read_bytes(), relative)

    def test_recorded_hashes_and_inventory_recompute(self) -> None:
        module = self.load_compiler()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            manifest = module.materialize(ROOT, output, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            entries = manifest["entries"]
            self.assertEqual(manifest["inventorySha256"], sha256_bytes(json_bytes(entries)))
            for entry in entries:
                input_path = output / entry["input"]["path"]
                provenance_path = output / entry["provenance"]["path"]
                input_bytes = input_path.read_bytes()
                provenance_bytes = provenance_path.read_bytes()
                self.assertEqual(entry["input"]["sha256"], sha256_bytes(input_bytes))
                self.assertEqual(entry["input"]["bytes"], len(input_bytes))
                self.assertEqual(entry["provenance"]["sha256"], sha256_bytes(provenance_bytes))
                self.assertEqual(entry["provenance"]["bytes"], len(provenance_bytes))

    def test_provenance_traces_authoring_and_generated_records(self) -> None:
        module = self.load_compiler()
        cases = {record["id"]: record for record in json.loads(CASES.read_text(encoding="utf-8"))}
        expected = {record["caseId"]: record for record in json.loads(EXPECTED.read_text(encoding="utf-8"))}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            module.materialize(ROOT, output, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            for case_id, case in cases.items():
                record = json.loads((output / "provenance" / f"{case_id}.json").read_text(encoding="utf-8"))
                self.assertEqual(f"authoring/cases.json#{case_id}", record["sourceCaseRef"])
                self.assertEqual(f"authoring/expected.json#{case_id}", record["expectedRef"])
                self.assertEqual(case["authorityRuleRefs"], record["caseAuthorityRuleRefs"])
                self.assertEqual(expected[case_id]["authorityRuleRefs"], record["expectedAuthorityRuleRefs"])
                self.assertEqual("g1-04-c-independent-fixture-compiler-v1", record["compiler"]["identity"])
                self.assertEqual(record["generatedInput"]["sha256"], sha256_bytes((output / "inputs" / f"{case_id}.json").read_bytes()))

    def test_authoring_output_is_rejected_before_any_write(self) -> None:
        before_cases = CASES.read_bytes()
        before_expected = EXPECTED.read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            attempted = ROOT / "verification/corpus/semantic/v1/g1-04-c/authoring" / f"c2-rejected-{Path(directory).name}"
            result = self.run_cli(attempted)
            self.assertNotEqual(0, result.returncode, result.stderr)
            self.assertFalse(attempted.exists())
        self.assertEqual(before_cases, CASES.read_bytes())
        self.assertEqual(before_expected, EXPECTED.read_bytes())

    def test_compiler_has_no_production_semantic_dependency(self) -> None:
        source = TOOL.read_text(encoding="utf-8")
        forbidden = (
            "OperationEngine",
            "prepareApplyPlan",
            "stateful_validation",
            "stateless_validation",
            "ReferenceObjectStore",
            "IndexedObjectStore",
            "runtime/semantic",
            "canvas::semantic",
            "subprocess",
            "os.system",
            "os.popen",
            "ctypes",
            "cffi",
            "dlopen",
            "importlib",
        )
        for token in forbidden:
            self.assertNotIn(token, source, token)

    def test_minimal_source_root_generates_without_runtime_tree(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in (
                "verification/fixture-author/compile_g1_04_c.py",
                "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
                "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
                "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
            ):
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / relative, destination)
            output = root / "out"
            result = subprocess.run(
                [
                    sys.executable,
                    str(root / "verification/fixture-author/compile_g1_04_c.py"),
                    "--root",
                    str(root),
                    "--cases",
                    str(root / "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json"),
                    "--expected",
                    str(root / "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json"),
                    "--suite",
                    str(root / "verification/corpus/semantic/v1/g1-04-c/suites/core.json"),
                    "--output",
                    str(output),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertTrue((output / "manifest.json").is_file())
            self.assertFalse((root / "runtime").exists())

    def test_generated_tree_contains_no_expected_oracle(self) -> None:
        module = self.load_compiler()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "generated"
            module.materialize(ROOT, output, cases_path=CASES, expected_path=EXPECTED, suite_path=CORE)
            forbidden_keys = {
                "disposition",
                "terminalPhase",
                "semanticErrorCategory",
                "logicalPlanProjection",
                "mutationExpected",
                "observedDisposition",
                "observedTerminalPhase",
                "PASS",
                "FAIL",
            }
            for path in output.rglob("*.json"):
                value = json.loads(path.read_text(encoding="utf-8"))
                self.assertTrue(forbidden_keys.isdisjoint(set(walk_values(value))), path)
                text = path.read_text(encoding="utf-8")
                self.assertNotIn("PLAN_READY", text, path)
                self.assertNotIn("ALREADY_APPLIED", text, path)
                self.assertNotIn("REJECTED", text, path)

    def test_accepted_c0_c1_paths_match_task_anchor(self) -> None:
        for relative in (
            "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
            "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
            "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
        ):
            anchored = subprocess.check_output(["git", "show", f"{ANCHOR}:{relative}"])
            self.assertEqual(anchored, (ROOT / relative).read_bytes(), relative)


if __name__ == "__main__":
    unittest.main()
