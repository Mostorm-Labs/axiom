from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github/workflows"


def trigger_block(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8").split(
        "concurrency:", maxsplit=1
    )[0]


class CiTriggerBoundaryTest(unittest.TestCase):
    def test_poc03_trigger_is_its_positive_dependency_allowlist(self):
        trigger = trigger_block("poc03.yml")
        required = (
            '"r1-full-skia-sdk.lock.json"',
            '"pocs/large_scene/**"',
            '"pocs/ink_engine/**"',
            '"runtime/foundation/**"',
            '"runtime/scene/**"',
            '"CMakeLists.txt"',
            '"CMakePresets.json"',
            '"cmake/**"',
            '"docs/api/canvas_runtime_api_v1.h"',
            '"docs/api/canvas_runtime_api_v1.manifest.json"',
            '"tools/check_runtime_abi_manifest.py"',
            '"tools/check_runtime_boundaries.py"',
        )
        for path in required:
            self.assertIn(path, trigger)
        self.assertNotIn('"runtime/**"', trigger)
        self.assertNotIn('"runtime/semantic/**"', trigger)

    def test_g1_semantic_lane_owns_semantic_inputs_without_poc03_dependency(self):
        trigger = trigger_block("g1-semantic-codec.yml")
        self.assertIn('"runtime/semantic/**"', trigger)
        self.assertIn('"schema/axiom/v1/**"', trigger)
        self.assertIn('"tools/setup_build_environment.py"', trigger)
        self.assertNotIn("poc03", trigger.lower())
        self.assertNotIn("needs:", trigger)
        workflow = (WORKFLOWS / "g1-semantic-codec.yml").read_text(encoding="utf-8")
        self.assertIn("tools/setup_build_environment.py --core --semantic", workflow)
        self.assertIn('-DCMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_RUNTIME_ROOT"', workflow)
        self.assertNotIn("bootstrap_deps.py --semantic-codec", workflow)
        self.assertNotIn("-DProtobuf_DIR=", workflow)
        self.assertNotIn("-Dabsl_DIR=", workflow)
        self.assertNotIn("-Dutf8_range_DIR=", workflow)

    def test_build_environment_contract_is_consumer_only(self):
        workflow = (WORKFLOWS / "build-environment-contract.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("tools/setup_build_environment.py --core --semantic", workflow)
        self.assertIn("tools/skia/fetch.py", workflow)
        self.assertIn("r1-full-skia-sdk.lock.json", workflow)
        self.assertIn("android-x86_64-gles3", workflow)
        self.assertNotIn("bootstrap_deps.py --semantic-codec", workflow)
        self.assertNotIn("tools/skia/build.py", workflow)
        self.assertNotIn("git-sync-deps", workflow)
        self.assertNotIn("gh release create", workflow)

    def test_semantic_producer_keeps_business_changes_out_and_checks_new_orchestration(self):
        workflow = (WORKFLOWS / "semantic-sdk-producer-contract.yml").read_text()
        triggers = workflow.split("permissions:", 1)[0]
        self.assertNotIn('"runtime/semantic/**"', triggers)
        for path in ('"tools/semantic/**"', '"tools/sdk/**"',
                     '".github/workflows/semantic-sdk-producer.yml"',
                     '"verification/tests/test_semantic_sdk_workflows.py"'):
            self.assertIn(path, triggers)
        self.assertIn("needs.classify.outputs.should_build == 'true'", workflow)
        self.assertIn("uses: ./.github/workflows/semantic-sdk-producer.yml", workflow)

    def test_semantic_v2_consumer_validation_has_transition_gate_and_exact_matrix(self):
        workflow = (WORKFLOWS / "semantic-sdk-consumer-validation.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_call:", workflow)
        self.assertIn("checkout_ref:", workflow)
        self.assertIn("ref: ${{ inputs.checkout_ref ||", workflow)
        self.assertIn("SEMANTIC_V2_NOT_LOCKED", workflow)
        self.assertIn("semantic-sdk.lock.json", workflow)
        for key in (
            "linux-x86_64", "windows-x64-msvc-static", "macos-arm64", "macos-x64",
            "ios-arm64", "ios-simulator-arm64", "android-arm64-v8a",
            "android-x86_64", "web-wasm32",
        ):
            self.assertIn(key, workflow)
        self.assertIn("tools/setup_build_environment.py --semantic", workflow)
        self.assertNotIn("AXIOM_SDK_STORE: ${{ runner.temp }}", workflow)
        self.assertIn('AXIOM_SEMANTIC_RUNTIME_ROOT', workflow)
        self.assertIn('AXIOM_PROTOC', workflow)
        self.assertNotIn("bootstrap_deps.py --semantic-codec", workflow)
        for forbidden in ("sdk_bytes", "sdk_url", "sdk_identity", "release_url"):
            self.assertNotIn(forbidden, workflow.lower())

    def test_build_environment_delegates_v2_matrix_without_rewriting_skia_consumer(self):
        workflow = (WORKFLOWS / "build-environment-contract.yml").read_text(encoding="utf-8")
        self.assertIn("uses: ./.github/workflows/semantic-sdk-consumer-validation.yml", workflow)
        self.assertIn("semantic-sdk.lock.json", workflow)
        skia = workflow.split("  skia-historical-consumer:", 1)[1].split("\n  sdk-infrastructure:", 1)[0]
        self.assertIn("tools/skia/fetch.py", skia)
        self.assertIn("android-x86_64-gles3", skia)
        self.assertNotIn("semantic-sdk-consumer-validation", skia)

    def test_g1_v2_evidence_uses_store_manifests_and_retains_v1_transition_marker(self):
        workflow = (WORKFLOWS / "g1-semantic-codec.yml").read_text(encoding="utf-8")
        self.assertIn("semantic-host-manifest.json", workflow)
        self.assertIn("semantic-runtime-manifest.json", workflow)
        self.assertIn("semantic-sdk.lock.json", workflow)
        self.assertIn(".deps/protobuf/.canvas-semantic-toolchain.json", workflow)
        self.assertIn("AXIOM_SEMANTIC_HOST_ROOT", workflow)
        self.assertIn("AXIOM_SEMANTIC_RUNTIME_ROOT", workflow)

    def test_g103_evidence_does_not_promote_poc03_to_gate_authority(self):
        generator = (ROOT / "verification/tools/generate_g1_03_evidence.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("CI_TRIGGER_IS_NOT_GATE_AUTHORITY", generator)
        self.assertIn('"poc03RequiredForGate": False', generator)
        self.assertNotIn("poc03.yml", generator.lower())

    def test_cheap_boundary_contract_runs_only_static_python_validation(self):
        workflow = (WORKFLOWS / "ci-boundary-contract.yml").read_text(encoding="utf-8")
        self.assertIn("verification.tests.test_ci_trigger_boundaries", workflow)
        self.assertIn("ubuntu-24.04", workflow)
        self.assertNotIn("skia", workflow.lower())
        self.assertNotIn("android", workflow.lower())
        self.assertNotIn("windows", workflow.lower())


if __name__ == "__main__":
    unittest.main()
