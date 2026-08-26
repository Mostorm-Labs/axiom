from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "tools/semantic/classify_producer_changes.py"
SPEC = importlib.util.spec_from_file_location("classify_producer_changes", SCRIPT)
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class ProducerChangeClassifierTest(unittest.TestCase):
    def _lock(self) -> dict:
        return {
            "schema_version": 1,
            "dependencies": {
                "protobuf": {"version": "36.0", "source_sha256": "a"},
                "abseil": {"version": "20250512.1", "source_sha256": "b"},
                "skia": {"commit": "skia"},
                "node": {"version": "24"},
            },
        }

    def test_consumer_only_paths_do_not_start_producer(self) -> None:
        result = module.classify([
            "tools/semantic_fetch.py",
            "tools/update_semantic_lock.py",
            "semantic-toolchain.lock.json",
            "verification/tests/test_semantic_lock.py",
        ])
        self.assertFalse(result["should_build"])

    def test_producer_recipe_path_starts_producer(self) -> None:
        result = module.classify(["tools/semantic_sdk.py"])
        self.assertTrue(result["should_build"])

    def test_semantic_dependency_change_starts_producer(self) -> None:
        before = self._lock()
        after = copy.deepcopy(before)
        after["dependencies"]["protobuf"]["source_sha256"] = "changed"
        result = module.classify(["deps.lock.json"], lock_before=before, lock_after=after)
        self.assertTrue(result["should_build"])
        self.assertEqual(result["changed_lock_keys"], ["protobuf"])

    def test_unrelated_dependency_change_does_not_start_producer(self) -> None:
        before = self._lock()
        after = copy.deepcopy(before)
        after["dependencies"]["skia"]["commit"] = "changed"
        result = module.classify(["deps.lock.json"], lock_before=before, lock_after=after)
        self.assertFalse(result["should_build"])
        self.assertEqual(result["changed_lock_keys"], ["skia"])

    def test_malformed_or_unavailable_lock_diff_is_conservative(self) -> None:
        result = module.classify(["deps.lock.json"])
        self.assertTrue(result["should_build"])
        result = module.classify(
            ["deps.lock.json"], lock_before={"dependencies": []}, lock_after={"dependencies": {}}
        )
        self.assertTrue(result["should_build"])

    def test_workflow_does_not_list_consumer_only_inputs(self) -> None:
        workflow = (ROOT / ".github/workflows/semantic-toolchain-producer.yml").read_text(
            encoding="utf-8"
        )
        for consumer_path in (
            "tools/semantic_fetch.py",
            "tools/update_semantic_lock.py",
            "semantic-toolchain.lock.json",
            "verification/tests/test_semantic_lock.py",
        ):
            self.assertNotIn(f'      - "{consumer_path}"', workflow)


if __name__ == "__main__":
    unittest.main()
