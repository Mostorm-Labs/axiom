#!/usr/bin/env python3
"""MR-10-01 contract tests.

These tests intentionally use only the Python standard library. They test the
repository's structural + semantic validation entrypoints, not a third-party
JSON Schema implementation.
"""
from __future__ import annotations

import copy
import unittest

import axiom_conformance as ac


class SemanticArtifactContractTests(unittest.TestCase):
    def observation(self):
        return {
            "format": "axiom-conformance-observation-v1",
            "formatVersion": 1,
            "runnerProtocolVersion": 1,
            "vectorId": "WIRE-F64-NEGZERO-001",
            "implementationId": "axiom-cpp-native",
            "implementationKind": "CPP_NATIVE",
            "execution": {
                "runnerVersion": "0.1.0",
                "runtimeVersion": "0.1.0",
                "platform": "linux-x64",
                "buildProfile": "release",
            },
            "terminal": {"stage": "NORMALIZE", "outcome": "ACCEPTED"},
            "stages": [
                {"stage": "DECODE", "outcome": "ACCEPTED"},
                {"stage": "NORMALIZE", "outcome": "ACCEPTED"},
            ],
            "artifacts": {},
            "checkpoints": [],
            "diagnostics": [],
        }

    def result(self):
        return {
            "format": "axiom-conformance-result-v1",
            "formatVersion": 1,
            "vectorId": "WIRE-F64-NEGZERO-001",
            "requirementStatus": "FREEZE_CANDIDATE",
            "result": "PASS",
            "participants": [
                {
                    "implementationId": "axiom-cpp-native",
                    "implementationKind": "CPP_NATIVE",
                    "policy": "REQUIRED",
                    "participation": "RAN",
                    "observation": "observations/axiom-cpp-native.json",
                }
            ],
            "comparison": {
                "golden": "CHECKED",
                "crossImplementation": "NOT_ENOUGH_PARTICIPANTS",
                "comparedImplementations": ["axiom-cpp-native"],
            },
            "divergence": None,
            "diagnostics": [],
        }

    def test_required_schema_files_are_materialized(self):
        for name in ("case", "observation", "result", "run"):
            self.assertTrue((ac.SCHEMAS / f"{name}.schema.json").is_file(), name)

    def test_valid_observation_passes_semantic_validation(self):
        ac.validate_observation_semantics(self.observation())

    def test_unknown_observation_top_level_field_rejected(self):
        obs = self.observation()
        obs["implementationPass"] = True
        with self.assertRaisesRegex(ValueError, "unknown top-level"):
            ac.validate_observation_semantics(obs)

    def test_duplicate_stage_rejected(self):
        obs = self.observation()
        obs["stages"].append(copy.deepcopy(obs["stages"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate stage"):
            ac.validate_observation_semantics(obs)

    def test_checkpoint_order_rejected(self):
        obs = self.observation()
        obs["checkpoints"] = [
            {
                "operationIndex": 2,
                "operationId": "id128:00000000000000000000000000000002",
                "projection": "observations/cpp/checkpoints/00000002.projection.json",
            },
            {
                "operationIndex": 1,
                "operationId": "id128:00000000000000000000000000000001",
                "projection": "observations/cpp/checkpoints/00000001.projection.json",
            },
        ]
        with self.assertRaisesRegex(ValueError, "strictly ascending"):
            ac.validate_observation_semantics(obs)

    def test_pass_with_divergence_rejected(self):
        result = self.result()
        result["divergence"] = {
            "kind": "OUTCOME",
            "basis": "GOLDEN",
            "reference": {"source": "GOLDEN", "value": "ACCEPTED"},
            "observed": [{"implementationId": "axiom-cpp-native", "value": "REJECTED"}],
            "summary": "unexpected mismatch",
        }
        with self.assertRaisesRegex(ValueError, "PASS requires divergence=null"):
            ac.validate_result_semantics(result)

    def test_open_cross_implementation_divergence_cannot_claim_golden_basis(self):
        result = self.result()
        result["requirementStatus"] = "OPEN"
        result["result"] = "OBSERVED_DIVERGENCE_OPEN"
        result["comparison"] = {
            "golden": "NOT_APPLICABLE",
            "crossImplementation": "CHECKED",
            "comparedImplementations": ["axiom-cpp-native", "axiom-wasm"],
        }
        result["participants"] = []
        result["divergence"] = {
            "kind": "OUTCOME",
            "basis": "GOLDEN",
            "reference": {"source": "GOLDEN", "value": "ACCEPTED"},
            "observed": [
                {"implementationId": "axiom-cpp-native", "value": "REJECTED"},
                {"implementationId": "axiom-wasm", "value": "ACCEPTED"},
            ],
            "summary": "OPEN implementations differ",
        }
        with self.assertRaisesRegex(ValueError, "CROSS_IMPLEMENTATION"):
            ac.validate_result_semantics(result)

    def test_required_capability_failure_requires_missing_participant(self):
        result = self.result()
        result["result"] = "FAIL_CAPABILITY_MISSING"
        with self.assertRaisesRegex(ValueError, "NOT_CAPABLE or MISSING"):
            ac.validate_result_semantics(result)

    def test_cross_implementation_divergence_cannot_have_reference(self):
        divergence = {
            "kind": "OUTCOME",
            "basis": "CROSS_IMPLEMENTATION",
            "reference": {"source": "GOLDEN", "value": "ACCEPTED"},
            "observed": [
                {"implementationId": "axiom-cpp-native", "value": "REJECTED"},
                {"implementationId": "axiom-wasm", "value": "ACCEPTED"},
            ],
            "summary": "implementations differ",
        }
        with self.assertRaisesRegex(ValueError, "reference must be absent"):
            ac.validate_divergence_semantics(divergence)

    def test_case_open_policy_cannot_claim_frozen_expected(self):
        case = {
            "formatVersion": 1,
            "id": "OP-DELETE-CONNECTOR-OPEN-001",
            "title": "OPEN policy observation",
            "status": "OPEN",
            "category": "operation.delete.connector",
            "authorityRefs": ["Semantic Schema authority"],
            "entrypoint": "APPLY",
            "requiredCapabilities": ["APPLY_OPERATION"],
            "input": {"kind": "ENCODED_OPERATION", "rootType": "auditoryworks.axiom.v1.Operation", "artifact": "input/op.pb"},
            "expected": {"outcome": "ACCEPTED"},
            "capture": {},
            "blockedByOpenPolicy": True,
            "notes": "",
        }
        with self.assertRaisesRegex(ValueError, "OPEN case"):
            ac.validate_case_semantics(case)


if __name__ == "__main__":
    unittest.main()
