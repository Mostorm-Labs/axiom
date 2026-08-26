import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "generate_g1_02_evidence",
    ROOT / "verification/tools/generate_g1_02_evidence.py",
)
assert SPEC and SPEC.loader
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class G102EvidenceTest(unittest.TestCase):
    def test_status_remains_blocked_without_hosted_and_authority_corpus(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            result = generator.generate(
                root=ROOT,
                source_commit="a" * 40,
                output_root=output,
                hosted_url=None,
                runtime_status="PASS",
                generated_code_sha256="b" * 64,
            )
        self.assertEqual(result["status"], "BLOCKED")
        self.assertIn("hosted", " ".join(result["blockingReasons"]))
        self.assertIn("BG/BGX", " ".join(result["blockingReasons"]))

    def test_evidence_is_commit_bound_and_hashes_descriptor(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            result = generator.generate(
                root=ROOT,
                source_commit="c" * 40,
                output_root=output,
                hosted_url="https://github.com/example/actions/runs/1",
                runtime_status="PASS",
                generated_code_sha256="d" * 64,
            )
            self.assertEqual(result["sourceCommit"], "c" * 40)
            self.assertEqual(result["descriptorSha256"], "ca3d93f126d32f22d4972e8c019f16e7e6b41068a69db9cd09f9dbf2d5547239")
            self.assertEqual(result["generatedCodeSha256"], "d" * 64)
            self.assertEqual(result["authorityBaseline"], "25332232f41b5973ca7057e3c84b0038573982b5")
            self.assertEqual(result["corpus"]["caseCount"], 18)
            self.assertRegex(result["corpus"]["fixtureManifestSha256"], r"^[0-9a-f]{64}$")
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["sourceCommit"], "c" * 40)
        self.assertTrue(manifest["files"])


if __name__ == "__main__":
    unittest.main()
