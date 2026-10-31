"""Reject misleading prerequisite and performance evidence before G5 admission."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("g5_preflight", ROOT / "verification/tools/g5_preflight.py")
g5 = importlib.util.module_from_spec(SPEC)


class G5PreflightTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not Path(SPEC.origin).exists():
            raise AssertionError("G5 prerequisite and source-bound measurement tool is missing")
        SPEC.loader.exec_module(g5)

    def test_readiness_and_dev_acceptance_do_not_satisfy_original_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, data in [("G4.7", {"status": "READY_FOR_CONTROL_REVIEW"}),
                               ("FC-05-DEV", {"verdict": "PASS", "scope": "DEV"})]:
                file = root / g5.RECORD_PATHS[name]
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text(json.dumps(data))
            report = g5.audit(root)
            self.assertEqual(report["g5_admission"], "BLOCKED_UNVERIFIED_PREREQUISITES")
            self.assertFalse(report["official_gate_pass_claimed"])
            obligations = {r["id"]: r for r in report["required_acceptance_bindings"]}
            self.assertEqual(obligations["G4.7"]["status"], "RECORDED_PENDING_OR_BLOCKED")
            self.assertEqual(obligations["FC-05-independent-DEV"]["status"], "INDEPENDENT_ACCEPTANCE_NOT_BOUND")

    def test_missing_input_is_reported_and_not_inferred_from_code(self):
        with tempfile.TemporaryDirectory() as directory:
            report = g5.audit(Path(directory))
        self.assertEqual(report["g5_admission"], "BLOCKED_UNVERIFIED_PREREQUISITES")
        self.assertFalse(report["official_gate_pass_claimed"])
        self.assertTrue(report["missing_reference_paths"])

    def test_source_identity_includes_untracked_files_and_detects_removal(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            (root / "runtime").mkdir()
            file = root / "runtime/new.cpp"
            file.write_text("first")
            before = g5.source_identity(root)
            file.write_text("second")
            self.assertNotEqual(before["tree_sha256"], g5.source_identity(root)["tree_sha256"])
            file.unlink()
            self.assertNotEqual(before["tree_sha256"], g5.source_identity(root)["tree_sha256"])

    def test_percentiles_keep_tail_and_unknown_is_not_zero(self):
        result = g5.summarize([1, 2, 3, 4, 100])
        self.assertEqual(result["p95"], 100)
        self.assertEqual(result["count"], 5)
        self.assertIsNone(g5.summarize([])["p95"])
        for samples in [[float("nan")], [-1], [True]]:
            with self.assertRaises(ValueError):
                g5.summarize(samples)

    def test_changed_source_or_missing_artifact_binding_invalidates_measurement(self):
        same = {"tree_sha256": "a" * 64}
        changed = {"tree_sha256": "b" * 64}
        valid = {"classification": "MEASURED", "artifact_binding": "same-build",
                 "checks": {"commit_observed": True}}
        self.assertEqual(g5.measurement_status(same, same, valid), "MEASURED")
        self.assertEqual(g5.measurement_status(same, changed, valid), "SOURCE_CHANGED")
        self.assertEqual(g5.measurement_status(same, same, {**valid, "artifact_binding": None}),
                         "BLOCKED_EVIDENCE")
        self.assertEqual(g5.measurement_status(same, same, {**valid, "checks": {"commit_observed": False}}),
                         "BLOCKED_EVIDENCE")

    def test_component_measurement_rejects_dropped_samples_and_geometry_mismatch(self):
        good = {"samples": 64, "confirmed_samples": 64, "seal_digest": "1234",
                "replay_digest": "1234", "append_ms": [1] * 64, "seal_ms": 2,
                "batch_size": 1}
        self.assertTrue(g5.valid_component_run(good))
        self.assertFalse(g5.valid_component_run({**good, "confirmed_samples": 63}))
        self.assertFalse(g5.valid_component_run({**good, "replay_digest": "ffff"}))
        self.assertFalse(g5.valid_component_run({**good, "append_ms": [1] * 63}))
        self.assertFalse(g5.valid_component_run({**good, "seal_ms": None}))

    def test_repeated_run_preserves_previous_evidence_without_reusing_stale_files(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "browser.json").write_text('{"old": true}')
            (out / "result.json").write_text('{"classification": "MEASURED"}')
            (out / "build").mkdir()
            (out / "build/cache").write_text("keep cached build")
            archive = g5.prepare_output(out)
            self.assertFalse((out / "browser.json").exists())
            self.assertFalse((out / "result.json").exists())
            self.assertEqual(json.loads((archive / "browser.json").read_text()), {"old": True})
            self.assertTrue((out / "build/cache").exists())

    def test_skia_identity_must_match_locked_target_before_build(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "manifest.json").write_text(json.dumps({"sdk_id": "a" * 64}))
            lock = {"targets": {"web": {"variants": {"release": {"sdk_id": "b" * 64}}}}}
            with self.assertRaisesRegex(ValueError, "Skia SDK identity"):
                g5.skia_identity(root, lock, "web")
            lock["targets"]["web"]["variants"]["release"]["sdk_id"] = "a" * 64
            value = g5.skia_identity(root, lock, "web")
            self.assertEqual(value["sdk_id"], "a" * 64)
            self.assertEqual(value["target"], "web")


if __name__ == "__main__":
    unittest.main()
