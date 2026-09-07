"""Target runtimes contain relocatable static consumers, never host protoc."""
import json
from pathlib import Path
import tempfile
import unittest

from tools.sdk.model import SdkError
from tools.semantic.contract import load_profile, RUNTIME_KEYS, make_runtime_identity
from tools.semantic.package_runtime import package_runtime, install_runtime, verify_runtime_archive
from tools.semantic.tests.test_contract_v2 import fixture_toolchain

ROOT = Path(__file__).resolve().parents[3]


def runtime_fixture(root: Path, windows=False):
    files = {"include/google/protobuf/message.h": b"#pragma once\n",
             "lib/cmake/protobuf/protobuf-config.cmake": b"# config\n",
             "lib/cmake/absl/abslConfig.cmake": b"# config\n",
             "lib/cmake/utf8_range/utf8_range-config.cmake": b"# config\n",
             "lib/libprotobuf.lib" if windows else "lib/libprotobuf.a": b"!<arch>\n",
             "licenses/Protobuf.txt": b"fixture license\n",
             "licenses/Abseil.txt": b"fixture license\n",
             "licenses/utf8_range.txt": b"fixture license\n"}
    for name, data in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return root


class RuntimePackageTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.lock = json.loads((ROOT / "deps.lock.json").read_text())
        self.profile = load_profile()

    def test_all_runtime_keys_package_and_relocate_without_host_tool(self):
        for key in RUNTIME_KEYS:
            with self.subTest(key=key):
                root = runtime_fixture(self.base / key, key.startswith("windows"))
                record = package_runtime(self.lock, self.profile, key, root, fixture_toolchain(), self.base / "a" / key)
                other = package_runtime(self.lock, self.profile, key, root, fixture_toolchain(), self.base / "b" / key)
                self.assertEqual(record, other)
                self.assertEqual(record["runtimeId"], make_runtime_identity(self.lock, self.profile, key, fixture_toolchain())[1])
                archive = self.base / "a" / key / record["asset"]
                installed = self.base / "relocated" / key
                manifest = install_runtime(archive, installed, record)
                self.assertEqual(manifest["format"], "axiom-semantic-runtime-v2")
                self.assertFalse((installed / "bin/protoc").exists())
                self.assertFalse((installed / "bin/protoc.exe").exists())
                verify_runtime_archive(archive, record)

    def test_host_executable_is_rejected_rather_than_hidden(self):
        for executable in ("protoc", "protoc.exe"):
            root = runtime_fixture(self.base / executable)
            (root / "bin").mkdir()
            (root / "bin" / executable).write_bytes(b"wrong-host-tool")
            with self.assertRaises(SdkError):
                package_runtime(self.lock, self.profile, "linux-x86_64", root, fixture_toolchain(), self.base / "out")

    def test_missing_package_config_library_or_license_fails(self):
        for number, missing in enumerate(("lib/cmake/absl/abslConfig.cmake", "lib/libprotobuf.a", "licenses/Protobuf.txt")):
            root = runtime_fixture(self.base / str(number))
            (root / missing).unlink()
            with self.subTest(missing=missing), self.assertRaises(SdkError):
                package_runtime(self.lock, self.profile, "linux-x86_64", root, fixture_toolchain(), self.base / "out")

    def test_cmake_cannot_reference_producer_install_root(self):
        root = runtime_fixture(self.base / "install")
        (root / "lib/cmake/protobuf/protobuf-config.cmake").write_text(f'set(path "{root.as_posix()}/lib")\n')
        with self.assertRaisesRegex(SdkError, "relocat"):
            package_runtime(self.lock, self.profile, "linux-x86_64", root, fixture_toolchain(), self.base / "out")

    def test_pkgconfig_and_build_junk_are_not_the_cmake_sdk_contract(self):
        root = runtime_fixture(self.base / "install")
        (root / "lib/pkgconfig").mkdir()
        (root / "lib/pkgconfig/protobuf.pc").write_text("prefix=/producer/path\n")
        (root / "install_manifest.txt").write_text("/producer/path\n")
        record = package_runtime(self.lock, self.profile, "linux-x86_64", root, fixture_toolchain(), self.base / "out")
        installed = self.base / "installed"
        install_runtime(self.base / "out" / record["asset"], installed, record)
        self.assertFalse((installed / "lib/pkgconfig").exists())
        self.assertFalse((installed / "install_manifest.txt").exists())


if __name__ == "__main__":
    unittest.main()
