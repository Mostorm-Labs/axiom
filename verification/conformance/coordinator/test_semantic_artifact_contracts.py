#!/usr/bin/env python3
"""MR-10-01 / MR-10-03 semantic artifact contract tests."""
from __future__ import annotations

import copy
import os
import unittest

import axiom_conformance as ac


class SemanticArtifactContractTests(unittest.TestCase):
    def observation(self):
        return {
            "format": "axiom-conformance-observation-v1", "formatVersion": 1,
            "runnerProtocolVersion": 1, "vectorId": "WIRE-F64-NEGZERO-001",
            "implementationId": "axiom-cpp-native", "implementationKind": "CPP_NATIVE",
            "execution": {"runnerVersion": "0.1.0", "runtimeVersion": "0.1.0", "platform": "linux-x64", "buildProfile": "release"},
            "terminal": {"stage": "NORMALIZE", "outcome": "ACCEPTED"},
            "stages": [{"stage": "DECODE", "outcome": "ACCEPTED"}, {"stage": "NORMALIZE", "outcome": "ACCEPTED"}],
            "artifacts": {}, "checkpoints": [], "diagnostics": [],
        }

    def result(self):
        return {
            "format": "axiom-conformance-result-v1", "formatVersion": 1,
            "vectorId": "WIRE-F64-NEGZERO-001", "requirementStatus": "FREEZE_CANDIDATE", "result": "PASS",
            "participants": [{"implementationId": "axiom-cpp-native", "implementationKind": "CPP_NATIVE", "policy": "REQUIRED", "participation": "RAN", "observation": "observations/axiom-cpp-native.json"}],
            "comparison": {"golden": "CHECKED", "crossImplementation": "NOT_ENOUGH_PARTICIPANTS", "comparedImplementations": ["axiom-cpp-native"]},
            "divergence": None, "diagnostics": [],
        }

    def golden_divergence(self, kind="OUTCOME"):
        return {
            "kind": kind,
            "basis": "GOLDEN",
            "reference": {"source": "GOLDEN", "value": "ACCEPTED"},
            "observed": [{"implementationId": "axiom-cpp-native", "value": "REJECTED"}],
            "summary": "expected mismatch",
        }

    def projection(self, root_type="auditoryworks.axiom.v1.Vec2", value=None, form="CANONICAL"):
        return {"format": "axiom-verification-projection-v1", "formatVersion": 1, "semanticSchemaVersion": 1, "rootType": root_type, "form": form, "value": {"x": "f64:3ff4000000000000", "y": "f64:c004000000000000"} if value is None else value}

    def test_required_schema_files_are_materialized(self):
        for name in ("case", "observation", "result", "run"):
            self.assertTrue((ac.SCHEMAS / f"{name}.schema.json").is_file(), name)

    def test_real_json_schema_rejects_nested_invalid_observation(self):
        obs = self.observation(); obs["terminal"]["outcome"] = "IMPLEMENTATION_SAYS_OK"
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_structure("observation", obs)

    def test_all_verification_schemas_are_valid_draft_2020_12(self): ac.validate_schema_inventory()
    def test_valid_observation_passes_semantic_validation(self): ac.validate_observation_semantics(self.observation())

    def test_unknown_observation_top_level_field_rejected(self):
        obs = self.observation(); obs["implementationPass"] = True
        with self.assertRaisesRegex(ValueError, "JSON Schema|unknown top-level"): ac.validate_observation_semantics(obs)

    def test_duplicate_stage_rejected(self):
        obs = self.observation(); obs["stages"].append(copy.deepcopy(obs["stages"][0]))
        with self.assertRaisesRegex(ValueError, "duplicate stage"): ac.validate_observation_semantics(obs)

    def test_checkpoint_order_rejected(self):
        obs = self.observation(); obs["checkpoints"] = [{"operationIndex": 2, "operationId": "id128:00000000000000000000000000000002", "projection": "observations/cpp/checkpoints/00000002.projection.json"}, {"operationIndex": 1, "operationId": "id128:00000000000000000000000000000001", "projection": "observations/cpp/checkpoints/00000001.projection.json"}]
        with self.assertRaisesRegex(ValueError, "strictly ascending"): ac.validate_observation_semantics(obs)

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_root_type_must_exist_in_frozen_idl(self):
        with self.assertRaisesRegex(ValueError, "rootType.*not found"): ac.validate_projection_against_idl(self.projection("auditoryworks.axiom.v1.DoesNotExist"))

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_message_rejects_unknown_field(self):
        with self.assertRaisesRegex(ValueError, "unknown field"): ac.validate_projection_against_idl(self.projection(value={"x": "f64:3ff0000000000000", "y": "f64:4000000000000000", "z": "f64:4008000000000000"}))

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_message_rejects_wrong_scalar_type(self):
        with self.assertRaisesRegex(ValueError, "expected floating-point"): ac.validate_projection_against_idl(self.projection(value={"x": "1.0", "y": 2.0}, form="DECODED"))

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_repeated_field_requires_array(self):
        with self.assertRaisesRegex(ValueError, "expected array"): ac.validate_projection_against_idl(self.projection("auditoryworks.axiom.v1.PropertyBag", {"entries": {}}))

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_oneof_rejects_multiple_members(self):
        with self.assertRaisesRegex(ValueError, "oneof"): ac.validate_projection_against_idl(self.projection("auditoryworks.axiom.v1.PropertyValue", {"boolValue": True, "f32Value": "f32:3f800000"}))

    @unittest.skipUnless(os.environ.get("AXIOM_V1_DESCRIPTOR"), "CI supplies frozen descriptor evidence")
    def test_projection_f32_uses_tagged_canonical_scalar(self): ac.validate_projection_against_idl(self.projection("auditoryworks.axiom.v1.PropertyValue", {"f32Value": "f32:3f800000"}))

    def test_pass_with_divergence_rejected(self):
        result = self.result(); result["divergence"] = self.golden_divergence()
        with self.assertRaisesRegex(ValueError, "JSON Schema|PASS requires divergence=null"): ac.validate_result_semantics(result)

    def test_open_cross_implementation_divergence_cannot_claim_golden_basis(self):
        result = self.result(); result["requirementStatus"] = "OPEN"; result["result"] = "OBSERVED_DIVERGENCE_OPEN"; result["participants"] = []; result["comparison"] = {"golden": "NOT_APPLICABLE", "crossImplementation": "CHECKED", "comparedImplementations": ["axiom-cpp-native", "axiom-wasm"]}; result["divergence"] = {"kind": "OUTCOME", "basis": "GOLDEN", "reference": {"source": "GOLDEN", "value": "ACCEPTED"}, "observed": [{"implementationId": "axiom-cpp-native", "value": "REJECTED"}, {"implementationId": "axiom-wasm", "value": "ACCEPTED"}], "summary": "OPEN implementations differ"}
        with self.assertRaisesRegex(ValueError, "CROSS_IMPLEMENTATION"): ac.validate_result_semantics(result)

    def test_required_capability_failure_requires_missing_participant(self):
        result = self.result(); result["result"] = "FAIL_CAPABILITY_MISSING"
        with self.assertRaisesRegex(ValueError, "NOT_CAPABLE or MISSING"): ac.validate_result_semantics(result)

    def test_cross_implementation_divergence_cannot_have_reference(self):
        divergence = {"kind": "OUTCOME", "basis": "CROSS_IMPLEMENTATION", "reference": {"source": "GOLDEN", "value": "ACCEPTED"}, "observed": [{"implementationId": "axiom-cpp-native", "value": "REJECTED"}, {"implementationId": "axiom-wasm", "value": "ACCEPTED"}], "summary": "implementations differ"}
        with self.assertRaisesRegex(ValueError, "reference must be absent"): ac.validate_divergence_semantics(divergence)

    def test_mr1003_golden_divergence_requires_reference(self):
        result = self.result(); result["result"] = "FAIL_GOLDEN_MISMATCH"; result["divergence"] = self.golden_divergence(); del result["divergence"]["reference"]
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_result_semantics(result)

    def test_mr1003_cross_implementation_requires_two_observed_operands(self):
        result = self.result(); result["requirementStatus"] = "OPEN"; result["result"] = "OBSERVED_DIVERGENCE_OPEN"; result["participants"] = []; result["comparison"] = {"golden": "NOT_APPLICABLE", "crossImplementation": "CHECKED", "comparedImplementations": ["axiom-cpp-native", "axiom-wasm"]}; result["divergence"] = {"kind": "OUTCOME", "basis": "CROSS_IMPLEMENTATION", "observed": [{"implementationId": "axiom-cpp-native", "value": "REJECTED"}], "summary": "only one operand"}
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_result_semantics(result)

    def test_mr1003_operation_index_and_id_are_atomic_location_pair(self):
        result = self.result(); result["result"] = "FAIL_GOLDEN_MISMATCH"; result["divergence"] = self.golden_divergence(); result["divergence"]["operationIndex"] = 42
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_result_semantics(result)

    def test_mr1003_semantic_path_uses_verification_path_grammar(self):
        result = self.result(); result["result"] = "FAIL_GOLDEN_MISMATCH"; result["divergence"] = self.golden_divergence("SEMANTIC_PROJECTION"); result["divergence"]["semanticPath"] = "document->objects[0]"
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_result_semantics(result)

    def test_mr1003_byte_offset_is_only_for_canonical_bytes(self):
        result = self.result(); result["result"] = "FAIL_GOLDEN_MISMATCH"; result["divergence"] = self.golden_divergence("SEMANTIC_PROJECTION"); result["divergence"]["byteOffset"] = 7
        with self.assertRaisesRegex(ValueError, "JSON Schema"): ac.validate_result_semantics(result)

    def test_mr1003_valid_semantic_path_selector_is_accepted(self):
        result = self.result(); result["result"] = "FAIL_GOLDEN_MISMATCH"; result["divergence"] = self.golden_divergence("SEMANTIC_PROJECTION"); result["divergence"]["semanticPath"] = "$.objects[id=id128:00000000000000000000000000000011].transform.tx"; result["divergence"]["reference"] = {"source": "GOLDEN", "artifact": "expected/final.projection.json"}; result["divergence"]["observed"] = [{"implementationId": "axiom-cpp-native", "artifact": "observations/cpp/final.projection.json"}]
        ac.validate_result_semantics(result)

    def test_case_open_policy_cannot_claim_frozen_expected(self):
        case = {"formatVersion": 1, "id": "OP-DELETE-CONNECTOR-OPEN-001", "title": "OPEN policy observation", "status": "OPEN", "category": "operation.delete.connector", "authorityRefs": ["Semantic Schema authority"], "entrypoint": "APPLY", "requiredCapabilities": ["APPLY_OPERATION"], "input": {"kind": "ENCODED_OPERATION", "rootType": "auditoryworks.axiom.v1.Operation", "artifact": "input/op.pb"}, "expected": {"outcome": "ACCEPTED"}, "capture": {}, "blockedByOpenPolicy": True, "notes": ""}
        with self.assertRaisesRegex(ValueError, "OPEN case"): ac.validate_case_semantics(case)


if __name__ == "__main__": unittest.main()
