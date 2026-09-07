"""Semantic v2 identity/schema authority, without transport or build execution.

Historical consumers validate pinned identity payloads, never today's recipe.
A producer contract version must advance whenever its bytes/contract change.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import ReleaseIndexRef, SdkError, validate_digest

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PROFILE = Path(__file__).with_name("profile-v2.json")
HOST_KEYS = ("linux-x86_64", "windows-x64", "macos-universal")
RUNTIME_KEYS = ("linux-x86_64", "windows-x64-msvc-static", "macos-arm64", "macos-x64",
                "ios-arm64", "ios-simulator-arm64", "android-arm64-v8a", "android-x86_64", "web-wasm32")
PROFILE_FORMAT = "axiom-semantic-sdk-profile-v2"
INDEX_FORMAT = "axiom-semantic-sdk-index-v2"
LOCK_FORMAT = "axiom-semantic-sdk-lock-v2"
_TOOLCHAIN_REQUIRED = {"compiler", "compilerVersion", "cmakeVersion", "ninjaVersion", "cxxRuntime"}
_TOOLCHAIN_FIELDS = _TOOLCHAIN_REQUIRED | {
    "compilerTarget", "llvmVersion", "ndkVersion", "apiLevel", "emscriptenVersion",
    "sdkName", "sdkVersion", "xcodeVersion", "deploymentTarget", "glibcVersion",
    "msvcVersion", "windowsSdkVersion", "compilerFlags", "linkerVersion",
}
_TOOLCHAIN_METADATA = {"installPath", "buildPath", "sourcePath", "sysrootPath", "timestamp"}
_TARGET_LAYOUT = {
    "linux-x86_64": ("linux", "x86_64", "libstdc++"),
    "windows-x64-msvc-static": ("windows", "x64", "msvc"),
    "macos-arm64": ("macos", "arm64", "libc++"),
    "macos-x64": ("macos", "x86_64", "libc++"),
    "ios-arm64": ("ios", "arm64", "libc++"),
    "ios-simulator-arm64": ("ios-simulator", "arm64", "libc++"),
    "android-arm64-v8a": ("android", "arm64-v8a", "libc++"),
    "android-x86_64": ("android", "x86_64", "libc++"),
    "web-wasm32": ("web", "wasm32", "libc++"),
}


def _fail(message: str) -> None:
    raise SdkError("semantic: " + message)


def _object(value: Any, required: set[str], label: str, optional: frozenset[str] = frozenset()) -> dict:
    if not isinstance(value, dict) or not required <= value.keys() or value.keys() - required - optional:
        _fail(f"invalid {label} fields")
    return value


def _string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        _fail(f"invalid {label}")
    return value


def _positive_int(value: Any, label: str) -> int:
    if type(value) is not int or value <= 0:
        _fail(f"invalid {label}")
    return value


def _pairs(pairs: list[tuple[str, Any]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            _fail(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs,
                           parse_constant=lambda token: _fail(f"nonfinite JSON value: {token}"))
    except (OSError, ValueError) as error:
        raise SdkError(f"semantic: cannot read JSON {path}: {error}") from error
    if not isinstance(value, dict):
        _fail("JSON root must be an object")
    return value


def _identity(payload: dict) -> tuple[dict, str]:
    return payload, hashlib.sha256(canonical_bytes(payload)).hexdigest()


def validate_profile(value: Any) -> dict:
    profile = _object(value, {"format", "hostContractVersion", "runtimeContractVersion", "hostTools", "runtimes"}, "profile")
    if profile["format"] != PROFILE_FORMAT:
        _fail("unsupported profile format")
    _positive_int(profile["hostContractVersion"], "hostContractVersion")
    _positive_int(profile["runtimeContractVersion"], "runtimeContractVersion")
    _object(profile["hostTools"], set(HOST_KEYS), "host matrix")
    _object(profile["runtimes"], set(RUNTIME_KEYS), "runtime matrix")
    for key, host in profile["hostTools"].items():
        _object(host, {"upstreamKey", "runner", "executable"}, f"host {key}")
        for field in host:
            _string(host[field], field)
        expected = "bin/protoc.exe" if key == "windows-x64" else "bin/protoc"
        if host["executable"] != expected:
            _fail(f"incorrect host executable: {key}")
    for key, target in profile["runtimes"].items():
        _object(target, {"platform", "arch", "runner", "abi", "toolchain"}, f"runtime {key}")
        for field in ("platform", "arch", "runner"):
            _string(target[field], field)
        if not isinstance(target["toolchain"], dict) or not target["toolchain"]:
            _fail(f"missing toolchain contract: {key}")
        _validate_abi(target["abi"], key)
        _validate_target_layout(target, key)
    return profile


def _validate_target_layout(target: dict, key: str) -> None:
    actual = (target["platform"], target["arch"], target["abi"]["cxxRuntime"])
    if actual != _TARGET_LAYOUT[key]:
        _fail(f"runtime key does not match its platform/architecture/C++ ABI: {key}")


def _validate_abi(abi: Any, key: str) -> None:
    fields = {"cxxStandard", "linkage", "pic", "buildType", "sanitizer", "exceptions", "rtti", "cxxRuntime"}
    extra = {"crt", "iteratorDebugLevel"} if key == "windows-x64-msvc-static" else {"pthread"} if key == "web-wasm32" else set()
    _object(abi, fields | extra, "runtime ABI")
    if abi["cxxStandard"] != 20 or abi["linkage"] != "static" or abi["buildType"] != "Release" or abi["sanitizer"] != "none":
        _fail("unsupported runtime ABI")
    for flag in ("pic", "exceptions", "rtti"):
        if abi[flag] is not True:
            _fail(f"runtime ABI requires {flag}")
    _string(abi["cxxRuntime"], "C++ runtime")
    if key == "windows-x64-msvc-static" and (abi["crt"] != "static-release" or abi["iteratorDebugLevel"] != 0):
        _fail("Windows runtime requires release static CRT and iterator ABI 0")
    if key == "web-wasm32" and abi["pthread"] is not False:
        _fail("Web runtime must not require pthreads")


def load_profile(path: Path = DEFAULT_PROFILE) -> dict:
    return validate_profile(read_json(path))


def dependency_authority(lock: dict) -> dict:
    try:
        protobuf, abseil = lock["dependencies"]["protobuf"], lock["dependencies"]["abseil"]
        result = {
            "protobuf": {"version": protobuf["version"], "edition": protobuf["edition"], "sourceSha256": protobuf["source_sha256"]},
            "abseil": {"version": abseil["version"], "sourceSha256": abseil["source_sha256"]},
        }
    except (KeyError, TypeError) as error:
        raise SdkError("semantic: missing producer dependencies") from error
    _validate_dependencies(result)
    return result


def _validate_dependencies(dependencies: dict) -> None:
    _object(dependencies, {"protobuf", "abseil"}, "dependency authority")
    for key, fields in (("protobuf", {"version", "edition", "sourceSha256"}),
                        ("abseil", {"version", "sourceSha256"})):
        record = _object(dependencies[key], fields, key)
        for field, item in record.items():
            _string(item, f"{key}.{field}")
        validate_digest(record["sourceSha256"])


def make_host_tool_identity(lock: dict, profile: dict, key: str) -> tuple[dict, str]:
    validate_profile(profile)
    if key not in HOST_KEYS:
        _fail(f"unsupported host key: {key}")
    try:
        protobuf = lock["dependencies"]["protobuf"]
        entry = profile["hostTools"][key]
        upstream = protobuf["protoc_assets"][entry["upstreamKey"]]
        digest = validate_digest(upstream["sha256"])
    except (KeyError, TypeError) as error:
        raise SdkError(f"semantic: missing locked upstream host asset: {key}") from error
    return _identity({"format": "axiom-semantic-host-identity-v2", "key": key,
                      "protobufVersion": _string(protobuf["version"], "protobuf version"),
                      "upstreamSha256": digest, "executable": entry["executable"],
                      "contractVersion": profile["hostContractVersion"]})


def make_runtime_identity(lock: dict, profile: dict, key: str, toolchain: dict) -> tuple[dict, str]:
    validate_profile(profile)
    if key not in RUNTIME_KEYS:
        _fail(f"unsupported runtime key: {key}")
    _object(toolchain, _TOOLCHAIN_REQUIRED, "toolchain", frozenset((_TOOLCHAIN_FIELDS - _TOOLCHAIN_REQUIRED) | _TOOLCHAIN_METADATA))
    for field in _TOOLCHAIN_REQUIRED:
        _string(toolchain[field], field)
    target = {field: item for field, item in profile["runtimes"][key].items() if field != "runner"}
    dependencies = dependency_authority(lock)
    return _identity({"format": "axiom-semantic-runtime-identity-v2", "key": key,
                      "protobuf": dependencies["protobuf"], "abseil": dependencies["abseil"],
                      "target": target, "toolchain": {k: v for k, v in toolchain.items() if k in _TOOLCHAIN_FIELDS},
                      "contractVersion": profile["runtimeContractVersion"]})


def make_release_set_identity(dependencies: dict, hosts: dict, runtimes: dict) -> tuple[dict, str]:
    _validate_dependencies(dependencies)
    def records(values: dict, id_field: str) -> list[dict]:
        return [{"key": key, "identity": record[id_field], "sha256": record["sha256"],
                 "manifestSha256": record["manifestSha256"], "size": record["size"]}
                for key, record in sorted(values.items())]
    return _identity({"format": "axiom-semantic-release-identity-v2", "dependencies": dependencies,
                      "hostTools": records(hosts, "hostToolId"), "runtimes": records(runtimes, "runtimeId")})


def _validate_record(record: Any, key: str, kind: str, dependencies: dict) -> None:
    is_host = kind == "hostTools"
    id_field = "hostToolId" if is_host else "runtimeId"
    _object(record, {id_field, "identity", "asset", "sha256", "manifestSha256", "size"}, f"{key} record")
    for field in (id_field, "sha256", "manifestSha256"):
        validate_digest(record[field])
    _positive_int(record["size"], "asset size")
    prefix = "protoc" if is_host else "runtime"
    if record["asset"] != f"axiom-semantic-{prefix}-{key}-{record[id_field]}.zip":
        _fail(f"incorrect identity-bound asset name: {key}")
    identity = record["identity"]
    fields = ({"format", "key", "protobufVersion", "upstreamSha256", "executable", "contractVersion"}
              if is_host else {"format", "key", "protobuf", "abseil", "target", "toolchain", "contractVersion"})
    _object(identity, fields, f"{key} identity")
    expected_format = "axiom-semantic-host-identity-v2" if is_host else "axiom-semantic-runtime-identity-v2"
    if identity["format"] != expected_format or identity["key"] != key or _identity(identity)[1] != record[id_field]:
        _fail(f"invalid canonical identity: {key}")
    _positive_int(identity["contractVersion"], "package contract version")
    if is_host:
        validate_digest(identity["upstreamSha256"])
        if identity["protobufVersion"] != dependencies["protobuf"]["version"]:
            _fail(f"host dependency drift: {key}")
        if identity["executable"] != ("bin/protoc.exe" if key == "windows-x64" else "bin/protoc"):
            _fail(f"incorrect host executable: {key}")
    else:
        if identity["protobuf"] != dependencies["protobuf"] or identity["abseil"] != dependencies["abseil"]:
            _fail(f"runtime dependency drift: {key}")
        target = _object(identity["target"], {"platform", "arch", "abi", "toolchain"}, "runtime target")
        _validate_abi(target["abi"], key)
        _validate_target_layout(target, key)
        _object(identity["toolchain"], _TOOLCHAIN_REQUIRED, "runtime toolchain", frozenset(_TOOLCHAIN_FIELDS - _TOOLCHAIN_REQUIRED))


def validate_index(value: Any) -> dict:
    index = _object(value, {"format", "releaseSetId", "dependencies", "hostTools", "runtimes"}, "index", frozenset({"provenance"}))
    if index["format"] != INDEX_FORMAT:
        _fail("unsupported index format")
    validate_digest(index["releaseSetId"])
    _validate_dependencies(index["dependencies"])
    _object(index["hostTools"], set(HOST_KEYS), "index host matrix")
    _object(index["runtimes"], set(RUNTIME_KEYS), "index runtime matrix")
    for kind in ("hostTools", "runtimes"):
        for key, record in index[kind].items():
            _validate_record(record, key, kind, index["dependencies"])
    _, expected = make_release_set_identity(index["dependencies"], index["hostTools"], index["runtimes"])
    if index["releaseSetId"] != expected:
        _fail("releaseSetId mismatch")
    return index


def validate_v2_lock(value: Any) -> dict:
    lock = _object(value, {"format", "repository", "releaseTag", "releaseSetId", "indexAsset", "indexSha256"}, "v2 lock")
    if lock["format"] != LOCK_FORMAT:
        _fail("unsupported lock format")
    validate_digest(lock["releaseSetId"])
    if lock["releaseTag"] != f"semantic-sdk-v2-{lock['releaseSetId'][:16]}" or lock["indexAsset"] != "semantic-sdk-index.json":
        _fail("lock tag or index asset does not match release identity")
    ReleaseIndexRef("semantic", lock["releaseSetId"], lock["repository"], lock["releaseTag"], lock["indexAsset"], lock["indexSha256"])
    return lock
