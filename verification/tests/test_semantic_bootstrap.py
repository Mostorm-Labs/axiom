import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("bootstrap_deps", ROOT / "tools/bootstrap_deps.py")
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class SemanticBootstrapTest(unittest.TestCase):
    def test_platform_asset_key_is_explicit(self):
        self.assertEqual(module.semantic_platform_asset_key("Linux", "x86_64"), "linux-x86_64")
        self.assertEqual(module.semantic_platform_asset_key("Darwin", "arm64"), "darwin-universal")
        with self.assertRaises(RuntimeError):
            module.semantic_platform_asset_key("Windows", "AMD64")

    def test_lock_contains_runtime_and_codegen_identity(self):
        lock = json.loads((ROOT / "deps.lock.json").read_text(encoding="utf-8"))
        plan = module.semantic_dependency_plan(lock, "Linux", "x86_64")
        self.assertEqual(plan["protobuf_version"], "36.0")
        self.assertEqual(plan["protobuf_edition"], "2024")
        self.assertEqual(plan["abseil_version"], "20250512.1")
        self.assertRegex(plan["protobuf_source_sha256"], r"^[0-9a-f]{64}$")
        self.assertRegex(plan["abseil_source_sha256"], r"^[0-9a-f]{64}$")

    def test_zip_extraction_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "bad.zip"
            destination = Path(directory) / "out"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape.txt", "no")
            with self.assertRaises(RuntimeError):
                module.safe_extract_zip(archive, destination)


if __name__ == "__main__":
    unittest.main()
