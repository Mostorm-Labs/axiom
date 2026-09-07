"""Tiny valid packages for release authority tests; never actual SDK binaries."""
from pathlib import Path

from tools.sdk.archive import canonical_bytes
from tools.semantic.contract import ROOT, HOST_KEYS, RUNTIME_KEYS, load_profile, read_json, make_host_tool_identity, make_runtime_identity
from tools.semantic.package_common import create_package
from tools.semantic.tests.test_contract_v2 import fixture_toolchain
from tools.semantic.tests.test_package_runtime_v2 import runtime_fixture


def release_fixture(base: Path):
    lock, profile = read_json(ROOT / "deps.lock.json"), load_profile()
    assets = base / "assets"
    records = {}
    for kind, keys in (("hostTools", HOST_KEYS), ("runtimes", RUNTIME_KEYS)):
        for key in keys:
            payload = base / "payload" / kind / key
            host = kind == "hostTools"
            if host:
                identity, _ = make_host_tool_identity(lock, profile, key)
                for name, data in ((identity["executable"], b"fixture executable"),
                                   ("include/google/protobuf/descriptor.proto", b"syntax = \"proto2\";\n"),
                                   ("licenses/Protobuf.txt", b"fixture license\n")):
                    path = payload / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
            else:
                runtime_fixture(payload, key.startswith("windows"))
                identity, _ = make_runtime_identity(lock, profile, key, fixture_toolchain())
            directory = assets / kind / key
            record = create_package(payload, identity, host=host, output=directory)
            (directory / "record.json").write_bytes(canonical_bytes(record) + b"\n")
            records[kind, key] = record
    return assets, records
