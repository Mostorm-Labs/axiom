#!/usr/bin/env python3
"""MR-10-05 platform corpus namespace and scenario-body authority tests."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

import platform_contracts as pc


PLATFORM_SEED_IDS = [
    "PLAT-CREATE-CANVAS-001", "PLAT-HOST-ATTACH-001", "PLAT-DOCUMENT-ATTACH-001", "PLAT-CANONICAL-REPLAY-001",
    "PLAT-METRICS-RESIZE-001", "PLAT-METRICS-DPI-SCALE-001", "PLAT-METRICS-ORIENTATION-001", "PLAT-VISIBILITY-001",
    "PLAT-APP-BACKGROUND-001", "PLAT-APP-FOREGROUND-001", "PLAT-CANVAS-SUSPEND-001", "PLAT-CANVAS-RESUME-001",
    "PLAT-SURFACE-LOST-001", "PLAT-SURFACE-REBIND-001", "PLAT-STALE-GENERATION-REJECT-001", "PLAT-DEVICE-LOST-001",
    "PLAT-DEVICE-RECOVER-001", "PLAT-HOST-DETACH-REATTACH-001", "PLAT-DATABRIDGE-NO-ECHO-001", "PLAT-CALLBACK-NONREENTRANT-001",
    "PLAT-INPUT-BATCH-NORMALIZED-001", "PLAT-INPUT-HOTPATH-001", "PLAT-ARC-PREVIEW-FALLBACK-001", "PLAT-ARC-CANONICAL-HANDOFF-001",
    "PLAT-SURFACE-OWNERSHIP-001", "PLAT-DESTROY-CANVAS-001", "PLAT-DESTROY-STALE-WORK-001", "PLAT-RECOVERY-REPEATED-001",
]

SCENARIO_AUTHORITY = {
    "PLAT-CREATE-CANVAS-001": ("SPEC_REQUIREMENT", ["PC-I02", "VER-20"], "ALL"),
    "PLAT-HOST-ATTACH-001": ("SPEC_REQUIREMENT", ["PC-I02"], "ALL"),
    "PLAT-DOCUMENT-ATTACH-001": ("SPEC_REQUIREMENT", ["PC-I02"], "ALL"),
    "PLAT-CANONICAL-REPLAY-001": ("SPEC_REQUIREMENT", ["PC-I01", "VER-07"], "ALL"),
    "PLAT-METRICS-RESIZE-001": ("SPEC_REQUIREMENT", ["PC-I03"], "ALL"),
    "PLAT-METRICS-DPI-SCALE-001": ("SPEC_REQUIREMENT", ["PC-I03"], "ALL"),
    "PLAT-METRICS-ORIENTATION-001": ("SPEC_REQUIREMENT", ["PC-I03"], "ORIENTATION"),
    "PLAT-VISIBILITY-001": ("SPEC_REQUIREMENT", ["PC-I03"], "ALL"),
    "PLAT-APP-BACKGROUND-001": ("SPEC_REQUIREMENT", ["PC-I09"], "ALL"),
    "PLAT-APP-FOREGROUND-001": ("SPEC_REQUIREMENT", ["PC-I09"], "ALL"),
    "PLAT-CANVAS-SUSPEND-001": ("SPEC_REQUIREMENT", ["PC-I10"], "ALL"),
    "PLAT-CANVAS-RESUME-001": ("SPEC_REQUIREMENT", ["PC-I10"], "ALL"),
    "PLAT-SURFACE-LOST-001": ("SPEC_REQUIREMENT", ["PC-I05"], "ALL"),
    "PLAT-SURFACE-REBIND-001": ("SPEC_REQUIREMENT", ["PC-I05", "PC-I06"], "ALL"),
    "PLAT-STALE-GENERATION-REJECT-001": ("FREEZE_CANDIDATE", ["PC-I04"], "ALL"),
    "PLAT-DEVICE-LOST-001": ("SPEC_REQUIREMENT", ["PC-I07", "VER-19"], "ALL"),
    "PLAT-DEVICE-RECOVER-001": ("SPEC_REQUIREMENT", ["PC-I08", "VER-19"], "ALL"),
    "PLAT-HOST-DETACH-REATTACH-001": ("SPEC_REQUIREMENT", ["PC-I02", "VER-20"], "ALL"),
    "PLAT-DATABRIDGE-NO-ECHO-001": ("SPEC_REQUIREMENT", ["PC-I18"], "ALL"),
    "PLAT-CALLBACK-NONREENTRANT-001": ("SPEC_REQUIREMENT", ["PC-I16", "PC-I18"], "ALL"),
    "PLAT-INPUT-BATCH-NORMALIZED-001": ("SPEC_REQUIREMENT", ["PC-I17", "VER-07"], "ALL"),
    "PLAT-INPUT-HOTPATH-001": ("SPEC_REQUIREMENT", ["PC-I14", "PC-I17"], "ALL"),
    "PLAT-ARC-PREVIEW-FALLBACK-001": ("SPEC_REQUIREMENT", ["PC-I12"], "NATIVE_ARC"),
    "PLAT-ARC-CANONICAL-HANDOFF-001": ("FREEZE_CANDIDATE", ["PC-I13"], "NATIVE_ARC"),
    "PLAT-SURFACE-OWNERSHIP-001": ("SPEC_REQUIREMENT", ["PC-I11"], "NATIVE_ARC"),
    "PLAT-DESTROY-CANVAS-001": ("SPEC_REQUIREMENT", ["VER-20"], "ALL"),
    "PLAT-DESTROY-STALE-WORK-001": ("FREEZE_CANDIDATE", ["PC-I20"], "ALL"),
    "PLAT-RECOVERY-REPEATED-001": ("SPEC_REQUIREMENT", ["PC-I05", "PC-I06", "PC-I07", "PC-I08"], "ALL"),
}


class PlatformCorpusContractTests(unittest.TestCase):
    def platform_suite_path(self) -> Path:
        return pc.ROOT / "platform" / "v1" / "suites" / "platform-seed-v0.1.json"

    def scenarios_root(self) -> Path:
        return pc.ROOT / "platform" / "v1" / "scenarios"

    def scenario(self):
        return {
            "format": "axiom-platform-scenario-v1", "formatVersion": 1, "id": "PLAT-CANONICAL-REPLAY-001",
            "requirementStatus": "SPEC_REQUIREMENT", "requirementIds": ["PC-I01", "VER-07"],
            "authorityRefs": ["https://app.notion.com/p/3c44c57a590c8196ac7acd5215dcbf1d"],
            "canonicalFixtureRef": "REPLAY-MIXED-OPERATIONS-001",
            "targets": [{"platformFamily": "WEB", "policy": "REQUIRED", "requiredCapabilities": ["semantic.projection.capture"]}],
            "preconditions": {"appState": "FOREGROUND", "canvasState": "NOT_CREATED", "hostState": "DETACHED", "documentState": "AVAILABLE", "surfaceState": "UNBOUND", "deviceState": "READY", "arcPreviewMode": "DISABLED"},
            "steps": [{"stepId": "s01", "kind": "SEMANTIC", "action": {"operation": "REPLAY_CANONICAL_FIXTURE", "fixtureRef": "REPLAY-MIXED-OPERATIONS-001"}, "completion": {"mode": "WAIT_FOR_ACTION_COMPLETION"}}],
            "expected": {"requiredEvents": [{"id": "evt-running", "selector": {"traceKind": "LIFECYCLE", "event": "CANVAS_RUNNING", "occurrence": "ANY"}, "minCount": 1}], "forbiddenEvents": [], "partialOrder": [], "stateAssertions": [], "openObservations": []},
            "capture": {"lifecycleTrace": True, "surfaceTrace": False, "bridgeTrace": False, "semanticCheckpoints": [], "stateCheckpoints": [], "ownershipSnapshot": False},
        }

    def _load_suite(self):
        return json.loads(self.platform_suite_path().read_text(encoding="utf-8"))

    def _load_materialized_scenario(self, scenario_id: str):
        path = self.scenarios_root() / scenario_id / "scenario.json"
        self.assertTrue(path.is_file(), f"missing materialized scenario body: {scenario_id}")
        return json.loads(path.read_text(encoding="utf-8"))

    def test_platform_seed_suite_is_materialized_with_exact_28_stable_ids(self):
        path = self.platform_suite_path()
        self.assertTrue(path.is_file(), "platform-seed-v0.1.json must be materialized")
        suite = self._load_suite()
        pc.validate_platform_suite_semantics(suite)
        self.assertEqual(suite["id"], "platform-seed-v0.1")
        self.assertEqual(suite["scenarios"], PLATFORM_SEED_IDS)
        self.assertEqual(len(suite["scenarios"]), 28)
        self.assertEqual(len(set(suite["scenarios"])), 28)

    def test_phase_b_all_28_scenario_bodies_are_materialized_and_valid(self):
        for scenario_id in self._load_suite()["scenarios"]:
            scenario = self._load_materialized_scenario(scenario_id)
            self.assertEqual(scenario["id"], scenario_id)
            pc.validate_platform_scenario_semantics(scenario)
            pc.validate_platform_scenario_references(scenario)

    def test_phase_b_status_requirement_and_target_policy_match_10_09_authority(self):
        for scenario_id in PLATFORM_SEED_IDS:
            scenario = self._load_materialized_scenario(scenario_id)
            expected_status, expected_requirements, target_kind = SCENARIO_AUTHORITY[scenario_id]
            self.assertEqual(scenario["requirementStatus"], expected_status, scenario_id)
            self.assertEqual(scenario["requirementIds"], expected_requirements, scenario_id)
            targets = {t["platformFamily"]: t for t in scenario["targets"]}
            if target_kind == "ALL":
                self.assertEqual(set(targets), {"WEB", "WINDOWS", "ANDROID", "APPLE"}, scenario_id)
                self.assertTrue(all(t["policy"] == "REQUIRED" for t in targets.values()), scenario_id)
            elif target_kind == "ORIENTATION":
                self.assertEqual(set(targets), {"WEB", "WINDOWS", "ANDROID", "APPLE"}, scenario_id)
                self.assertEqual(targets["ANDROID"]["policy"], "REQUIRED")
                self.assertEqual(targets["APPLE"]["policy"], "REQUIRED")
                self.assertEqual(targets["WEB"]["policy"], "REQUIRED_WHEN_CAPABLE")
                self.assertEqual(targets["WINDOWS"]["policy"], "REQUIRED_WHEN_CAPABLE")
            elif target_kind == "NATIVE_ARC":
                self.assertEqual(set(targets), {"WINDOWS", "ANDROID", "APPLE"}, scenario_id)
                self.assertTrue(all(t["policy"] == "REQUIRED_WHEN_CAPABLE" for t in targets.values()), scenario_id)
                self.assertTrue(all("arc.preview" in t["requiredCapabilities"] for t in targets.values()), scenario_id)

    def test_phase_b_has_one_shared_scenario_body_per_id_and_no_platform_authority_variants(self):
        variants = sorted(self.scenarios_root().glob("*/scenario.*.json")) if self.scenarios_root().exists() else []
        self.assertEqual(variants, [], f"platform-specific scenario authority variants are forbidden: {variants}")

    def test_phase_b_common_running_scenarios_do_not_hide_runtime_bootstrap(self):
        for scenario_id in PLATFORM_SEED_IDS[3:]:
            scenario = self._load_materialized_scenario(scenario_id)
            operations = [step.get("action", {}).get("operation") for step in scenario["steps"]]
            if scenario_id in {"PLAT-DATABRIDGE-NO-ECHO-001", "PLAT-INPUT-BATCH-NORMALIZED-001"}:
                continue
            self.assertIn("CREATE_CANVAS", operations, scenario_id)
            self.assertIn("ATTACH_HOST", operations, scenario_id)
            self.assertIn("ATTACH_DOCUMENT", operations, scenario_id)
            self.assertIn("REPLAY_CANONICAL_FIXTURE", operations, scenario_id)

    def test_semantic_case_namespace_contains_seed_references_used_by_platform_authority(self):
        ids = pc.semantic_case_ids()
        self.assertIn("REPLAY-MIXED-OPERATIONS-001", ids)
        self.assertIn("OP-SETTRANSFORMS-VALID-001", ids)

    def test_meta_platform_scenario_missing_fixture_reject(self):
        scenario = self.scenario(); scenario["canonicalFixtureRef"] = "MISSING-SEMANTIC-FIXTURE-001"
        with self.assertRaisesRegex(ValueError, "semantic fixture"):
            pc.validate_platform_scenario_references(scenario)

    def test_known_canonical_fixture_reference_passes(self):
        pc.validate_platform_scenario_references(self.scenario())

    def test_missing_semantic_step_fixture_rejects_even_when_canonical_fixture_exists(self):
        scenario = copy.deepcopy(self.scenario())
        scenario["steps"][0]["action"]["fixtureRef"] = "MISSING-STEP-FIXTURE-001"
        with self.assertRaisesRegex(ValueError, "semantic fixture"):
            pc.validate_platform_scenario_references(scenario)

    def test_missing_databridge_apply_external_fixture_rejects(self):
        scenario = copy.deepcopy(self.scenario())
        scenario["steps"] = [{
            "stepId": "s01", "kind": "BRIDGE",
            "action": {"contract": "DATA_BRIDGE", "operation": "APPLY_EXTERNAL", "correlationId": "bridge-apply-01", "fixtureRef": "MISSING-BRIDGE-FIXTURE-001"},
            "completion": {"mode": "WAIT_FOR_ACTION_COMPLETION"},
        }]
        with self.assertRaisesRegex(ValueError, "semantic fixture"):
            pc.validate_platform_scenario_references(scenario)


if __name__ == "__main__":
    unittest.main()
