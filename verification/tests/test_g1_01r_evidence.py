import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_g1_01r_evidence", ROOT / "verification/tools/generate_g1_01r_evidence.py"
)
assert SPEC and SPEC.loader
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class G101REvidenceTest(unittest.TestCase):
    def test_reconciliation_evidence_records_current_authority_and_stop_boundary(self):
        evidence = generator.generate(ROOT, "a" * 40, "b" * 40)
        self.assertEqual(evidence["taskId"], "GT-G1-01R")
        self.assertFalse(evidence["architectureChanged"])
        self.assertEqual(evidence["gtG102"]["status"], "PASS")
        self.assertFalse(evidence["gtG103Authorized"])
        self.assertIn("SemanticRevision", evidence["oldBoundaryRemoved"])
        self.assertFalse(any(item["privateLocatorIncluded"] for item in evidence["authority"]))

    def test_reconciliation_evidence_records_red_green_regression_and_boundary_provenance(self):
        evidence = generator.generate(ROOT, "a" * 40, "b" * 40)

        self.assertEqual(evidence["redEvidence"]["result"], "EXPECTED_FAIL")
        self.assertEqual(evidence["greenEvidence"]["result"], "PASS")
        self.assertEqual(evidence["regression"]["result"], "PASS")
        self.assertEqual(evidence["publicDependencyCheck"]["result"], "PASS")
        self.assertIn("filesChanged", evidence)

    def test_manifest_hashes_the_commit_bound_evidence_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            generator.write(output, generator.generate(ROOT, "c" * 40, "d" * 40))
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["taskId"], "GT-G1-01R")
            self.assertEqual(manifest["sourceCommit"], "c" * 40)
            self.assertTrue(manifest["files"])


if __name__ == "__main__":
    unittest.main()
