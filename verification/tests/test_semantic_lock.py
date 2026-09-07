import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/update_semantic_lock.py"
FETCH = ROOT / "tools/semantic_fetch.py"
SDK_ID = "a" * 64
SHA256 = "b" * 64
ASSET = f"semantic-toolchain-linux-x86_64-{SDK_ID}.zip"


class SemanticLockTest(unittest.TestCase):
    def test_update_lock_writes_release_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "semantic-toolchain.lock.json"
            subprocess.run([
                sys.executable, str(SCRIPT), "--tag", "semantic-toolchain-poc01-aaaaaaaaaaaaaaaa",
                "--asset", ASSET, "--sdk-id", SDK_ID, "--sha256", SHA256,
                "--output", str(output),
            ], check=True)
            lock = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(lock["format"], "axiom-semantic-toolchain-lock-v1")
            self.assertEqual(lock["target"], "linux-x86_64")
            self.assertEqual(lock["sha256"], SHA256)

    def test_update_lock_rejects_asset_not_bound_to_sdk(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([
                sys.executable, str(SCRIPT), "--tag", "semantic-toolchain-poc01-aaaaaaaaaaaaaaaa",
                "--asset", "wrong.zip", "--sdk-id", SDK_ID, "--sha256", SHA256,
                "--output", str(Path(directory) / "lock.json"),
            ], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("asset name", result.stderr + result.stdout)

    def test_fetch_rejects_malformed_lock_before_download(self):
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / "semantic-toolchain.lock.json"
            lock.write_text(json.dumps({
                "format": "axiom-semantic-toolchain-lock-v1",
                "repository": "Mostorm-Labs/axiom",
                "releaseTag": "test",
                "target": "linux-x86_64",
                "asset": "wrong.zip",
                "sdkId": SDK_ID,
                "sha256": SHA256,
            }), encoding="utf-8")
            result = subprocess.run([
                sys.executable, str(FETCH), "--lock", str(lock),
                "--destination", str(Path(directory) / "protobuf"),
            ], capture_output=True, text=True, env={**os.environ, "CANVAS_SEMANTIC_SDK_BASE_URL": "http://127.0.0.1:9"})
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("invalid asset or digest", result.stderr + result.stdout)


class SemanticV2LockTest(unittest.TestCase):
    def setUp(self):
        from tools.sdk.archive import canonical_bytes
        from tools.semantic.contract import load_profile, read_json
        from tools.semantic.tests.test_contract_v2 import fixture_index
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.index = fixture_index(read_json(ROOT / "deps.lock.json"), load_profile())
        self.path = self.base / "index.json"
        self.path.write_bytes(canonical_bytes(self.index) + b"\n")
        self.tag = "semantic-sdk-v2-" + self.index["releaseSetId"][:16]

    def test_v2_generator_pins_exact_index_bytes_and_does_not_touch_v1(self):
        import hashlib
        from tools.semantic.contract import validate_v2_lock
        old = (ROOT / "semantic-toolchain.lock.json").read_bytes()
        output = self.base / "v2.lock.json"
        result = subprocess.run([sys.executable, str(SCRIPT), "--index", str(self.path),
                                 "--tag", self.tag, "--output", str(output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        value = validate_v2_lock(json.loads(output.read_bytes()))
        self.assertEqual(value["indexSha256"], hashlib.sha256(self.path.read_bytes()).hexdigest())
        self.assertEqual(value["releaseSetId"], self.index["releaseSetId"])
        self.assertEqual((ROOT / "semantic-toolchain.lock.json").read_bytes(), old)

    def test_invalid_index_tag_or_mixed_v1_arguments_preserve_existing_output(self):
        output = self.base / "lock.json"
        output.write_bytes(b"preserve")
        commands = [("--index", str(self.path), "--tag", "wrong"),
                    ("--index", str(self.path), "--tag", self.tag, "--asset", ASSET)]
        for options in commands:
            result = subprocess.run([sys.executable, str(SCRIPT), *options, "--output", str(output)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(output.read_bytes(), b"preserve")
        self.path.write_bytes(b"{}")
        result = subprocess.run([sys.executable, str(SCRIPT), "--index", str(self.path),
                                 "--tag", self.tag, "--output", str(output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
