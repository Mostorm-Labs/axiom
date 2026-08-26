import json
import tempfile
import unittest
from pathlib import Path
import importlib.util


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_g1_01_evidence", ROOT / "verification/tools/generate_g1_01_evidence.py"
)
assert SPEC and SPEC.loader
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class G101EvidenceTest(unittest.TestCase):
    def test_registry_is_derived_from_explicit_authority_lists(self):
        evidence = generator.generate(ROOT, "a" * 40, "2026-08-26T00:00:00Z")
        registry = evidence["promotedSemanticRegistry"]
        self.assertEqual(len(registry["objectKinds"]), 9)
        self.assertEqual(len(registry["operations"]), 15)
        self.assertEqual(registry["operations"][0]["symbol"], "InsertObjects")
        self.assertEqual(registry["operations"][-1]["symbol"], "SetConnectorContent")

    def test_reconciliation_contains_boundaries_and_no_private_locator(self):
        evidence = generator.generate(ROOT, "b" * 40, "2026-08-26T00:00:00Z")
        self.assertFalse(any(source["privateLocatorIncluded"] for source in evidence["sources"]))
        self.assertEqual(evidence["boundaries"]["pageCollectionOwner"], "upper Product Shell")
        self.assertIn("Page ObjectKind", evidence["boundaries"]["forbidden"])
        self.assertIn("Frame", evidence["boundaries"]["framePdf"])
        self.assertIn("PDF", evidence["boundaries"]["framePdf"])

    def test_output_manifest_hashes_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            evidence = generator.generate(ROOT, "c" * 40, "2026-08-26T00:00:00Z")
            (output / "authority-reconciliation.json").write_text(
                json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
            )
            self.assertEqual(json.loads((output / "authority-reconciliation.json").read_text())["sourceCommit"], "c" * 40)


if __name__ == "__main__":
    unittest.main()
