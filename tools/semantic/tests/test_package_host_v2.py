"""Host tools are deterministic packages, not target runtimes or source builds."""
import copy
import json
import os
from pathlib import Path
import tempfile
import unittest
import zipfile

from tools.sdk.archive import file_sha256
from tools.sdk.model import IntegrityError, SdkError
from tools.semantic.contract import load_profile, make_host_tool_identity
from tools.semantic.package_host import package_host, install_host, verify_host_archive

ROOT = Path(__file__).resolve().parents[3]


class HostPackageTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.lock = json.loads((ROOT / "deps.lock.json").read_text())
        self.profile = load_profile()
        self.license = self.base / "LICENSE"
        self.license.write_bytes(b"Protobuf license fixture\n")

    def upstream(self, key="linux-x86_64", *, include_tool=True, unsafe=False):
        archive = self.base / (key + ".zip")
        executable = self.profile["hostTools"][key]["executable"]
        with zipfile.ZipFile(archive, "w") as zipped:
            if include_tool:
                zipped.writestr(executable, b"MZ fake tool" if key == "windows-x64" else b"\x7fELF fake tool")
            zipped.writestr("include/google/protobuf/descriptor.proto", 'syntax = "proto2";\n')
            zipped.writestr("readme.txt", "upstream notice\n")
            if unsafe:
                zipped.writestr("../escape", "bad")
        self.lock["dependencies"]["protobuf"]["protoc_assets"][self.profile["hostTools"][key]["upstreamKey"]]["sha256"] = file_sha256(archive)
        return archive

    def test_all_three_hosts_package_twice_identically_and_install(self):
        for key in self.profile["hostTools"]:
            with self.subTest(key=key):
                upstream = self.upstream(key)
                a = package_host(self.lock, self.profile, key, upstream, self.license, self.base / (key + "-a"))
                b = package_host(self.lock, self.profile, key, upstream, self.license, self.base / (key + "-b"))
                archive_a = self.base / (key + "-a") / a["asset"]
                archive_b = self.base / (key + "-b") / b["asset"]
                self.assertEqual(archive_a.read_bytes(), archive_b.read_bytes())
                self.assertEqual(a, b)
                self.assertEqual(a["hostToolId"], make_host_tool_identity(self.lock, self.profile, key)[1])
                installed = self.base / (key + "-installed")
                install_host(archive_a, installed, a)
                executable = installed / self.profile["hostTools"][key]["executable"]
                self.assertTrue(executable.is_file())
                self.assertTrue((installed / "include/google/protobuf/descriptor.proto").is_file())
                self.assertEqual((installed / "licenses/Protobuf.txt").read_bytes(), self.license.read_bytes())
                self.assertFalse((installed / "package").exists())
                manifest = json.loads((installed / "manifest.json").read_bytes())
                self.assertEqual(manifest["format"], "axiom-semantic-host-tool-v2")
                self.assertEqual(file_sha256(installed / "manifest.json"), a["manifestSha256"])
                self.assertFalse((installed / "lib").exists())
                if os.name != "nt":
                    self.assertTrue(executable.stat().st_mode & 0o111)
                verify_host_archive(archive_a, a)

    def test_upstream_digest_is_required_before_reading_payload(self):
        archive = self.upstream()
        archive.write_bytes(b"corrupted archive")
        with self.assertRaises(IntegrityError):
            package_host(self.lock, self.profile, "linux-x86_64", archive, self.license, self.base / "out")
        self.assertFalse((self.base / "out").exists())

    def test_missing_tool_or_unsafe_archive_is_rejected(self):
        for kwargs in ({"include_tool": False}, {"unsafe": True}):
            with self.subTest(kwargs=kwargs), self.assertRaises(SdkError):
                package_host(self.lock, self.profile, "linux-x86_64", self.upstream(**kwargs), self.license, self.base / "out")

    def test_record_manifest_and_archive_digests_are_independent_checks(self):
        upstream = self.upstream()
        record = package_host(self.lock, self.profile, "linux-x86_64", upstream, self.license, self.base / "out")
        archive = self.base / "out" / record["asset"]
        for field in ("sha256", "manifestSha256", "hostToolId"):
            altered = {**record, field: "f" * 64}
            with self.subTest(field=field), self.assertRaises(SdkError):
                verify_host_archive(archive, altered)

    def test_valid_archive_with_extra_unlisted_payload_is_rejected(self):
        upstream = self.upstream()
        record = package_host(self.lock, self.profile, "linux-x86_64", upstream, self.license, self.base / "out")
        archive = self.base / "out" / record["asset"]
        with zipfile.ZipFile(archive, "a") as zipped:
            zipped.writestr("package/lib/unexpected.a", b"bad")
        changed = {**record, "sha256": file_sha256(archive), "size": archive.stat().st_size}
        with self.assertRaises(SdkError):
            verify_host_archive(archive, changed)

    def test_packaging_does_not_accept_runtime_libraries_from_upstream(self):
        upstream = self.upstream()
        with zipfile.ZipFile(upstream, "a") as zipped:
            zipped.writestr("lib/libprotobuf.a", b"runtime")
        self.lock["dependencies"]["protobuf"]["protoc_assets"]["linux-x86_64"]["sha256"] = file_sha256(upstream)
        record = package_host(self.lock, self.profile, "linux-x86_64", upstream, self.license, self.base / "out")
        archive = self.base / "out" / record["asset"]
        with zipfile.ZipFile(archive) as zipped:
            self.assertFalse(any(name.startswith("package/lib/") for name in zipped.namelist()))


if __name__ == "__main__":
    unittest.main()
