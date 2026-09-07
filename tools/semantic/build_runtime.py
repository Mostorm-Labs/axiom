#!/usr/bin/env python3
"""Producer-only native/cross Protobuf and Abseil runtime builder."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.sdk.archive import canonical_bytes
from tools.sdk.model import SdkError
from tools.semantic.contract import ROOT, DEFAULT_PROFILE, RUNTIME_KEYS, load_profile, read_json
from tools.semantic.sources import source_archive, safe_extract_source_tar
from tools.semantic.toolchain import identify_toolchain


def runtime_build_commands(protobuf_source: Path, abseil_source: Path, build_root: Path,
                           install_root: Path, target_args: list[str] | tuple[str, ...],
                           *, jobs: int = 2) -> tuple[list[str], ...]:
    if type(jobs) is not int or jobs < 1:
        raise SdkError("semantic: build jobs must be a positive integer")
    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_STANDARD=20",
              "-DCMAKE_CXX_STANDARD_REQUIRED=ON", "-DCMAKE_INSTALL_LIBDIR=lib",
              f"-DCMAKE_INSTALL_PREFIX={install_root.resolve().as_posix()}", *target_args]
    abseil_build, protobuf_build = build_root / "abseil", build_root / "protobuf"
    return (
        ["cmake", "-S", str(abseil_source), "-B", str(abseil_build), *common,
         "-DABSL_BUILD_TESTING=OFF", "-DABSL_ENABLE_INSTALL=ON", "-DABSL_PROPAGATE_CXX_STD=ON"],
        ["cmake", "--build", str(abseil_build), "--target", "install", "--parallel", str(jobs)],
        ["cmake", "-S", str(protobuf_source), "-B", str(protobuf_build), *common,
         "-Dprotobuf_BUILD_TESTS=OFF", "-Dprotobuf_BUILD_CONFORMANCE=OFF", "-Dprotobuf_BUILD_EXAMPLES=OFF",
         "-Dprotobuf_BUILD_LIBPROTOC=OFF", "-Dprotobuf_BUILD_PROTOC_BINARIES=OFF", "-Dprotobuf_BUILD_LIBUPB=OFF",
         "-Dprotobuf_ABSL_PROVIDER=package", "-Dprotobuf_BUILD_SHARED_LIBS=OFF",
         "-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON", "-Dprotobuf_WITH_ZLIB=OFF",
         f"-DCMAKE_PREFIX_PATH={install_root.resolve().as_posix()}",
         f"-Dabsl_DIR={install_root.resolve().as_posix()}/lib/cmake/absl"],
        ["cmake", "--build", str(protobuf_build), "--target", "install", "--parallel", str(jobs)],
    )


def _prefix_maps(args: list[str], source: Path, build: Path, windows: bool,
                 *, install_root: Path | None = None) -> list[str]:
    # Normalize __FILE__/debug paths without making a checkout path an SDK ID.
    # Prefix replacement compares spelling, not filesystem identity. In
    # particular Windows temporary roots can use an 8.3 alias while /I and
    # CMake use the resolved long path. Match that same real spelling first.
    if windows:
        source, build = source.resolve(), build.resolve()
        if install_root is not None:
            install_root = install_root.resolve()
    option = "/clang:" if windows else ""
    maps = [f'{option}-ffile-prefix-map="{source.as_posix()}"=/axiom-sdk/src',
            f'{option}-fdebug-prefix-map="{build.as_posix()}"=/axiom-sdk/build']
    if windows and install_root is not None:
        maps.append(f'{option}-ffile-prefix-map="{install_root.as_posix()}"=/axiom-sdk/install')
    result = list(args)
    for field in ("CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS"):
        prefix = f"-D{field}="
        old = next((item for item in result if item.startswith(prefix)), prefix)
        result = [item for item in result if not item.startswith(prefix)]
        result.append(old + " " + " ".join(maps))
    if windows:
        launcher = ";".join((sys.executable, str(Path(__file__).with_name("windows_compile.py"))))
        for language in ("C", "CXX"):
            prefix = f"-DCMAKE_{language}_FLAGS="
            for index, flag in enumerate(result):
                if flag.startswith(prefix) and "/Brepro" not in flag.split():
                    result[index] += " /Brepro"
            result.append(f"-DCMAKE_{language}_COMPILER_LAUNCHER={launcher}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=RUNTIME_KEYS, required=True)
    parser.add_argument("--install-root", type=Path, required=True)
    parser.add_argument("--toolchain-json", type=Path, required=True)
    parser.add_argument("--profile", type=Path, default=DEFAULT_PROFILE)
    parser.add_argument("--deps-lock", type=Path, default=ROOT / "deps.lock.json")
    parser.add_argument("--work-root", type=Path, default=ROOT / "out/semantic-sdk/work")
    parser.add_argument("--cache", type=Path, default=ROOT / "out/semantic-sdk/upstream")
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--emscripten", type=Path)
    parser.add_argument("--cc")
    parser.add_argument("--cxx")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--identify-only", action="store_true")
    parser.add_argument("--facts-output", type=Path)
    args = parser.parse_args()
    lock, profile = read_json(args.deps_lock), load_profile(args.profile)
    selection = identify_toolchain(profile, args.target, ndk=args.ndk, emscripten=args.emscripten, cc=args.cc, cxx=args.cxx)
    args.toolchain_json.parent.mkdir(parents=True, exist_ok=True)
    args.toolchain_json.write_bytes(canonical_bytes(selection.record) + b"\n")
    if args.identify_only:
        print(canonical_bytes(selection.record).decode())
        return 0
    install_root = args.install_root.resolve()
    if install_root.exists() and any(install_root.iterdir()):
        raise SdkError("semantic: producer install prefix must be empty")
    args.work_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=args.target + "-", dir=args.work_root.resolve()))
    # Keep work/logs on failure for diagnosis. No caller-owned directory is deleted.
    protobuf_source = safe_extract_source_tar(source_archive("protobuf", lock, args.cache), work / "sources/protobuf")
    abseil_source = safe_extract_source_tar(source_archive("abseil", lock, args.cache), work / "sources/abseil")
    target_args = _prefix_maps(list(selection.cmake_args), work / "sources", work / "build",
                               args.target.startswith("windows"), install_root=install_root)
    environment = {**os.environ, "SOURCE_DATE_EPOCH": "0", "ZERO_AR_DATE": "1"}
    for command in runtime_build_commands(protobuf_source, abseil_source, work / "build", install_root, target_args, jobs=args.jobs):
        print("+", " ".join(command), flush=True)
        subprocess.run(command, check=True, env=environment)
    licenses = install_root / "licenses"
    licenses.mkdir(parents=True, exist_ok=True)
    for source, name in ((protobuf_source / "LICENSE", "Protobuf.txt"),
                         (protobuf_source / "third_party/utf8_range/LICENSE", "utf8_range.txt"),
                         (abseil_source / "LICENSE", "Abseil.txt")):
        (licenses / name).write_bytes(source.read_bytes())
    facts = {"target": args.target, "installRoot": str(install_root), "workRoot": str(work),
             "sourceRoot": str(work / "sources"), "buildRoot": str(work / "build"),
             "cleanInstall": True}
    if args.facts_output is not None:
        args.facts_output.parent.mkdir(parents=True, exist_ok=True)
        args.facts_output.write_bytes(canonical_bytes(facts) + b"\n")
    print(canonical_bytes(facts).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
