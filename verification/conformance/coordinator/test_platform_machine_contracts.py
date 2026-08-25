#!/usr/bin/env python3
"""MR-10-04 core Platform Machine Contract meta-tests."""
from __future__ import annotations

import copy
import unittest

import platform_contracts as pc


class PlatformMachineContractTests(unittest.TestCase):
    def scenario(self, status="SPEC_REQUIREMENT"):
        return {
            "format": "axiom-platform-scenario-v1",
            "formatVersion": 1,
            "id": "PLAT-SURFACE-LOST-001",
            "requirementStatus": status,
            "requirementIds": ["PC-I05"],
            "authorityRefs": ["https://app.notion.com/p/3c44c57a590c81159b0bfca5e6963851"],
            "canonicalFixtureRef": "REPLAY-MIXED-OPERATIONS-001",
            "targets": [{"platformFamily": "ANDROID", "policy": "REQUIRED", "requiredCapabilities": ["surface.generation", "surface.loss.inject"]}],
            "preconditions": {
                "appState": "FOREGROUND", "canvasState": "RUNNING", "hostState": "ATTACHED",
                "documentState": "ATTACHED", "surfaceState": "BOUND", "deviceState": "READY",
                "arcPreviewMode": "DISABLED",
            },
            "steps": [
                {"stepId": "s01", "kind": "FAULT", "action": {"faultId": "f01", "type": "SURFACE_LOST", "mode": "PULSE"},
                 "completion": {"mode": "WAIT_FOR_EVENT", "event": {"traceKind": "SURFACE", "event": "SURFACE_UNAVAILABLE", "occurrence": "FIRST"}}}
            ],
            "expected": {
                "requiredEvents": [{"id": "evt-loss", "selector": {"traceKind": "SURFACE", "event": "SURFACE_UNAVAILABLE", "occurrence": "ANY"}, "minCount": 1}],
                "forbiddenEvents": [], "partialOrder": [], "stateAssertions": [], "openObservations": [],
            },
            "capture": {"lifecycleTrace": True, "surfaceTrace": True, "bridgeTrace": False, "semanticCheckpoints": [], "stateCheckpoints": [], "ownershipSnapshot": False},
        }

    def trace(self, events=None):
        return {
            "format": "axiom-platform-trace-v1", "formatVersion": 1,
            "scenarioId": "PLAT-SURFACE-LOST-001", "profileId": "android-reference",
            "traceKind": "SURFACE",
            "events": events if events is not None else [
                {"eventSeq": "u64:0000000000000001", "event": "SURFACE_BOUND", "target": "CANONICAL_PRESENTATION", "surfaceGeneration": "u64:0000000000000001"},
                {"eventSeq": "u64:0000000000000002", "event": "SURFACE_UNAVAILABLE", "surfaceGeneration": "u64:0000000000000001"},
            ],
        }

    def profile(self):
        return {
            "format": "axiom-platform-profile-v1", "formatVersion": 1,
            "profileId": "android-reference", "platformFamily": "ANDROID", "platformVariant": "ANDROID_NORMAL_CLIENT",
            "capabilities": ["surface.generation", "surface.loss.inject"],
            "realization": {"renderBackend": "candidate-vulkan", "surfacePrimitive": "platform-private", "bridgeMechanism": ["JNI", "JSI"], "arcPreviewEnabled": False},
        }

    def observation(self):
        return {
            "format": "axiom-platform-observation-v1", "formatVersion": 1,
            "scenarioId": "PLAT-SURFACE-LOST-001", "profileId": "android-reference", "platformFamily": "ANDROID",
            "execution": {"runnerVersion": "0.1.0", "runtimeVersion": "0.1.0", "platformHarnessVersion": "0.1.0", "buildProfile": "verification"},
            "terminal": {"outcome": "COMPLETED"},
            "steps": [{"stepId": "s01", "outcome": "COMPLETED"}],
            "artifacts": {"lifecycleTrace": "traces/lifecycle.json", "surfaceTrace": "traces/surface.json"},
            "semanticCheckpoints": [], "stateCheckpoints": [],
            "targetBindings": [{"role": "CANONICAL", "bindingTag": "binding:canonical:1", "owner": "AXIOM", "surfaceGeneration": "u64:0000000000000001"}],
            "realization": self.profile()["realization"], "diagnostics": [],
        }

    def result(self, requirement_status="SPEC_REQUIREMENT", result="PASS"):
        return {
            "format": "axiom-platform-conformance-result-v1", "formatVersion": 1,
            "scenarioId": "PLAT-SURFACE-LOST-001", "requirementStatus": requirement_status, "result": result,
            "participants": [{"platformFamily": "ANDROID", "profileId": "android-reference", "policy": "REQUIRED", "participation": "RAN", "observation": "android/observation.json"}],
            "checks": [{"checkId": "evt-loss", "kind": "REQUIRED_EVENT", "status": "PASS", "participants": ["android-reference"], "evidence": ["android/traces/surface.json"]}],
            "openObservations": [], "divergence": None, "diagnostics": [],
        }

    def test_core_six_platform_schemas_are_materialized(self):
        pc.validate_platform_schema_inventory()

    def test_meta_platform_scenario_unknown_field_reject(self):
        scenario = self.scenario(); scenario["platformWinner"] = "SurfaceControl"
        with self.assertRaisesRegex(ValueError, "JSON Schema"):
            pc.validate_platform_structure("platform-scenario", scenario)

    def test_meta_platform_scenario_duplicate_step_id_reject(self):
        scenario = self.scenario(); scenario["steps"].append(copy.deepcopy(scenario["steps"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate stepId"):
            pc.validate_platform_scenario_semantics(scenario)

    def test_meta_platform_scenario_spec_without_oracle_reject(self):
        scenario = self.scenario(); scenario["expected"] = {"requiredEvents": [], "forbiddenEvents": [], "partialOrder": [], "stateAssertions": [], "openObservations": []}
        with self.assertRaisesRegex(ValueError, "oracle"):
            pc.validate_platform_scenario_semantics(scenario)

    def test_meta_platform_trace_duplicate_eventseq_reject(self):
        trace = self.trace(); trace["events"][1]["eventSeq"] = trace["events"][0]["eventSeq"]
        with self.assertRaisesRegex(ValueError, "eventSeq"):
            pc.validate_platform_trace_semantics(trace)

    def test_meta_platform_trace_nonmonotonic_eventseq_reject(self):
        trace = self.trace(); trace["events"][0]["eventSeq"] = "u64:0000000000000002"; trace["events"][1]["eventSeq"] = "u64:0000000000000001"
        with self.assertRaisesRegex(ValueError, "eventSeq"):
            pc.validate_platform_trace_semantics(trace)

    def test_meta_platform_trace_surface_bound_without_generation_reject(self):
        trace = self.trace(); del trace["events"][0]["surfaceGeneration"]
        with self.assertRaisesRegex(ValueError, "surfaceGeneration"):
            pc.validate_platform_trace_semantics(trace)

    def test_meta_platform_observation_missing_requested_trace_reject(self):
        observation = self.observation(); del observation["artifacts"]["surfaceTrace"]
        with self.assertRaisesRegex(ValueError, "surfaceTrace"):
            pc.validate_platform_observation_semantics(observation, self.scenario(), self.profile())

    def test_meta_platform_observation_real_handle_in_bindingtag_reject(self):
        observation = self.observation(); observation["targetBindings"][0]["bindingTag"] = "0x00007ffee123abcd"
        with self.assertRaisesRegex(ValueError, "bindingTag"):
            pc.validate_platform_observation_semantics(observation, self.scenario(), self.profile())

    def test_meta_platform_result_pass_with_divergence_reject(self):
        result = self.result(); result["divergence"] = {"kind": "REQUIRED_EVENT", "summary": "missing event"}
        with self.assertRaisesRegex(ValueError, "JSON Schema|PASS"):
            pc.validate_platform_result_semantics(result, self.scenario())

    def test_meta_platform_result_open_scenario_pass_reject(self):
        scenario = self.scenario("OPEN"); scenario["expected"] = {"requiredEvents": [], "forbiddenEvents": [], "partialOrder": [], "stateAssertions": [], "openObservations": [{"id": "obs-backend", "key": "RENDER_BACKEND"}]}
        result = self.result(requirement_status="OPEN", result="PASS")
        with self.assertRaisesRegex(ValueError, "OPEN scenario"):
            pc.validate_platform_result_semantics(result, scenario)

    def test_meta_platform_result_required_notcapable_without_fail_reject(self):
        result = self.result(); result["participants"][0]["participation"] = "NOT_CAPABLE"; result["participants"][0].pop("observation")
        with self.assertRaisesRegex(ValueError, "FAIL_CAPABILITY_MISSING"):
            pc.validate_platform_result_semantics(result, self.scenario())

    def test_open_physical_realization_is_profile_metadata_not_enum_winner(self):
        profile = self.profile(); profile["realization"]["renderBackend"] = "experiment-backend-not-frozen-by-08"; profile["realization"]["surfacePrimitive"] = "experiment-surface-not-frozen-by-08"
        pc.validate_platform_profile_semantics(profile)


if __name__ == "__main__":
    unittest.main()
