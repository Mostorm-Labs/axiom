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
        self.assertNotIn("poc03", trigger.lower())
        self.assertNotIn("needs:", trigger)

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
