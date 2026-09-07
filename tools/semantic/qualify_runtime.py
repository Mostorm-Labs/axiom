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
from tools.semantic.package_runtime import package_runtime
from tools.semantic.smoke_consumer import smoke_consumer


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=RUNTIME_KEYS, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "out/semantic-sdk/runtime")
    parser.add_argument("--host-directory", type=Path, default=ROOT / "out/semantic-sdk/host")
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
    toolchain_file = args.output / "toolchain.json"
    install_root = args.output / "install"
    subprocess.run([sys.executable, str(ROOT / "tools/semantic/build_runtime.py"), "--target", args.target,
                    "--install-root", str(install_root), "--toolchain-json", str(toolchain_file), *extra], check=True)
    toolchain = read_json(toolchain_file)
    first = args.output / "package-a"
    second = args.output / "package-b"
    a = package_runtime(lock, profile, args.target, install_root, toolchain, first)
    b = package_runtime(lock, profile, args.target, install_root, toolchain, second)
    if a != b or file_sha256(first / a["asset"]) != file_sha256(second / b["asset"]):
        raise SdkError("semantic: runtime packaging is not deterministic")
    for directory in (first, second):
        (directory / "record.json").write_bytes(canonical_bytes(a) + b"\n")
    facts = smoke_consumer(args.target, first, args.host_directory, profile=profile,
                           ndk=ndk, emscripten=emscripten, cc=cc, cxx=cxx)
    (args.output / "smoke.json").write_bytes(canonical_bytes(facts) + b"\n")
    print(canonical_bytes(facts).decode())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
