import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "run_g1_semantic_differential",
    ROOT / "verification/tools/run_g1_semantic_differential.py",
)
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class SemanticDifferentialTest(unittest.TestCase):
    def test_missing_authority_binary_corpus_is_blocked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "verification/corpus/semantic/v1"
            (corpus / "wire/bg").mkdir(parents=True)
            (corpus / "wire/bgx").mkdir(parents=True)
            (corpus / "corpus.json").write_text(json.dumps({
                "format": "axiom-semantic-v1-corpus-manifest",
                "version": 1,
                "status": "candidate_pending_authority",
                "wireRoots": [
                    "verification/corpus/semantic/v1/wire/bg",
                    "verification/corpus/semantic/v1/wire/bgx",
                ],
            }) + "\n", encoding="utf-8")
            result = module.run_differential(root)
        self.assertEqual(result["status"], "BLOCKED_AUTHORITY")
        self.assertIn("BG/BGX", " ".join(result["blockingReasons"]))
        self.assertEqual(result["binaryCorpus"]["fileCount"], 0)

    def test_binary_inventory_is_sorted_and_hashed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "verification/corpus/semantic/v1"
            (corpus / "wire/bg").mkdir(parents=True)
            (corpus / "wire/bgx").mkdir(parents=True)
            (corpus / "wire/bg/z.bin").write_bytes(b"z")
            (corpus / "wire/bg/a.bin").write_bytes(b"a")
            (corpus / "wire/bgx/x.bin").write_bytes(b"x")
            (corpus / "corpus.json").write_text(json.dumps({
                "format": "axiom-semantic-v1-corpus-manifest",
                "version": 1,
                "status": "promoted",
                "differentialOracle": "pending_authority",
                "wireRoots": [
                    "verification/corpus/semantic/v1/wire/bg",
                    "verification/corpus/semantic/v1/wire/bgx",
                ],
            }) + "\n", encoding="utf-8")
            result = module.run_differential(root)
        self.assertEqual(result["status"], "BLOCKED_AUTHORITY")
        self.assertIn("oracle", " ".join(result["blockingReasons"]))
        paths = [item["path"] for item in result["binaryCorpus"]["files"]]
        self.assertEqual(paths, sorted(paths))
        self.assertEqual(result["binaryCorpus"]["fileCount"], 3)
        self.assertEqual(len(result["binaryCorpus"]["inventorySha256"]), 64)

    def test_promoted_manifest_without_the_complete_authority_case_set_is_not_a_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "verification/corpus/semantic/v1"
            (corpus / "wire/bg/BG-001-id128").mkdir(parents=True)
            (corpus / "wire/bg/BG-001-id128/case.json").write_text("{}\n", encoding="utf-8")
            (corpus / "corpus.json").write_text(json.dumps({
                "format": "axiom-semantic-v1-corpus-manifest",
                "version": 1,
                "status": "promoted",
                "differentialOracle": "authority_promoted",
            }) + "\n", encoding="utf-8")
            result = module.run_differential(root)
        self.assertNotEqual(result["status"], "PASS")
        self.assertIn("18", " ".join(result["blockingReasons"]))


if __name__ == "__main__":
    unittest.main()
