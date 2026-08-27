"""GT-G1-02R commit-bound evidence status tests."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "verification/tools/generate_g1_02r_evidence.py"
DEFECT_IDS = ("RT-D01", "RT-D02", "RT-D03", "RT-D04", "RT-D05", "RT-D06", "ST-D01", "ST-D02", "ST-D03", "ST-D04", "ST-D05")


def evidence_module():
    spec = importlib.util.spec_from_file_location("g1_02r_evidence", TOOL)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def write_required_inputs(output: Path) -> None:
    (output / "descriptor-refreeze-diff.json").write_text(json.dumps({
        "beforeDescriptorSha256": "a" * 64,
        "afterDescriptorSha256": "b" * 64,
        "unmappedChanges": [],
        "changes": [{"defectIds": [defect]} for defect in DEFECT_IDS],
    }) + "\n", encoding="utf-8")
    for name in ("bg-differential.json", "g1-02r-differential.json"):
        (output / name).write_text(json.dumps({"status": "PASS", "firstDivergence": None}) + "\n", encoding="utf-8")
    (output / "local-verification.json").write_text(json.dumps({
        "descriptorReproducibility": "PASS",
        "generatedCodeReproducibility": "PASS",
        "semanticCTest": "PASS",
        "g1_01r": "PASS",
        "g1_03": "PASS",
        "publicBoundary": "PASS",
        "fixtureDoubleBuild": "PASS",
    }) + "\n", encoding="utf-8")


class G102REvidenceTest(unittest.TestCase):
    def test_hosted_g1_lane_runs_old_bg_and_new_leaf_refreeze_oracles(self) -> None:
        workflow = (ROOT / ".github/workflows/g1-semantic-codec.yml").read_text(encoding="utf-8")
        for required in (
            "compile_g1_02r_leaf_golden.py",
            "run_g1_02r_semantic_differential.py",
            "descriptor_refreeze_diff.py",
            "generate_g1_02r_evidence.py",
            "test_g1_02r_evidence",
            "fetch-depth: 0",
            "CANVAS_SEMANTIC_GOLDEN_PROBE=out/g1-hosted/runtime/semantic/tools/canvas_semantic_golden_probe",
        ):
            self.assertIn(required, workflow)

    def test_evidence_stays_blocked_without_exact_source_hosted_validation(self) -> None:
        module = evidence_module()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_required_inputs(output)
            result = module.generate(ROOT, "c" * 40, output, hosted=None, generated_code_sha256="d" * 64)
        self.assertEqual("BLOCKED", result["status"])
        self.assertIn("hosted", " ".join(result["blockingReasons"]))

    def test_complete_evidence_is_commit_bound_and_preserves_historical_g1_02(self) -> None:
        module = evidence_module()
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_required_inputs(output)
            result = module.generate(
                ROOT,
                "c" * 40,
                output,
                hosted={"url": "https://example.test/actions/runs/2", "headSha": "c" * 40, "status": "PASS"},
                generated_code_sha256="d" * 64,
            )
            module.write(output, result)
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual("PASS", result["status"])
        self.assertEqual("GT-G1-02R", result["taskId"])
        self.assertEqual("GT-G1-02", result["reconcilesTask"])
        self.assertTrue(result["historicalG1_02"]["preserved"])
        self.assertTrue(result["oldBgCorpusPreserved"])
        self.assertEqual([], result["descriptor"]["unmappedChanges"])
        self.assertEqual(set(DEFECT_IDS), set(result["defectClosure"]))
        self.assertEqual("c" * 40, manifest["sourceCommit"])
        self.assertTrue(manifest["files"])


if __name__ == "__main__":
    unittest.main()
