"""Complete sets, verified bytes and immutable identities are release authority."""
import copy
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.sdk.archive import file_sha256
from tools.sdk.model import SdkError
from tools.semantic.aggregate import aggregate, check_identity_collisions, verify_release_directory
from tools.semantic.contract import read_json, validate_index
from tools.semantic.tests.release_fixtures import release_fixture


class AggregateTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.assets, self.records = release_fixture(self.base)
        self.output = self.base / "release"

    def test_complete_set_is_deterministic_and_has_fourteen_publishable_assets(self):
        release = aggregate(self.assets, self.output)
        index = validate_index(read_json(self.output / "semantic-sdk-index.json"))
        self.assertEqual(len(index["hostTools"]), 3)
        self.assertEqual(len(index["runtimes"]), 9)
        expected = {r["asset"] for r in self.records.values()} | {"semantic-sdk-index.json", "SHA256SUMS"}
        self.assertEqual(set(release["assets"]), expected)
        self.assertEqual({p.name for p in self.output.iterdir()}, expected | {"release.json"})
        self.assertEqual(release["tag"], "semantic-sdk-v2-" + index["releaseSetId"][:16])
        self.assertEqual(release["indexSha256"], file_sha256(self.output / "semantic-sdk-index.json"))
        sums = dict(line.split("  ", 1)[::-1] for line in (self.output / "SHA256SUMS").read_text().splitlines())
        self.assertEqual(set(sums), expected - {"SHA256SUMS"})
        for name, digest in sums.items():
            self.assertEqual(file_sha256(self.output / name), digest)
        self.assertEqual(verify_release_directory(self.output), release)
        other = self.base / "second"
        self.assertEqual(aggregate(self.assets, other), release)
        for name in expected | {"release.json"}:
            self.assertEqual((self.output / name).read_bytes(), (other / name).read_bytes())

    def test_missing_or_duplicate_key_fails_before_output(self):
        key = "linux-x86_64"
        source = self.assets / "hostTools" / key
        duplicate = self.assets / "duplicate"
        shutil.copytree(source, duplicate)
        with self.assertRaisesRegex(SdkError, "duplicate"):
            aggregate(self.assets, self.output)
        self.assertFalse(self.output.exists())
        shutil.rmtree(duplicate)
        shutil.rmtree(source)
        with self.assertRaises(SdkError):
            aggregate(self.assets, self.output)
        self.assertFalse(self.output.exists())

    def test_corrupt_archive_fails_before_output(self):
        record = self.records["runtimes", "web-wasm32"]
        (self.assets / "runtimes/web-wasm32" / record["asset"]).write_bytes(b"corrupt")
        with self.assertRaises(SdkError):
            aggregate(self.assets, self.output)
        self.assertFalse(self.output.exists())

    def test_same_id_with_different_bytes_requires_contract_increment(self):
        for kind, field in (("hostTools", "hostToolId"), ("runtimes", "runtimeId")):
            prior = {"hostTools": {}, "runtimes": {}}
            candidate = copy.deepcopy(prior)
            prior[kind]["sample"] = {field: "a" * 64, "sha256": "1" * 64}
            candidate[kind]["sample"] = {field: "a" * 64, "sha256": "2" * 64}
            with self.subTest(kind=kind), self.assertRaisesRegex(SdkError, "identity.collision.*contract"):
                check_identity_collisions(prior, candidate)
            candidate[kind]["sample"]["sha256"] = "1" * 64
            check_identity_collisions(prior, candidate)

    def test_existing_output_is_not_silently_overwritten(self):
        aggregate(self.assets, self.output)
        path = self.output / "SHA256SUMS"
        path.write_bytes(b"existing bad data")
        with self.assertRaises(SdkError):
            aggregate(self.assets, self.output)
        self.assertEqual(path.read_bytes(), b"existing bad data")


if __name__ == "__main__":
    unittest.main()
