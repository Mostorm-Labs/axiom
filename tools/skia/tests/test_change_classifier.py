from __future__ import annotations

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from classify_r1_changes import ALL_TARGETS, classify


class ChangeClassifierTest(unittest.TestCase):
    def test_semantic_runtime_changes_do_not_trigger_scene_workflows(self) -> None:
        workflows = Path(__file__).resolve().parents[3] / ".github/workflows"
        for name in ("poc03.yml", "rf01.yml"):
            trigger = (workflows / name).read_text(encoding="utf-8").split(
                "concurrency:", maxsplit=1
            )[0]
            self.assertNotIn('"runtime/**"', trigger)
            self.assertNotIn('"tools/bootstrap_deps.py"', trigger)
            self.assertIn('"runtime/scene/**"', trigger)

    def test_lock_change_with_only_semantic_dependencies_does_not_trigger_skia(self) -> None:
        before = {
            "dependencies": {
                "skia": {"commit": "a"},
                "protobuf": {"version": "35.0"},
                "abseil": {"version": "2024"},
            },
            "skia_builds": {"windows_d3d12": {"skia_enable_ganesh": True}},
        }
        after = copy.deepcopy(before)
        after["dependencies"]["protobuf"]["version"] = "36.0"
        after["dependencies"]["abseil"]["version"] = "20250512.1"
        result = classify(["deps.lock.json"], lock_before=before, lock_after=after)
        self.assertEqual(result["mode"], "none")
        self.assertIn("semantic", result["reason"])

    def test_lock_change_to_skia_identity_triggers_full_matrix(self) -> None:
        before = {
            "dependencies": {"skia": {"commit": "a"}, "protobuf": {"version": "35.0"}},
            "skia_builds": {"windows_d3d12": {"skia_enable_ganesh": True}},
        }
        after = copy.deepcopy(before)
        after["dependencies"]["skia"]["commit"] = "b"
        result = classify(["deps.lock.json"], lock_before=before, lock_after=after)
        self.assertEqual(result["mode"], "full")
        self.assertEqual(result["targets"], ALL_TARGETS)

    def test_mixed_semantic_and_skia_lock_change_remains_full(self) -> None:
        before = {
            "dependencies": {
                "skia": {"commit": "a"},
                "protobuf": {"version": "35.0"},
            },
            "skia_builds": {"windows_d3d12": {"skia_enable_ganesh": True}},
        }
        after = copy.deepcopy(before)
        after["dependencies"]["skia"]["commit"] = "b"
        after["dependencies"]["protobuf"]["version"] = "36.0"
        result = classify(["deps.lock.json"], lock_before=before, lock_after=after)
        self.assertEqual(result["mode"], "full")

    def test_lock_change_without_documents_uses_conservative_full_fallback(self) -> None:
        result = classify(["deps.lock.json"])
        self.assertEqual(result["mode"], "full")
        self.assertIn("lock diff unavailable", result["reason"])

    def test_r1_classifier_orchestration_changes_do_not_start_skia_jobs(self) -> None:
        result = classify([
            ".github/workflows/r1-full-producer-contract.yml",
            "tools/skia/classify_r1_changes.py",
        ])
        self.assertEqual(result["mode"], "none")
        self.assertIn("classification", result["reason"])


if __name__ == "__main__":
    unittest.main()
