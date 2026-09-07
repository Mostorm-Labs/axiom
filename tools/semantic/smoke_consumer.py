#!/usr/bin/env python3
"""Configure, link and where possible execute using only packaged SDK inputs."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import HostPlatform, SdkError, detect_host_platform, validate_component
from tools.semantic.cmake_consumer import consumer_cmake_arguments
from tools.semantic.contract import DEFAULT_PROFILE, ROOT, RUNTIME_KEYS, load_profile, read_json
from tools.semantic.package_host import install_host, probe_host
from tools.semantic.package_runtime import install_runtime
from tools.semantic.toolchain import identify_toolchain


def can_execute_target(host: HostPlatform, target: str) -> bool:
    return (host.os == "linux" and host.arch == "x86_64" and target == "linux-x86_64"
            or host.os == "windows" and host.arch == "x64" and target == "windows-x64-msvc-static"
            or host.os == "macos" and target == f"macos-{host.arch}")


def smoke_consumer(target: str, runtime_directory: Path, host_directory: Path, *, profile: dict,
                   ndk: Path | None = None, emscripten: Path | None = None,
                   cc: str | None = None, cxx: str | None = None) -> dict:
    runtime = read_json(runtime_directory / "record.json")
    host = read_json(host_directory / "record.json")
    if runtime["identity"]["key"] != target:
        raise SdkError("semantic: smoke runtime target mismatch")
    if host["identity"]["protobufVersion"] != runtime["identity"]["protobuf"]["version"]:
        raise SdkError("semantic: smoke host and runtime versions differ")
    runtime_archive = runtime_directory / validate_component(runtime["asset"])
    host_archive = host_directory / validate_component(host["asset"])
    probe_host(host_archive, host)
    toolchain = identify_toolchain(profile, target, ndk=ndk, emscripten=emscripten, cc=cc, cxx=cxx)
    with tempfile.TemporaryDirectory(prefix="axiom-sdk-consumer-") as directory:
        workspace = Path(directory)
        runtime_root, host_root, build = workspace / "runtime", workspace / "host", workspace / "build"
        install_runtime(runtime_archive, runtime_root, runtime)
        install_host(host_archive, host_root, host)
        protoc = host_root / host["identity"]["executable"]
        command = ["cmake", "-S", str(ROOT / "tools/semantic/smoke"), "-B", str(build), "-G", "Ninja",
                   "-DCMAKE_BUILD_TYPE=Release", *toolchain.cmake_args,
                   *consumer_cmake_arguments(runtime_root, protoc)]
        subprocess.run(command, check=True)
        subprocess.run(["cmake", "--build", str(build), "--parallel", "2"], check=True)
        execution = "not-run-cross-target"
        if target == "web-wasm32":
            subprocess.run(["node", str(build / "semantic_sdk_smoke.js")], check=True)
            execution = "node"
        elif can_execute_target(detect_host_platform(), target):
            suffix = ".exe" if target.startswith("windows") else ""
            subprocess.run([str(build / ("semantic_sdk_smoke" + suffix))], check=True)
            execution = "native"
    return {"format": "axiom-semantic-sdk-smoke-v2", "status": "PASS", "target": target,
            "host": detect_host_platform().key, "runtimeId": runtime["runtimeId"],
            "hostToolId": host["hostToolId"], "runtimeSha256": runtime["sha256"],
            "hostSha256": host["sha256"], "sourceFree": True, "linkedExecutable": True,
            "execution": execution}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=RUNTIME_KEYS, required=True)
    parser.add_argument("--runtime-directory", type=Path, required=True)
    parser.add_argument("--host-directory", type=Path, required=True)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--emscripten", type=Path)
    parser.add_argument("--cc")
    parser.add_argument("--cxx")
    parser.add_argument("--facts-output", type=Path)
    args = parser.parse_args()
    facts = smoke_consumer(args.target, args.runtime_directory, args.host_directory,
                           profile=load_profile(args.profile), ndk=args.ndk, emscripten=args.emscripten,
                           cc=args.cc, cxx=args.cxx)
    if args.facts_output is not None:
        args.facts_output.parent.mkdir(parents=True, exist_ok=True)
        args.facts_output.write_bytes(canonical_bytes(facts) + b"\n")
    print(canonical_bytes(facts).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
