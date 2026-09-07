"""Semantic v2 authority is independent of Store and unrelated SDK families."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import SdkError
from tools.semantic.contract import (
    HOST_KEYS, RUNTIME_KEYS, load_profile, read_json, validate_profile,
    make_host_tool_identity, make_runtime_identity, make_release_set_identity,
    dependency_authority, validate_index, validate_v2_lock,
)

ROOT = Path(__file__).resolve().parents[3]


def fixture_toolchain():
    return {"compiler": "gcc", "compilerVersion": "fixture-1",
            "compilerTarget": "x86_64-linux-gnu", "cmakeVersion": "3.30.5",
            "ninjaVersion": "1.12.1", "cxxRuntime": "libstdc++"}


def fixture_index(lock, profile):
    hosts, runtimes = {}, {}
    for key in HOST_KEYS:
        identity, identifier = make_host_tool_identity(lock, profile, key)
        hosts[key] = {"hostToolId": identifier, "identity": identity,
                      "asset": f"axiom-semantic-protoc-{key}-{identifier}.zip",
                      "sha256": hashlib.sha256((key + "host").encode()).hexdigest(),
                      "manifestSha256": "b" * 64, "size": 100}
    for key in RUNTIME_KEYS:
        identity, identifier = make_runtime_identity(lock, profile, key, fixture_toolchain())
        runtimes[key] = {"runtimeId": identifier, "identity": identity,
                         "asset": f"axiom-semantic-runtime-{key}-{identifier}.zip",
                         "sha256": hashlib.sha256((key + "runtime").encode()).hexdigest(),
                         "manifestSha256": "c" * 64, "size": 100}
    dependencies = dependency_authority(lock)
    _, identifier = make_release_set_identity(dependencies, hosts, runtimes)
    return {"format": "axiom-semantic-sdk-index-v2", "releaseSetId": identifier,
            "dependencies": dependencies, "hostTools": hosts, "runtimes": runtimes}


class SemanticContractTest(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads((ROOT / "deps.lock.json").read_text(encoding="utf-8"))
        self.profile = load_profile()

    def test_exact_supported_matrix(self):
        self.assertEqual(set(self.profile["hostTools"]), {
            "linux-x86_64", "windows-x64", "macos-universal"})
        self.assertEqual(set(self.profile["runtimes"]), {
            "linux-x86_64", "windows-x64-msvc-static", "macos-arm64", "macos-x64",
            "ios-arm64", "ios-simulator-arm64", "android-arm64-v8a", "android-x86_64", "web-wasm32"})

    def test_windows_host_asset_is_pinned_without_changing_version(self):
        protobuf = self.lock["dependencies"]["protobuf"]
        self.assertEqual(protobuf["version"], "36.0")
        self.assertEqual(protobuf["protoc_assets"]["windows-x64"]["sha256"],
                         "510fb2369a4720adb457783768c5e1481a0e1137d9e7694478dfe9b8fe445dc1")

    def test_windows_protoc_addition_does_not_trigger_skia_producer(self):
        from tools.skia.classify_r1_changes import classify_lock_documents
        before = copy.deepcopy(self.lock)
        before["dependencies"]["protobuf"]["protoc_assets"].pop("windows-x64")
        decision = classify_lock_documents(before, self.lock)
        self.assertEqual(decision["mode"], "none")
        self.assertEqual(decision["targets"], [])

    def test_windows_runtime_has_explicit_crt_and_iterator_contract(self):
        abi = self.profile["runtimes"]["windows-x64-msvc-static"]["abi"]
        self.assertEqual(abi["crt"], "static-release")
        self.assertEqual(abi["iteratorDebugLevel"], 0)
        self.assertEqual(abi["cxxStandard"], 20)
        self.assertEqual(abi["linkage"], "static")

    def test_identity_hashes_canonical_payload_without_self_reference(self):
        identity, identifier = make_host_tool_identity(self.lock, self.profile, "linux-x86_64")
        self.assertNotIn("hostToolId", identity)
        self.assertEqual(identifier, hashlib.sha256(canonical_bytes(identity)).hexdigest())
        identity, identifier = make_runtime_identity(self.lock, self.profile, "linux-x86_64", fixture_toolchain())
        self.assertNotIn("runtimeId", identity)
        self.assertEqual(identifier, hashlib.sha256(canonical_bytes(identity)).hexdigest())

    def test_unrelated_sdk_node_and_store_metadata_never_change_ids(self):
        other = copy.deepcopy(self.lock)
        other["dependencies"]["node"]["version"] = "999.0"
        other["dependencies"]["future_pdf"] = {"version": "1.0"}
        other["storePath"] = "/unrelated"
        other["mirror"] = "https://irrelevant.example.test"
        for key in HOST_KEYS:
            self.assertEqual(make_host_tool_identity(self.lock, self.profile, key),
                             make_host_tool_identity(other, self.profile, key))
        for key in RUNTIME_KEYS:
            self.assertEqual(make_runtime_identity(self.lock, self.profile, key, fixture_toolchain()),
                             make_runtime_identity(other, self.profile, key, fixture_toolchain()))

    def test_protobuf_version_changes_all_identities(self):
        other = copy.deepcopy(self.lock)
        other["dependencies"]["protobuf"]["version"] = "37.0"
        for key in HOST_KEYS:
            self.assertNotEqual(make_host_tool_identity(self.lock, self.profile, key)[1],
                                make_host_tool_identity(other, self.profile, key)[1])
        for key in RUNTIME_KEYS:
            self.assertNotEqual(make_runtime_identity(self.lock, self.profile, key, fixture_toolchain())[1],
                                make_runtime_identity(other, self.profile, key, fixture_toolchain())[1])

    def test_protobuf_source_only_does_not_reidentify_existing_host_binary(self):
        other = copy.deepcopy(self.lock)
        other["dependencies"]["protobuf"]["source_sha256"] = "1" * 64
        for key in HOST_KEYS:
            self.assertEqual(make_host_tool_identity(self.lock, self.profile, key),
                             make_host_tool_identity(other, self.profile, key))
        for key in RUNTIME_KEYS:
            self.assertNotEqual(make_runtime_identity(self.lock, self.profile, key, fixture_toolchain())[1],
                                make_runtime_identity(other, self.profile, key, fixture_toolchain())[1])

    def test_one_upstream_host_digest_affects_only_that_host(self):
        other = copy.deepcopy(self.lock)
        other["dependencies"]["protobuf"]["protoc_assets"]["windows-x64"]["sha256"] = "1" * 64
        for key in HOST_KEYS:
            before = make_host_tool_identity(self.lock, self.profile, key)[1]
            after = make_host_tool_identity(other, self.profile, key)[1]
            self.assertEqual(before == after, key != "windows-x64")
        for key in RUNTIME_KEYS:
            self.assertEqual(make_runtime_identity(self.lock, self.profile, key, fixture_toolchain()),
                             make_runtime_identity(other, self.profile, key, fixture_toolchain()))

    def test_abseil_changes_runtimes_not_hosts(self):
        other = copy.deepcopy(self.lock)
        other["dependencies"]["abseil"]["source_sha256"] = "1" * 64
        for key in HOST_KEYS:
            self.assertEqual(make_host_tool_identity(self.lock, self.profile, key),
                             make_host_tool_identity(other, self.profile, key))
        for key in RUNTIME_KEYS:
            self.assertNotEqual(make_runtime_identity(self.lock, self.profile, key, fixture_toolchain())[1],
                                make_runtime_identity(other, self.profile, key, fixture_toolchain())[1])

    def test_platform_profile_change_is_local(self):
        other = copy.deepcopy(self.profile)
        other["runtimes"]["android-x86_64"]["toolchain"]["apiLevel"] = 27
        for key in RUNTIME_KEYS:
            before = make_runtime_identity(self.lock, self.profile, key, fixture_toolchain())[1]
            after = make_runtime_identity(self.lock, other, key, fixture_toolchain())[1]
            self.assertEqual(before == after, key != "android-x86_64")

    def test_toolchain_paths_and_timestamps_are_not_identity(self):
        other = {**fixture_toolchain(), "installPath": "C:/different", "buildPath": "/other",
                 "timestamp": "tomorrow", "sourcePath": "/source", "sysrootPath": "/sdk"}
        self.assertEqual(make_runtime_identity(self.lock, self.profile, "linux-x86_64", fixture_toolchain()),
                         make_runtime_identity(self.lock, self.profile, "linux-x86_64", other))
        other["compilerVersion"] = "fixture-2"
        self.assertNotEqual(make_runtime_identity(self.lock, self.profile, "linux-x86_64", fixture_toolchain())[1],
                            make_runtime_identity(self.lock, self.profile, "linux-x86_64", other)[1])

    def test_missing_toolchain_and_unsupported_target_fail(self):
        with self.assertRaises(SdkError):
            make_runtime_identity(self.lock, self.profile, "linux-x86_64", {})
        with self.assertRaises(SdkError):
            make_host_tool_identity(self.lock, self.profile, "windows-arm64")

    def test_contract_versions_are_independent(self):
        other = copy.deepcopy(self.profile)
        other["hostContractVersion"] += 1
        self.assertNotEqual(make_host_tool_identity(self.lock, self.profile, "linux-x86_64")[1],
                            make_host_tool_identity(self.lock, other, "linux-x86_64")[1])
        self.assertEqual(make_runtime_identity(self.lock, self.profile, "linux-x86_64", fixture_toolchain()),
                         make_runtime_identity(self.lock, other, "linux-x86_64", fixture_toolchain()))

    def test_release_identity_does_not_hash_itself(self):
        index = fixture_index(self.lock, self.profile)
        payload, identifier = make_release_set_identity(index["dependencies"], index["hostTools"], index["runtimes"])
        self.assertNotIn("releaseSetId", payload)
        self.assertEqual(index["releaseSetId"], hashlib.sha256(canonical_bytes(payload)).hexdigest())
        self.assertEqual(validate_index(index), index)
        index["hostTools"]["windows-x64"]["sha256"] = "1" * 64
        self.assertNotEqual(make_release_set_identity(index["dependencies"], index["hostTools"], index["runtimes"])[1], identifier)
        with self.assertRaises(SdkError):
            validate_index(index)

    def test_index_rejects_missing_extra_and_renamed_cells(self):
        base = fixture_index(self.lock, self.profile)
        for mutation in ("missing", "extra", "renamed"):
            index = copy.deepcopy(base)
            if mutation == "missing":
                del index["runtimes"]["web-wasm32"]
            elif mutation == "extra":
                index["runtimes"]["future"] = index["runtimes"]["web-wasm32"]
            else:
                index["hostTools"]["linux-x86_64"]["asset"] = "other.zip"
            with self.subTest(mutation=mutation), self.assertRaises(SdkError):
                validate_index(index)

    def test_index_rejects_identity_and_dependency_drift(self):
        for field in ("identity", "dependencies", "manifestSha256", "size"):
            index = fixture_index(self.lock, self.profile)
            if field == "identity":
                index["runtimes"]["web-wasm32"]["identity"]["key"] = "linux-x86_64"
            elif field == "dependencies":
                index["dependencies"]["protobuf"]["version"] = "999"
            elif field == "manifestSha256":
                index["hostTools"]["linux-x86_64"][field] = "bad"
            else:
                index["runtimes"]["web-wasm32"][field] = -1
            with self.subTest(field=field), self.assertRaises(SdkError):
                validate_index(index)

    def test_self_consistent_index_still_rejects_wrong_platform_contract(self):
        index = fixture_index(self.lock, self.profile)
        record = index["runtimes"]["linux-x86_64"]
        record["identity"]["target"]["platform"] = "windows"
        record["runtimeId"] = hashlib.sha256(canonical_bytes(record["identity"])).hexdigest()
        record["asset"] = f"axiom-semantic-runtime-linux-x86_64-{record['runtimeId']}.zip"
        index["releaseSetId"] = make_release_set_identity(index["dependencies"], index["hostTools"], index["runtimes"])[1]
        with self.assertRaises(SdkError):
            validate_index(index)

    def test_profile_rejects_missing_matrix_and_unknown_fields(self):
        profile = copy.deepcopy(self.profile)
        del profile["hostTools"]["linux-x86_64"]
        with self.assertRaises(SdkError):
            validate_profile(profile)
        profile = copy.deepcopy(self.profile)
        profile["unexpected"] = True
        with self.assertRaises(SdkError):
            validate_profile(profile)

    def test_json_parser_rejects_duplicate_keys_and_nonfinite_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            for text in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}'):
                path.write_text(text, encoding="utf-8")
                with self.subTest(text=text), self.assertRaises(SdkError):
                    read_json(path)

    def test_lock_pins_derived_tag_and_exact_index_bytes(self):
        index = fixture_index(self.lock, self.profile)
        identifier = index["releaseSetId"]
        lock = {"format": "axiom-semantic-sdk-lock-v2", "repository": "Mostorm-Labs/axiom",
                "releaseTag": f"semantic-sdk-v2-{identifier[:16]}", "releaseSetId": identifier,
                "indexAsset": "semantic-sdk-index.json", "indexSha256": "1" * 64}
        self.assertEqual(validate_v2_lock(lock), lock)
        for field, value in (("releaseTag", "latest"), ("indexSha256", "BAD"),
                             ("indexAsset", "../index.json"), ("format", "v1")):
            with self.subTest(field=field), self.assertRaises(SdkError):
                validate_v2_lock({**lock, field: value})


if __name__ == "__main__":
    unittest.main()
