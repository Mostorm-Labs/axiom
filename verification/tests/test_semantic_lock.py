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


if __name__ == "__main__":
    unittest.main()
