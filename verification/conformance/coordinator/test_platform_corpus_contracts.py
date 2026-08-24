#!/usr/bin/env python3
"""MR-10-05A platform corpus namespace and semantic reference meta-tests."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

import platform_contracts as pc


PLATFORM_SEED_IDS = [
    "PLAT-CREATE-CANVAS-001",
    "PLAT-HOST-ATTACH-001",
    "PLAT-DOCUMENT-ATTACH-001",
    "PLAT-CANONICAL-REPLAY-001",
    "PLAT-METRICS-RESIZE-001",
    "PLAT-METRICS-DPI-SCALE-001",
    "PLAT-METRICS-ORIENTATION-001",
    "PLAT-VISIBILITY-001",
    "PLAT-APP-BACKGROUND-001",
    "PLAT-APP-FOREGROUND-001",
    "PLAT-CANVAS-SUSPEND-001",
    "PLAT-CANVAS-RESUME-001",
    "PLAT-SURFACE-LOST-001",
    "PLAT-SURFACE-REBIND-001",
    "PLAT-STALE-GENERATION-REJECT-001",
    "PLAT-DEVICE-LOST-001",
    "PLAT-DEVICE-RECOVER-001",
    "PLAT-HOST-DETACH-REATTACH-001",
    "PLAT-DATABRIDGE-NO-ECHO-001",
    "PLAT-CALLBACK-NONREENTRANT-001",
    "PLAT-INPUT-BATCH-NORMALIZED-001",
    "PLAT-INPUT-HOTPATH-001",
    "PLAT-ARC-PREVIEW-FALLBACK-001",
    "PLAT-ARC-CANONICAL-HANDOFF-001",
    "PLAT-SURFACE-OWNERSHIP-001",
    "PLAT-DESTROY-CANVAS-001",
    "PLAT-DESTROY-STALE-WORK-001",
    "PLAT-RECOVERY-REPEATED-001",
]


class PlatformCorpusContractTests(unittest.TestCase):
    def platform_suite_path(self) -> Path:
        return pc.ROOT / "platform" / "v1" / "suites" / "platform-seed-v0.1.json"

    def scenario(self):
        return {
            "format": "axiom-platform-scenario-v1",
            "formatVersion": 1,
            "id": "PLAT-CANONICAL-REPLAY-001",
            "requirementStatus": "SPEC_REQUIREMENT",
            "requirementIds": ["PC-I01", "VER-07"],
            "authorityRefs": ["https://app.notion.com/p/3c44c57a590c81d19ebacc7546070191"],
            "canonicalFixtureRef": "REPLAY-MIXED-OPERATIONS-001",
            "targets": [{"platformFamily": "WEB", "policy": "REQUIRED", "requiredCapabilities": ["semantic.projection.capture"]}],
            "preconditions": {
                "appState": "FOREGROUND",
                "canvasState": "NOT_CREATED",
                "hostState": "DETACHED",
                "documentState": "AVAILABLE",
                "surfaceState": "UNBOUND",
                "deviceState": "READY",
                "arcPreviewMode": "DISABLED",
            },
            "steps": [
                {
                    "stepId": "s01",
                    "kind": "SEMANTIC",
                    "action": {"operation": "REPLAY_CANONICAL_FIXTURE", "fixtureRef": "REPLAY-MIXED-OPERATIONS-001"},
                    "completion": {"mode": "WAIT_FOR_ACTION_COMPLETION"},
                }
            ],
            "expected": {
                "requiredEvents": [
                    {"id": "evt-running", "selector": {"traceKind": "LIFECYCLE", "event": "CANVAS_RUNNING", "occurrence": "ANY"}, "minCount": 1}
                ],
                "forbiddenEvents": [],
                "partialOrder": [],
                "stateAssertions": [],
                "openObservations": [],
            },
            "capture": {
                "lifecycleTrace": True,
                "surfaceTrace": False,
                "bridgeTrace": False,
                "semanticCheckpoints": [],
                "stateCheckpoints": [],
                "ownershipSnapshot": False,
            },
        }

    def test_platform_seed_suite_is_materialized_with_exact_28_stable_ids(self):
        path = self.platform_suite_path()
        self.assertTrue(path.is_file(), "platform-seed-v0.1.json must be materialized")
        suite = json.loads(path.read_text(encoding="utf-8"))
        pc.validate_platform_suite_semantics(suite)
        self.assertEqual(suite["id"], "platform-seed-v0.1")
        self.assertEqual(suite["scenarios"], PLATFORM_SEED_IDS)
        self.assertEqual(len(suite["scenarios"]), 28)
        self.assertEqual(len(set(suite["scenarios"])), 28)

    def test_semantic_case_namespace_contains_seed_references_used_by_platform_authority(self):
        self.assertTrue(hasattr(pc, "semantic_case_ids"), "semantic_case_ids resolver must exist")
        ids = pc.semantic_case_ids()
        self.assertIn("REPLAY-MIXED-OPERATIONS-001", ids)
        self.assertIn("OP-SETTRANSFORMS-VALID-001", ids)

    def test_meta_platform_scenario_missing_fixture_reject(self):
        self.assertTrue(hasattr(pc, "validate_platform_scenario_references"), "platform reference validator must exist")
        scenario = self.scenario()
        scenario["canonicalFixtureRef"] = "MISSING-SEMANTIC-FIXTURE-001"
        with self.assertRaisesRegex(ValueError, "semantic fixture"):
            pc.validate_platform_scenario_references(scenario)

    def test_known_canonical_fixture_reference_passes(self):
        self.assertTrue(hasattr(pc, "validate_platform_scenario_references"), "platform reference validator must exist")
        pc.validate_platform_scenario_references(self.scenario())

    def test_missing_semantic_step_fixture_rejects_even_when_canonical_fixture_exists(self):
        self.assertTrue(hasattr(pc, "validate_platform_scenario_references"), "platform reference validator must exist")
        scenario = copy.deepcopy(self.scenario())
        scenario["steps"][0]["action"]["fixtureRef"] = "MISSING-STEP-FIXTURE-001"
        with self.assertRaisesRegex(ValueError, "semantic fixture"):
            pc.validate_platform_scenario_references(scenario)


if __name__ == "__main__":
    unittest.main()
