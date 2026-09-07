#!/usr/bin/env python3
"""Producer-cell entry point: build, package twice, relocate, link and probe."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes, file_sha256
from tools.sdk.model import SdkError
from tools.semantic.contract import ROOT, RUNTIME_KEYS, load_profile, read_json
from tools.semantic.package_runtime import package_runtime, verify_runtime_archive
from tools.semantic.smoke_consumer import smoke_consumer


def compare_runtime_packages(first: Path, second: Path) -> dict:
    if first.resolve() == second.resolve():
        raise SdkError("semantic: clean-build comparison requires distinct package directories")
    records = [read_json(path / "record.json") for path in (first, second)]
    manifests = [verify_runtime_archive(path / record["asset"], record)
                 for path, record in zip((first, second), records)]
    if records[0] != records[1]:
        raise SdkError("semantic: clean builds differ in runtime identity or archive bytes")
    a, b = records
    if file_sha256(first / a["asset"]) != file_sha256(second / b["asset"]):
        raise SdkError("semantic: clean build archive bytes differ")
    return {"format": "axiom-semantic-clean-build-comparison-v2", "byteIdentical": True,
            "target": a["identity"]["key"], "runtimeId": a["runtimeId"], "sha256": a["sha256"],
            "manifestSha256": a["manifestSha256"], "size": a["size"],
            "payloadFiles": len(manifests[0]["files"]),
            "staticLibraries": sum(entry["path"].endswith((".lib", ".a"))
                                   for entry in manifests[0]["files"])}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=RUNTIME_KEYS, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "out/semantic-sdk/runtime")
    parser.add_argument("--host-directory", type=Path, default=ROOT / "out/semantic-sdk/host")
    parser.add_argument("--verify-clean-rebuild", action="store_true")
    args = parser.parse_args()
    profile, lock = load_profile(), read_json(ROOT / "deps.lock.json")
    family = profile["runtimes"][args.target]["platform"]
    ndk = Path(os.environ["ANDROID_NDK_ROOT"]) if family == "android" else None
    emscripten = ROOT / ".deps/emsdk/upstream/emscripten" if family == "web" else None
    cc = cxx = str(ROOT / ".deps/llvm/bin/clang-cl.exe") if family == "windows" else None
    extra = []
    for flag, value in (("--ndk", ndk), ("--emscripten", emscripten), ("--cc", cc), ("--cxx", cxx)):
        if value is not None:
            extra += [flag, str(value)]
    first_work = ["--work-root", str(args.output / "work")] if args.verify_clean_rebuild else []
    toolchain_file = args.output / "toolchain.json"
    install_root = args.output / "install"
    subprocess.run([sys.executable, str(ROOT / "tools/semantic/build_runtime.py"), "--target", args.target,
                    "--install-root", str(install_root), "--toolchain-json", str(toolchain_file),
                    "--facts-output", str(args.output / "build.json"), *first_work, *extra], check=True)
    toolchain = read_json(toolchain_file)
    first = args.output / "package-a"
    second = args.output / "package-b"
    a = package_runtime(lock, profile, args.target, install_root, toolchain, first)
    b = package_runtime(lock, profile, args.target, install_root, toolchain, second)
    if a != b or file_sha256(first / a["asset"]) != file_sha256(second / b["asset"]):
        raise SdkError("semantic: runtime packaging is not deterministic")
    for directory in (first, second):
        (directory / "record.json").write_bytes(canonical_bytes(a) + b"\n")
    comparison = None
    if args.verify_clean_rebuild:
        other = args.output / "different depth/clean rebuild"
        other_toolchain = other / "toolchain.json"
        other_install = other / "install"
        subprocess.run([sys.executable, str(ROOT / "tools/semantic/build_runtime.py"),
                        "--target", args.target, "--install-root", str(other_install),
                        "--toolchain-json", str(other_toolchain), "--work-root", str(other / "work"),
                        "--facts-output", str(other / "build.json"), *extra], check=True)
        other_package = other / "package"
        record = package_runtime(lock, profile, args.target, other_install,
                                 read_json(other_toolchain), other_package)
        (other_package / "record.json").write_bytes(canonical_bytes(record) + b"\n")
        comparison = compare_runtime_packages(first, other_package)
        builds = [read_json(path / "build.json") for path in (args.output, other)]
        for field in ("workRoot", "sourceRoot", "buildRoot", "installRoot"):
            if Path(builds[0][field]).resolve() == Path(builds[1][field]).resolve():
                raise SdkError("semantic: clean builds reused a producer directory")
        comparison["builds"] = builds
        comparison["secondSmoke"] = smoke_consumer(
            args.target, other_package, args.host_directory, profile=profile,
            ndk=ndk, emscripten=emscripten, cc=cc, cxx=cxx)
    facts = smoke_consumer(args.target, first, args.host_directory, profile=profile,
                           ndk=ndk, emscripten=emscripten, cc=cc, cxx=cxx)
    (args.output / "smoke.json").write_bytes(canonical_bytes(facts) + b"\n")
    if comparison is not None:
        comparison["firstSmoke"] = facts
        comparison["status"] = "PASS"
        (args.output / "reproducibility.json").write_bytes(canonical_bytes(comparison) + b"\n")
        print(canonical_bytes(comparison).decode())
    print(canonical_bytes(facts).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
