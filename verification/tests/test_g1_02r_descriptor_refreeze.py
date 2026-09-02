"""GT-G1-02R descriptor reconciliation closure.

The historical descriptor is deliberately read from the accepted reconciliation
baseline.  It is comparison input, not current authority: all accepted changes
must map to the current RichText / Stroke reconciliation defect registry.
"""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTHORITY_BASELINE = "e7a0581706b4e4370fd397ccffef81aa84e48a27"
SPEC = importlib.util.spec_from_file_location(
    "descriptor_refreeze_diff",
    ROOT / "verification/tools/descriptor_refreeze_diff.py",
)
assert SPEC and SPEC.loader
descriptor_diff = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = descriptor_diff
SPEC.loader.exec_module(descriptor_diff)


class G102RDescriptorRefreezeTest(unittest.TestCase):
    def test_every_descriptor_change_maps_to_a_closed_machine_projection_defect(self) -> None:
        before = subprocess.check_output(
            [
                "git",
                "show",
                f"{AUTHORITY_BASELINE}:schema/axiom/v1/descriptor/descriptor.lock.pb",
            ],
            cwd=ROOT,
        )
        after = (ROOT / "schema/axiom/v1/descriptor/descriptor.lock.pb").read_bytes()
        result = descriptor_diff.compare(before, after, authority_baseline=AUTHORITY_BASELINE)

        self.assertNotEqual(result["beforeDescriptorSha256"], result["afterDescriptorSha256"])
        self.assertEqual([], result["unmappedChanges"])
        self.assertEqual(
            {"IMG-05", "RT-D01", "RT-D02", "RT-D03", "RT-D04", "RT-D05", "RT-D06", "ST-D01", "ST-D02", "ST-D03", "ST-D04", "ST-D05"},
            {defect for change in result["changes"] for defect in change["defectIds"]},
        )

    def test_outer_object_content_and_operation_registry_identity_is_preserved(self) -> None:
        before = subprocess.check_output(
            [
                "git",
                "show",
                f"{AUTHORITY_BASELINE}:schema/axiom/v1/descriptor/descriptor.lock.pb",
            ],
            cwd=ROOT,
        )
        after = (ROOT / "schema/axiom/v1/descriptor/descriptor.lock.pb").read_bytes()
        result = descriptor_diff.compare(before, after, authority_baseline=AUTHORITY_BASELINE)

        self.assertTrue(result["outerRegistryPreserved"]["ObjectContent"])
        self.assertTrue(result["outerRegistryPreserved"]["Operation"])

    def test_written_review_artifact_is_deterministic_and_machine_readable(self) -> None:
        before = subprocess.check_output(
            [
                "git",
                "show",
                f"{AUTHORITY_BASELINE}:schema/axiom/v1/descriptor/descriptor.lock.pb",
            ],
            cwd=ROOT,
        )
        after = (ROOT / "schema/axiom/v1/descriptor/descriptor.lock.pb").read_bytes()
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory) / "descriptor-refreeze-diff.json"
            first = descriptor_diff.write_review_artifact(before, after, artifact, authority_baseline=AUTHORITY_BASELINE)
            serialized_once = artifact.read_bytes()
            second = descriptor_diff.write_review_artifact(before, after, artifact, authority_baseline=AUTHORITY_BASELINE)
            serialized_twice = artifact.read_bytes()

        self.assertEqual(first, second)
        self.assertEqual(serialized_once, serialized_twice)
        self.assertEqual(first, json.loads(serialized_once))


if __name__ == "__main__":
    unittest.main()
