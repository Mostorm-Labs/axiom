"""Trusted prior locks allow byte reuse, never authority inferred from a cache."""
import copy
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import SdkError
from tools.semantic.aggregate import aggregate
from tools.semantic.contract import read_json
from tools.semantic.reuse_release import load_accepted_index, reuse_asset
from tools.semantic.tests.release_fixtures import release_fixture
from tools.update_semantic_lock import make_v2_lock


class ReuseReleaseTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        assets, self.records = release_fixture(self.base)
        release = aggregate(assets, self.base / "release")
        self.mirror = self.base / "mirror"
        self.remote = self.mirror / release["tag"]
        shutil.copytree(self.base / "release", self.remote)
        self.lock = self.base / "semantic-sdk.lock.json"
        value = make_v2_lock(self.remote / "semantic-sdk-index.json", release["tag"])
        self.lock.write_bytes(canonical_bytes(value))
        self.store = self.base / "store"
        self.options = {"store_root": self.store, "mirror": str(self.mirror)}

    def test_no_v2_lock_is_clean_miss_without_network_or_output(self):
        with mock.patch("tools.sdk.transport._github_download", side_effect=AssertionError("network")):
            self.assertIsNone(load_accepted_index(self.base / "missing", offline=True))
            self.assertIsNone(reuse_asset(self.base / "missing", "hostTools", "linux-x86_64", "a" * 64,
                                          self.base / "out", offline=True))
        self.assertFalse((self.base / "out").exists())

    def test_same_identity_reuses_exact_archive_and_second_call_is_offline(self):
        for kind, key in (("hostTools", "windows-x64"), ("runtimes", "macos-arm64")):
            with self.subTest(kind=kind):
                record = self.records[kind, key]
                identifier = record["hostToolId" if kind == "hostTools" else "runtimeId"]
                output = self.base / kind
                with mock.patch("tools.sdk.transport._github_download", side_effect=AssertionError("network")):
                    result = reuse_asset(self.lock, kind, key, identifier, output, **self.options)
                    again = reuse_asset(self.lock, kind, key, identifier, self.base / (kind + "-offline"),
                                        store_root=self.store, offline=True)
                self.assertEqual(result, record)
                self.assertEqual(again, record)
                self.assertEqual(read_json(output / "record.json"), record)
                self.assertEqual((output / record["asset"]).read_bytes(), (self.remote / record["asset"]).read_bytes())

    def test_changed_identity_is_clean_miss_without_package_transfer(self):
        prior = load_accepted_index(self.lock, **self.options)
        self.assertIsNotNone(prior)
        with mock.patch("tools.sdk.transport._github_download", side_effect=AssertionError("network")):
            self.assertIsNone(reuse_asset(self.lock, "runtimes", "linux-x86_64", "e" * 64,
                                          self.base / "out", store_root=self.store, offline=True))
        self.assertFalse((self.base / "out").exists())

    def test_corrupt_pinned_index_is_not_a_rebuild_miss(self):
        (self.remote / "semantic-sdk-index.json").write_bytes(b"{}")
        with self.assertRaises(SdkError):
            load_accepted_index(self.lock, **self.options)

    def test_index_release_identity_must_match_lock_even_with_matching_hash(self):
        value = read_json(self.lock)
        value["releaseSetId"] = "e" * 64
        value["releaseTag"] = "semantic-sdk-v2-" + "e" * 16
        shutil.copytree(self.remote, self.mirror / value["releaseTag"])
        self.lock.write_bytes(canonical_bytes(value))
        with self.assertRaisesRegex(SdkError, "releaseSetId"):
            load_accepted_index(self.lock, **self.options)

    def test_corrupt_archive_is_not_a_rebuild_miss_or_partial_output(self):
        record = self.records["runtimes", "linux-x86_64"]
        (self.remote / record["asset"]).write_bytes(b"bad")
        with self.assertRaises(SdkError):
            reuse_asset(self.lock, "runtimes", "linux-x86_64", record["runtimeId"], self.base / "out", **self.options)
        self.assertFalse((self.base / "out").exists())

    def test_existing_output_is_preserved_on_conflict(self):
        record = self.records["hostTools", "linux-x86_64"]
        output = self.base / "out"
        output.mkdir()
        (output / "record.json").write_bytes(b"old")
        with self.assertRaises(SdkError):
            reuse_asset(self.lock, "hostTools", "linux-x86_64", record["hostToolId"], output, **self.options)
        self.assertEqual((output / "record.json").read_bytes(), b"old")


if __name__ == "__main__":
    unittest.main()
