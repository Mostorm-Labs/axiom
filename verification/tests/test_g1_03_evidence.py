import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_g1_03_evidence", ROOT / "verification/tools/generate_g1_03_evidence.py"
)
assert SPEC and SPEC.loader
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class G103EvidenceTest(unittest.TestCase):
    def test_evidence_records_reentry_scope_and_private_rebuildable_index(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            result = generator.generate(
                ROOT,
                "a" * 40,
                output,
                "https://github.com/example/actions/runs/3",
                "https://github.com/example/actions/runs/4",
            )

            self.assertEqual(result["taskId"], "GT-G1-03")
            self.assertEqual(result["status"], "PASS")
            self.assertEqual(result["authorityBaseline"], "06b60dffad8fdbbd6254f40ca4c65147881d445c")
            self.assertEqual(result["reentryPromptRevision"], "v2")
            self.assertFalse(result["architectureChanged"])
            self.assertEqual(result["authorityPublicationGap"], "DEFERRED_GOVERNANCE_DEBT")
            self.assertEqual(result["typedObjectRecordMaterialization"], "PASS")
            self.assertEqual(result["propertyValueRepresentation"], "TYPED_CLOSED_UNION")
            self.assertEqual(result["objectContentRepresentation"], "TYPED_NINE_WAY_UNION")
            self.assertEqual(result["eraseMaskRepresentation"], "TYPED_GEOMETRY_UNION")
            self.assertEqual(result["regression"]["semanticCTest"]["testCount"], 33)
            self.assertEqual(result["objectIndex"]["authorityRole"], "PRIVATE_REBUILDABLE_ACCELERATION")
            self.assertEqual(result["objectIndex"]["families"], ["parent_children"])
            self.assertEqual(result["ordering"]["allObjects"], "OBJECT_ID_BYTE_ORDER_IMPLEMENTATION_CONVENTION")
            self.assertEqual(result["ordering"]["equalOrderKeyTieBreak"], "OBJECT_ID_IMPLEMENTATION_ONLY")
            self.assertEqual(result["differential"]["firstDivergence"], None)
            self.assertEqual(result["ciBoundary"]["rule"], "CI_TRIGGER_IS_NOT_GATE_AUTHORITY")
            self.assertFalse(result["ciBoundary"]["poc03RequiredForGate"])
            self.assertEqual(result["ciBoundary"]["ciBoundaryContract"], "PASS")
            self.assertFalse(result["gtG104Authorized"])

    def test_evidence_requires_hosted_validation_and_hashes_written_files(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            blocked = generator.generate(ROOT, "b" * 40, output, None, None)
            self.assertEqual(blocked["status"], "BLOCKED")
            self.assertIn("hosted", " ".join(blocked["blockingReasons"]))

            generator.write(output, generator.generate(
                ROOT,
                "c" * 40,
                output,
                "https://example.test/run",
                "https://example.test/boundary",
            ))
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["taskId"], "GT-G1-03")
            self.assertEqual(manifest["sourceCommit"], "c" * 40)
            for file_entry in manifest["files"]:
                payload = (output / file_entry["path"]).read_bytes()
                self.assertEqual(file_entry["sha256"], hashlib.sha256(payload).hexdigest())


if __name__ == "__main__":
    unittest.main()
