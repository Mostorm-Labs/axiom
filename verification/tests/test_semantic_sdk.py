import importlib.util
import json
from pathlib import Path
import tempfile
import stat
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("semantic_sdk", ROOT / "tools/semantic_sdk.py")
assert SPEC and SPEC.loader
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class SemanticSdkTest(unittest.TestCase):
    def _lock(self):
        return json.loads((ROOT / "deps.lock.json").read_text(encoding="utf-8"))

    def _toolchain(self):
        return {
            "runner": "ubuntu-24.04",
            "arch": "x86_64",
            "compiler": "gcc-hosted",
            "cmake": "hosted",
            "ninja": "hosted",
            "cxxStandard": 20,
        }

    def test_identity_excludes_install_path_and_is_canonical(self):
        identity_a, sdk_a = module.make_identity(self._lock(), self._toolchain())
        identity_b, sdk_b = module.make_identity(self._lock(), {**self._toolchain(), "installPath": "/tmp/other"})
        self.assertEqual(identity_a, identity_b)
        self.assertEqual(sdk_a, sdk_b)
        self.assertRegex(sdk_a, r"^[0-9a-f]{64}$")

    def test_archive_is_deterministic_and_manifest_is_verified(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "toolchain"
            (root / "bin").mkdir(parents=True)
            (root / "lib").mkdir()
            (root / "include").mkdir()
            (root / "bin" / "protoc").write_bytes(b"protoc")
            (root / "bin" / "protoc").chmod(0o755)
            (root / "lib" / "libprotobuf.a").write_bytes(b"protobuf")
            (root / "lib" / "cmake" / "protobuf").mkdir(parents=True)
            (root / "lib" / "cmake" / "absl").mkdir(parents=True)
            (root / "lib" / "cmake" / "utf8_range").mkdir(parents=True)
            (root / "lib" / "cmake" / "protobuf" / "protobuf-config.cmake").write_text("# protobuf\n")
            (root / "lib" / "cmake" / "absl" / "abslConfig.cmake").write_text("# absl\n")
            (root / "lib" / "cmake" / "utf8_range" / "utf8_range-config.cmake").write_text("# utf8\n")
            lock = self._lock()
            (root / ".canvas-semantic-toolchain.json").write_text(json.dumps({
                "format": "canvas-semantic-toolchain-v1",
                "protobuf_version": lock["dependencies"]["protobuf"]["version"],
                "protobuf_edition": lock["dependencies"]["protobuf"]["edition"],
                "protobuf_source_sha256": lock["dependencies"]["protobuf"]["source_sha256"],
                "protobuf_asset_key": "linux-x86_64",
                "protobuf_asset_sha256": lock["dependencies"]["protobuf"]["protoc_assets"]["linux-x86_64"]["sha256"],
                "abseil_version": lock["dependencies"]["abseil"]["version"],
                "abseil_source_sha256": lock["dependencies"]["abseil"]["source_sha256"],
            }) + "\n")
            (root / "include" / "semantic.h").write_text("#pragma once\n", encoding="utf-8")
            archive_a = Path(directory) / "a.zip"
            archive_b = Path(directory) / "b.zip"
            identity, sdk_id = module.make_identity(self._lock(), self._toolchain())
            manifest = module.make_manifest(root, identity, sdk_id)
            module.create_archive(root, manifest, archive_a)
            module.create_archive(root, manifest, archive_b)
            self.assertEqual(archive_a.read_bytes(), archive_b.read_bytes())
            destination = Path(directory) / "installed"
            verified = module.verify_archive(archive_a, destination, self._lock(), sdk_id)
            self.assertEqual(verified["sdkId"], sdk_id)
            self.assertEqual((destination / "bin" / "protoc").read_bytes(), b"protoc")
            self.assertTrue(stat.S_IMODE((destination / "bin" / "protoc").stat().st_mode) & stat.S_IXUSR)

    def test_archive_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "bad.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape", b"bad")
            with self.assertRaises(module.SdkError):
                module.verify_archive(archive, Path(directory) / "installed", self._lock(), "0" * 64)

    def test_archive_rejects_sdk_identity_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "toolchain"
            (root / "bin").mkdir(parents=True)
            (root / "bin" / "protoc").write_bytes(b"protoc")
            identity, sdk_id = module.make_identity(self._lock(), self._toolchain())
            archive = Path(directory) / "toolchain.zip"
            module.create_archive(root, module.make_manifest(root, identity, sdk_id), archive)
            with self.assertRaises(module.SdkError):
                module.verify_archive(archive, Path(directory) / "installed", self._lock(), "f" * 64)

    def test_archive_rejects_missing_consumer_toolchain_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "toolchain"
            (root / "bin").mkdir(parents=True)
            (root / "bin" / "protoc").write_bytes(b"protoc")
            identity, sdk_id = module.make_identity(self._lock(), self._toolchain())
            archive = Path(directory) / "toolchain.zip"
            module.create_archive(root, module.make_manifest(root, identity, sdk_id), archive)
            with self.assertRaisesRegex(module.SdkError, "consumer contract"):
                module.verify_archive(archive, Path(directory) / "installed", self._lock(), sdk_id)

    def test_archive_rejects_non_executable_protoc(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "toolchain"
            (root / "bin").mkdir(parents=True)
            (root / "lib" / "cmake" / "protobuf").mkdir(parents=True)
            (root / "lib" / "cmake" / "absl").mkdir(parents=True)
            (root / "lib" / "cmake" / "utf8_range").mkdir(parents=True)
            (root / "bin" / "protoc").write_bytes(b"protoc")
            (root / "lib" / "libprotobuf.a").write_bytes(b"protobuf")
            (root / "lib" / "cmake" / "protobuf" / "protobuf-config.cmake").write_text("# protobuf\n")
            (root / "lib" / "cmake" / "absl" / "abslConfig.cmake").write_text("# absl\n")
            (root / "lib" / "cmake" / "utf8_range" / "utf8_range-config.cmake").write_text("# utf8\n")
            lock = self._lock()
            (root / ".canvas-semantic-toolchain.json").write_text(json.dumps({
                "format": "canvas-semantic-toolchain-v1",
                "protobuf_version": lock["dependencies"]["protobuf"]["version"],
                "protobuf_edition": lock["dependencies"]["protobuf"]["edition"],
                "protobuf_source_sha256": lock["dependencies"]["protobuf"]["source_sha256"],
                "protobuf_asset_key": "linux-x86_64",
                "protobuf_asset_sha256": lock["dependencies"]["protobuf"]["protoc_assets"]["linux-x86_64"]["sha256"],
                "abseil_version": lock["dependencies"]["abseil"]["version"],
                "abseil_source_sha256": lock["dependencies"]["abseil"]["source_sha256"],
            }) + "\n")
            identity, sdk_id = module.make_identity(lock, self._toolchain())
            archive = Path(directory) / "toolchain.zip"
            module.create_archive(root, module.make_manifest(root, identity, sdk_id), archive)
            with self.assertRaisesRegex(module.SdkError, "executable"):
                module.verify_archive(archive, Path(directory) / "installed", lock, sdk_id)


if __name__ == "__main__":
    unittest.main()
