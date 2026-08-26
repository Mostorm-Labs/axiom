import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("validate_semantic_contract", ROOT / "verification/tools/validate_semantic_contract.py")
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class SemanticContractTest(unittest.TestCase):
    def test_descriptor_hash_and_seed_are_locked(self):
        self.assertEqual(module.main.__name__, "main")
        seed = json.loads((ROOT / "verification/corpus/semantic/v1/suites/seed-v0.1.json").read_text())
        self.assertEqual(len(seed["cases"]), 60)
        self.assertEqual(seed["cases"][0]["stable_id"], "g1-seed-v0.1-000")
        self.assertEqual(seed["cases"][-1]["stable_id"], "g1-seed-v0.1-059")

    def test_binary_corpus_is_explicitly_pending_authority(self):
        corpus = json.loads((ROOT / "verification/corpus/semantic/v1/corpus.json").read_text())
        self.assertEqual(corpus["status"], "candidate_pending_authority")
        self.assertEqual(corpus["differentialOracle"], "pending_authority")


if __name__ == "__main__":
    unittest.main()
