"""Trust-boundary and reproducibility tests for the C2 fixture compiler."""

from __future__ import annotations

import hashlib
import importlib.util
import inspect
import math
import json
from pathlib import Path
import re
import struct
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

    def test_nonfinite_f64_carrier_roundtrips_exact_bits(self) -> None:
        carriers = {
            "f64:7ff8000000000000": 0x7FF8000000000000,
            "f64:7ff0000000000000": 0x7FF0000000000000,
            "f64:fff0000000000000": 0xFFF0000000000000,
        }
        for token, expected_bits in carriers.items():
            bits = int(token[4:], 16)
            self.assertEqual(expected_bits, bits)
            value = struct.unpack(">d", bits.to_bytes(8, "big"))[0]
            self.assertTrue(math.isnan(value) or math.isinf(value))
            self.assertEqual(expected_bits, int.from_bytes(struct.pack(">d", value), "big"))

    def test_transform_nan_inf_fixture_realizes_current_authority(self) -> None:
        value = json.loads((GENERATED / "inputs/C1-TRANSFORM-NAN-INF.json").read_text(encoding="utf-8"))
        transform = value["operation"]["payload"]["value"]["items"][0]["transform"]
        self.assertEqual(
            [
                "f64:7ff8000000000000",
                0.0,
                0.0,
                1.0,
                "f64:7ff0000000000000",
                "f64:fff0000000000000",
            ],
            transform,
        )

    def test_patch_duplicate_field_fixture_realizes_case_intent(self) -> None:
        value = json.loads((GENERATED / "inputs/C1-PATCH-DUPLICATE-FIELD.json").read_text(encoding="utf-8"))
        patches = value["operation"]["payload"]["value"]["patches"]
        keys = [(patch["object_id"], patch["field_id"]) for patch in patches]
        self.assertGreaterEqual(len(keys), 2)
        self.assertLess(len(set(keys)), len(keys))

    def test_size_nonfinite_fixture_realizes_case_intent(self) -> None:
        value = json.loads((GENERATED / "inputs/C1-SIZE-NONFINITE.json").read_text(encoding="utf-8"))
        item = value["operation"]["payload"]["value"]["items"][0]
        self.assertEqual("f64:7ff0000000000000", item["width"])
        self.assertEqual(24.0, item["height"])

    def test_every_accepted_case_has_explicit_realization_rule(self) -> None:
        cases = json.loads(CASES.read_text(encoding="utf-8"))
        self.assertEqual({case["id"] for case in cases}, set(REALIZATION_RULES))

    def test_every_generated_fixture_satisfies_realization_rule(self) -> None:
        audit = build_case_intent_audit(CASES, GENERATED)
        self.assertEqual(90, len(audit))
        self.assertTrue(all(entry["result"] == "PASS" for entry in audit))

    def test_realization_audit_does_not_use_expected_outcome_or_production_semantics(self) -> None:
        source = inspect.getsource(_assert_case_realization)
        for token in (
            "disposition",
            "terminalPhase",
            "semanticErrorCategory",
            "logicalPlanProjection",
            "mutationExpected",
            "PLAN_READY",
            "ALREADY_APPLIED",
            "REJECTED",
            "ReferenceObjectStore",
            "IndexedObjectStore",
            "OperationEngine",
            "prepareApplyPlan",
        ):
            self.assertNotIn(token, source, token)

    def test_no_realization_rule_uses_generic_fallback(self) -> None:
        source = inspect.getsource(_assert_case_realization)
        self.assertNotIn("assert isinstance(operation, dict) and operation", source)
        self.assertIn("No explicit realization predicate", source)

    def test_success_fixture_audit_entries_have_distinguishing_descriptions(self) -> None:
        audit = {entry["caseId"]: entry for entry in build_case_intent_audit(CASES, GENERATED)}
        expected = {
            "C1-INSERT-VALID",
            "C1-PLACEMENT-VALID",
            "C1-PATCH-VALID",
            "C1-SIZE-VALID",
            "C1-IMAGE-VALID",
            "C1-SPLIT-PLAN",
            "C1-ERASE-ADD-VALID",
        }
        for case_id in expected:
            assertions = audit[case_id]["assertions"]
            self.assertGreaterEqual(len(assertions), 2, case_id)
            self.assertTrue(all("stimulus predicate for" not in item for item in assertions), case_id)


def _stable_id(case_id: str, role: str) -> str:
    return hashlib.sha256(f"axiom-g1-04-c:{case_id}:{role}".encode("utf-8")).hexdigest()[:32]


def _assert_case_realization(case_id: str, case: dict[str, object], value: dict[str, object]) -> list[str]:
    assert case["id"] == case_id
    assert value["caseId"] == case_id
    assert value["operationFamily"] == case["operationFamily"]
    operation = value["operation"]["payload"]["value"]
    initial = value["initialState"]["objects"]
    initial_by_id = {item["id"]: item for item in initial}
    target_id = _stable_id(case_id, "target")
    if case_id == "C1-INSERT-VALID":
        assert len(operation["objects"]) == 1
        inserted = operation["objects"][0]
        assert inserted["id"] not in initial_by_id
        assert inserted["kind"] == 1 and inserted["kind_version"] == 1
        assert inserted["placement"]["parent_id"] is None
        assert inserted["content"]["variant"] == 0
        shape = inserted["content"]["value"]
        assert shape["shape_kind"] == 1 and shape["width"] > 0 and shape["height"] > 0
        assert all(isinstance(x, (int, float)) and math.isfinite(x) for x in inserted["transform"])
        return [
            "inserted object id is absent from initial state",
            "inserted object has the published shape kind and positive dimensions",
            "inserted object placement is root-level with a finite transform",
        ]
    elif case_id == "C1-PLACEMENT-VALID":
        assert len(operation["items"]) == 1
        item = operation["items"][0]
        assert item["object_id"] in initial_by_id
        assert item["placement"]["parent_id"] is None
        assert item["placement"]["order_key"] == initial_by_id[item["object_id"]]["placement"]["order_key"]
        return [
            "placement target resolves to an existing object",
            "valid placement keeps a root-level parent reference",
            "placement order key matches the deterministic fixture state",
        ]
    elif case_id == "C1-PATCH-VALID":
        assert len(operation["patches"]) == 1
        patch = operation["patches"][0]
        assert patch["object_id"] in initial_by_id
        assert patch["action"] == "set" and patch["field_id"] == 1
        assert patch["value"]["variant"] == 1 and isinstance(patch["value"]["value"], str)
        return [
            "patch target resolves to an existing object",
            "patch selects the published field id with a set action",
            "patch value uses the declared string value branch",
        ]
    elif case_id == "C1-SIZE-VALID":
        assert len(operation["items"]) == 1
        item = operation["items"][0]
        assert item["object_id"] in initial_by_id
        assert item["width"] > 0 and item["height"] > 0
        assert math.isfinite(item["width"]) and math.isfinite(item["height"])
        return [
            "size target resolves to an existing object",
            "width and height are finite positive dimensions",
        ]
    elif case_id == "C1-IMAGE-VALID":
        assert operation["object_id"] in initial_by_id
        assert initial_by_id[operation["object_id"]]["kind"] == 2
        content = operation["content"]
        assert content["content_mode"] == 1
        assert content["resource_id"] and content["width"] > 0 and content["height"] > 0
        assert content["intrinsic_width"] > 0 and content["intrinsic_height"] > 0
        return [
            "image target resolves to an existing image object",
            "image content carries a resource id and published content mode",
            "local and intrinsic image dimensions are positive",
        ]
    elif case_id == "C1-SPLIT-PLAN":
        assert len(operation["splits"]) == 1
        split = operation["splits"][0]
        assert split["source_stroke_id"] in initial_by_id
        replacements = split["replacements"]
        assert len(replacements) == 2
        replacement_ids = [item["id"] for item in replacements]
        assert len(set(replacement_ids)) == 2 and not (set(replacement_ids) & set(initial_by_id))
        assert all(item["kind"] == 5 and item["content"]["variant"] == 4 for item in replacements)
        assert all(len(item["content"]["value"]["stroke"]["data"]) >= 2 for item in replacements)
        return [
            "split source references an existing stroke object",
            "split produces two distinct replacement ids absent from initial state",
            "each replacement carries a valid stroke payload with multiple points",
        ]
    elif case_id == "C1-ERASE-ADD-VALID":
        assert len(operation["items"]) == 1
        item = operation["items"][0]
        assert item["object_id"] in initial_by_id
        assert len(item["masks"]) == 1
        mask = item["masks"][0]
        assert mask["id"] and mask["id"] not in {m["id"] for m in initial_by_id[item["object_id"]]["erase_masks"]}
        assert len(mask["geometry"]["value"]["segments"]) == 1
        return [
            "erase-mask target resolves to an existing object",
            "valid add operation introduces one previously absent mask id",
            "mask geometry contains one structural segment",
        ]
    elif case_id == "C1-TRANSFORM-FINITE":
        assert all(isinstance(x, (int, float)) and math.isfinite(x) for x in operation["items"][0]["transform"])
        assert all(isinstance(x, (int, float)) and math.isfinite(x) for x in operation["items"][0]["transform"])
    elif case_id == "C1-TRANSFORM-NEGATIVE-ZERO":
        tx = operation["items"][0]["transform"][4]
        assert tx == 0.0 and (isinstance(tx, str) or math.copysign(1.0, tx) < 0)
    elif case_id == "C1-TRANSFORM-NAN-INF":
        assert operation["items"][0]["transform"] == ["f64:7ff8000000000000", 0.0, 0.0, 1.0, "f64:7ff0000000000000", "f64:fff0000000000000"]
    elif case_id == "C1-PATCH-DUPLICATE-FIELD":
        keys = [(patch["object_id"], patch["field_id"]) for patch in operation["patches"]]
        assert len(keys) >= 2 and len(set(keys)) < len(keys)
    elif case_id == "C1-PATCH-FIELD-ID":
        assert operation["patches"][0]["field_id"] == 999999
    elif case_id == "C1-PATCH-BRANCH-TYPE":
        assert operation["patches"][0]["value"]["variant"] == 2
    elif case_id == "C1-PATCH-APPLICABILITY":
        assert initial_by_id[target_id]["kind"] == 5
    elif case_id == "C1-PATCH-PRESENCE-DEFAULT":
        assert "value" not in operation["patches"][0]
    elif case_id == "C1-SIZE-NONFINITE":
        assert operation["items"][0]["width"] == "f64:7ff0000000000000" and math.isfinite(operation["items"][0]["height"])
    elif case_id == "C1-SIZE-NONPOSITIVE":
        assert operation["items"][0]["width"] <= 0
    elif case_id == "C1-SIZE-HARD-LIMIT":
        assert operation["items"][0]["width"] == 100000.0
    elif case_id == "C1-SIZE-WRONG-KIND":
        assert initial_by_id[target_id]["kind"] == 4
    elif case_id == "C1-DELETE-MISSING-TARGET":
        assert target_id not in initial_by_id
    elif case_id == "C1-DELETE-DUPLICATE-TARGET":
        assert operation["object_ids"].count(target_id) >= 2
    elif case_id in {"C1-DELETE-VALID", "C1-DELETE-CASCADE", "C1-DELETE-SUBTREE"}:
        assert target_id in initial_by_id and target_id in operation["object_ids"]
        if case_id != "C1-DELETE-VALID":
            assert _stable_id(case_id, "child") in operation["object_ids"]
    elif case_id in {"C1-PLACEMENT-CYCLE", "C1-INSERT-HIERARCHY-CYCLE"}:
        if case_id == "C1-PLACEMENT-CYCLE":
            assert operation["items"][0]["placement"]["parent_id"] == target_id
        else:
            created = operation["objects"]
            by_id = {item["id"]: item for item in created}
            assert len(created) == 2 and all(item["placement"]["parent_id"] in by_id for item in created)
            assert all(by_id[item["placement"]["parent_id"]]["placement"]["parent_id"] == item["id"] for item in created)
    elif case_id == "C1-HIERARCHY-STICKY":
        assert initial_by_id[target_id]["kind"] == 8 and operation["items"][0]["placement"]["parent_id"] == _stable_id(case_id, "parent")
    elif case_id == "C1-PLACEMENT-GROUP-ANY":
        assert initial_by_id[_stable_id(case_id, "parent")]["kind"] == 9
    elif case_id == "C1-PLACEMENT-STICKY-RICHTEXT":
        assert initial_by_id[target_id]["kind"] == 4 and operation["items"][0]["placement"]["parent_id"] is not None
    elif case_id == "C1-PLACEMENT-INVALID-PARENT":
        parent_id = operation["items"][0]["placement"]["parent_id"]
        assert parent_id not in initial_by_id
    elif case_id == "C1-PLACEMENT-ORDERKEY":
        assert operation["items"][0]["placement"]["order_key"] == []
    elif case_id == "C1-PLACEMENT-NONPARENT":
        assert operation["items"][0]["placement"]["parent_id"] == _stable_id(case_id, "unrelated-parent")
    elif case_id in {"C1-INSERT-STAGED-PARENT", "C1-RESTORE-STAGED-PARENT-CHILD"}:
        records = operation["objects"]
        ids = {item["id"] for item in records}
        assert len(records) == 2 and any(item["placement"]["parent_id"] in ids for item in records)
    elif case_id in {"C1-INSERT-STAGED-CONNECTOR", "C1-RESTORE-STAGED-CONNECTOR"}:
        records = operation["objects"]
        ids = {item["id"] for item in records}
        connector = next(item for item in records if item["kind"] == 7)
        assert len(records) == 2 and connector["content"]["value"]["start"]["value"]["target_object_id"] in ids
    elif case_id == "C1-INSERT-STICKY-CARDINALITY":
        records = operation["objects"]
        parent = next(item for item in records if item["kind"] == 8 and item["placement"]["parent_id"] is None)
        assert sum(item["placement"]["parent_id"] == parent["id"] for item in records) >= 2
    elif case_id == "C1-INSERT-EXISTING-ID":
        assert operation["objects"][0]["id"] in initial_by_id
    elif case_id == "C1-ID-COLLISION":
        prior = value["initialState"]["priorOperations"][0]
        assert prior["operation_id"] == value["operation"]["id"] and prior["payload"] != value["operation"]["payload"]
    elif case_id == "C1-IDEMPOTENT-EQUIVALENT":
        prior = value["initialState"]["priorOperations"][0]
        assert prior["operation_id"] == value["operation"]["id"] and prior["payload"] == value["operation"]["payload"]
    elif case_id == "C1-RESTORE-SAME-PAYLOAD-NEW-OPID":
        prior = value["initialState"]["priorOperations"][0]
        assert prior["operation_id"] != value["operation"]["id"] and prior["payload"] == value["operation"]["payload"]
    elif case_id == "C1-RESTORE-OPID-BEFORE-EXISTENCE":
        assert len(value["initialState"]["priorOperations"]) == 1 and not initial
    elif case_id == "C1-RESTORE-LOCAL-REPLAY-REMOTE":
        assert [item["name"] for item in value["executionVariants"]] == ["local", "replay", "remote"]
    elif case_id == "C1-RESTORE-ELIGIBLE":
        assert operation["objects"] and not initial
    elif case_id in {"C1-RESTORE-EXISTING-ID", "C1-RESTORE-EXISTING-ID-DIFFERENT", "C1-RESTORE-BATCH-EXISTING-ID"}:
        assert any(item["id"] in initial_by_id for item in operation["objects"])
    elif case_id == "C1-RESTORE-ABSENT-REF":
        assert operation["objects"][0]["placement"]["parent_id"] not in initial_by_id
    elif case_id == "C1-RESTORE-CONNECTOR-TARGET-ABSENT":
        connector = operation["objects"][0]
        target = connector["content"]["value"]["start"]["value"]["target_object_id"]
        assert target not in initial_by_id
    elif case_id == "C1-RESTORE-NO-TOMBSTONE":
        assert not initial and operation["objects"]
    elif case_id == "C1-GEOMETRY-STRUCTURAL":
        assert operation["geometry"]["value"]["segments"] == []
    elif case_id == "C1-GEOMETRY-WRONG-KIND":
        assert initial_by_id[target_id]["kind"] == 1
    elif case_id in {"C1-GEOMETRY-N-1", "C1-GEOMETRY-N", "C1-GEOMETRY-BOUNDARY", "C1-GEOMETRY-LIMIT", "C1-GEOMETRY-OVERFLOW"}:
        expected_counts = {"C1-GEOMETRY-N-1": 2, "C1-GEOMETRY-N": 3, "C1-GEOMETRY-BOUNDARY": 3, "C1-GEOMETRY-LIMIT": 4, "C1-GEOMETRY-OVERFLOW": 5}
        assert len(operation["geometry"]["value"]["segments"]) == expected_counts[case_id]
    elif case_id == "C1-IMAGE-WRONG-KIND":
        assert initial_by_id[target_id]["kind"] == 1
    elif case_id == "C1-IMAGE-CONTENT-PRESENCE":
        assert operation["content"] == {}
    elif case_id == "C1-IMAGE-SOURCE-RECT":
        assert "source_rect" in operation["content"]
    elif case_id == "C1-IMAGE-INTRINSIC":
        assert operation["content"]["intrinsic_width"] == 640.0
    elif case_id == "C1-IMAGE-CONTENTMODE":
        assert operation["content"]["content_mode"] == 2
    elif case_id == "C1-IMAGE-LOCAL-SIZE":
        assert operation["content"]["width"] == 64.0 and operation["content"]["height"] == 48.0
    elif case_id == "C1-IMAGE-RUNTIME-RESOURCE-NONSEMANTIC":
        assert operation["content"]["resource_id"] == _stable_id(case_id, "runtime-resource")
    elif case_id == "C1-STROKE-WRONG-CONTENT":
        assert operation["object"]["content"]["variant"] == 0
    elif case_id == "C1-STROKE-INVALID-RECORD":
        assert operation["object"]["content"]["value"] == {}
    elif case_id in {"C1-STROKE-VALID", "C1-STROKE-NEW-ID"}:
        assert operation["object"]["id"] not in initial_by_id and len(operation["object"]["content"]["value"]["stroke"]["data"]) >= 2
    elif case_id == "C1-STROKE-EXISTING-ID":
        assert operation["object"]["id"] in initial_by_id
    elif case_id == "C1-SPLIT-SOURCE-MISSING":
        assert target_id not in initial_by_id
    elif case_id == "C1-SPLIT-REPLACEMENT-COLLISION":
        assert operation["splits"][0]["replacements"][0]["id"] in initial_by_id
    elif case_id == "C1-SPLIT-REPLACEMENT-STRUCTURAL":
        assert operation["splits"][0]["replacements"][0]["content"]["value"] == {}
    elif case_id == "C1-ERASE-ADD-UNIQUENESS":
        ids = [item["id"] for item in operation["items"][0]["masks"]]
        assert len(ids) >= 2 and len(set(ids)) < len(ids)
    elif case_id == "C1-ERASE-ADD-GEOMETRY":
        assert operation["items"][0]["masks"][0]["geometry"]["value"]["segments"] == []
    elif case_id == "C1-ERASE-ADD-CAPABILITY":
        assert initial_by_id[target_id]["kind"] == 1
    elif case_id == "C1-ERASE-ADD-EXISTING-MASK":
        assert initial_by_id[target_id]["erase_masks"]
    elif case_id in {"C1-ERASE-REMOVE-VALID", "C1-ERASE-REMOVE-MISSING", "C1-ERASE-REMOVE-DUPLICATE", "C1-ERASE-REMOVE-WHOLE-REJECT"}:
        ids = operation["items"][0]["mask_ids"]
        if case_id == "C1-ERASE-REMOVE-MISSING":
            assert not initial_by_id[target_id]["erase_masks"]
        elif case_id == "C1-ERASE-REMOVE-DUPLICATE":
            assert len(ids) >= 2 and len(set(ids)) < len(ids)
        elif case_id == "C1-ERASE-REMOVE-WHOLE-REJECT":
            assert set(ids) == {item["id"] for item in initial_by_id[target_id]["erase_masks"]}
        else:
            assert ids[0] in {item["id"] for item in initial_by_id[target_id]["erase_masks"]}
    elif case_id == "C1-RICHTEXT-UTF8-STYLE":
        assert "λ" in operation["delta"]["steps"][0]["text"]
    elif case_id == "C1-RICHTEXT-INVALID-STEP":
        assert operation["delta"]["steps"][0]["kind"] == "UnknownStep"
    elif case_id in {"C1-RICHTEXT-VALID", "C1-RICHTEXT-STABLE-REFS"}:
        assert operation["delta"]["steps"][0]["paragraph_id"] == _stable_id(case_id, "paragraph")
    elif case_id == "C1-CONNECTOR-INVALID-END":
        assert operation["content"]["end"]["variant"] == 99
    elif case_id == "C1-CONNECTOR-ANCHOR":
        assert operation["content"]["start"]["value"]["anchor"]["value"]["port_id"] == 0
    elif case_id == "C1-CONNECTOR-ROUTING":
        assert operation["content"]["routing"] == 99
    elif case_id in {"C1-CONNECTOR-VALID", "C1-CONNECTOR-ATTACHED-ENDPOINT", "C1-CONNECTOR-TARGET-CAPABILITY"}:
        target = operation["content"]["start"]["value"]["target_object_id"]
        if case_id == "C1-CONNECTOR-VALID":
            assert operation["content"]["end"]["variant"] == 0 and operation["content"]["routing"] == 1
        else:
            assert target in initial_by_id
        if case_id == "C1-CONNECTOR-TARGET-CAPABILITY":
            assert initial_by_id[target]["kind"] == 1
    else:
        raise AssertionError(f"No explicit realization predicate for {case_id}")
    return [f"stimulus predicate for {case_id}"]


_REALIZATION_CASE_IDS = (
    "C1-INSERT-VALID", "C1-DELETE-CASCADE", "C1-RESTORE-ELIGIBLE", "C1-PLACEMENT-VALID", "C1-TRANSFORM-FINITE",
    "C1-PATCH-VALID", "C1-SIZE-VALID", "C1-GEOMETRY-BOUNDARY", "C1-IMAGE-VALID", "C1-STROKE-VALID", "C1-SPLIT-PLAN",
    "C1-ERASE-ADD-VALID", "C1-ERASE-REMOVE-VALID", "C1-RICHTEXT-VALID", "C1-CONNECTOR-VALID", "C1-IDEMPOTENT-EQUIVALENT",
    "C1-ID-COLLISION", "C1-GEOMETRY-LIMIT", "C1-HIERARCHY-STICKY", "C1-INSERT-STAGED-PARENT", "C1-INSERT-STAGED-CONNECTOR",
    "C1-INSERT-HIERARCHY-CYCLE", "C1-INSERT-STICKY-CARDINALITY", "C1-DELETE-MISSING-TARGET", "C1-DELETE-DUPLICATE-TARGET",
    "C1-RESTORE-EXISTING-ID", "C1-RESTORE-STAGED-PARENT-CHILD", "C1-RESTORE-STAGED-CONNECTOR", "C1-RESTORE-ABSENT-REF",
    "C1-RESTORE-OPID-BEFORE-EXISTENCE", "C1-RESTORE-LOCAL-REPLAY-REMOTE", "C1-RESTORE-NO-TOMBSTONE", "C1-PLACEMENT-CYCLE",
    "C1-PLACEMENT-INVALID-PARENT", "C1-PLACEMENT-ORDERKEY", "C1-TRANSFORM-NEGATIVE-ZERO", "C1-TRANSFORM-NAN-INF",
    "C1-PATCH-FIELD-ID", "C1-PATCH-BRANCH-TYPE", "C1-PATCH-APPLICABILITY", "C1-PATCH-DUPLICATE-FIELD", "C1-SIZE-WRONG-KIND",
    "C1-SIZE-NONFINITE", "C1-SIZE-HARD-LIMIT", "C1-GEOMETRY-WRONG-KIND", "C1-GEOMETRY-STRUCTURAL", "C1-GEOMETRY-N-1",
    "C1-GEOMETRY-N", "C1-GEOMETRY-OVERFLOW", "C1-IMAGE-WRONG-KIND", "C1-IMAGE-CONTENT-PRESENCE", "C1-IMAGE-SOURCE-RECT",
    "C1-STROKE-WRONG-CONTENT", "C1-SPLIT-SOURCE-MISSING", "C1-SPLIT-REPLACEMENT-COLLISION", "C1-ERASE-ADD-UNIQUENESS",
    "C1-ERASE-REMOVE-MISSING", "C1-RICHTEXT-UTF8-STYLE", "C1-RICHTEXT-INVALID-STEP", "C1-CONNECTOR-ATTACHED-ENDPOINT",
    "C1-CONNECTOR-INVALID-END", "C1-INSERT-EXISTING-ID", "C1-DELETE-VALID", "C1-DELETE-SUBTREE", "C1-RESTORE-EXISTING-ID-DIFFERENT",
    "C1-RESTORE-CONNECTOR-TARGET-ABSENT", "C1-RESTORE-SAME-PAYLOAD-NEW-OPID", "C1-RESTORE-BATCH-EXISTING-ID", "C1-PLACEMENT-GROUP-ANY",
    "C1-PLACEMENT-STICKY-RICHTEXT", "C1-PLACEMENT-NONPARENT", "C1-PATCH-PRESENCE-DEFAULT", "C1-SIZE-NONPOSITIVE", "C1-IMAGE-INTRINSIC",
    "C1-IMAGE-CONTENTMODE", "C1-IMAGE-LOCAL-SIZE", "C1-IMAGE-RUNTIME-RESOURCE-NONSEMANTIC", "C1-STROKE-INVALID-RECORD",
    "C1-STROKE-NEW-ID", "C1-STROKE-EXISTING-ID", "C1-SPLIT-REPLACEMENT-STRUCTURAL", "C1-ERASE-ADD-GEOMETRY", "C1-ERASE-ADD-CAPABILITY",
    "C1-ERASE-ADD-EXISTING-MASK", "C1-ERASE-REMOVE-DUPLICATE", "C1-ERASE-REMOVE-WHOLE-REJECT", "C1-RICHTEXT-STABLE-REFS",
    "C1-CONNECTOR-TARGET-CAPABILITY", "C1-CONNECTOR-ANCHOR", "C1-CONNECTOR-ROUTING",
)


REALIZATION_RULES = {case_id: (lambda case, value, _id=case_id: _assert_case_realization(_id, case, value)) for case_id in _REALIZATION_CASE_IDS}


def build_case_intent_audit(cases_path: Path, generated_root: Path) -> list[dict[str, object]]:
    cases = json.loads(cases_path.read_text(encoding="utf-8"))
    assert set(REALIZATION_RULES) == {case["id"] for case in cases}
    audit: list[dict[str, object]] = []
    for case in sorted(cases, key=lambda item: item["id"]):
        case_id = case["id"]
        path = generated_root / "inputs" / f"{case_id}.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        assertions = REALIZATION_RULES[case_id](case, value)
        audit.append({"caseId": case_id, "operationFamily": case["operationFamily"], "authorityRuleRefs": case["authorityRuleRefs"], "generatedInputPath": f"generated/inputs/{case_id}.json", "generatedInputSha256": sha256_bytes(path.read_bytes()), "assertions": assertions, "result": "PASS"})
    return audit


if __name__ == "__main__":
    unittest.main()
