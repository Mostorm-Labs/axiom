"""Run source-qualified FC-01 builds and tests without changing frozen Expected."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def windows_environment() -> dict[str, str]:
    env = {k.upper(): v for k, v in os.environ.items()}
    vcvars = Path(env.get("FC01_VCVARS", r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"))
    p = subprocess.run(f'call "{vcvars}" >nul && set', shell=True, check=True, capture_output=True)
    for line in p.stdout.decode("mbcs").splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key.upper()] = value
    compiler = Path(env.get("FC01_CLANG_CL", r"D:\ax8\.deps\llvm\bin\clang-cl.exe"))
    if not compiler.is_file():
        raise RuntimeError(f"clang-cl missing: {compiler}")
    env["PATH"] = str(compiler.parent) + os.pathsep + env["PATH"]
    env["CC"] = env["CXX"] = str(compiler)
    env["VSLANG"] = "1033"
    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=["core", "windows-host", "web-wasm", "android-native"], default="core")
    parser.add_argument("--build-dir", type=Path, default=Path(r"D:\axbuild\fc01-core"))
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--test-regex")
    parser.add_argument("--label", default="run")
    parser.add_argument("--no-protobuf", action="store_true")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    if args.profile != "core":
        raise RuntimeError("Host profiles require their FC01-I5 configuration; not yet implemented or qualified")
    build = args.build_dir.resolve()
    logs = ROOT / "out/fc01/runs" / args.label
    logs.mkdir(parents=True, exist_ok=True)
    env = windows_environment()
    cmake = shutil.which("cmake", path=env["PATH"]) or r"D:\Softwares\CMake\bin\cmake.exe"
    ninja = shutil.which("ninja", path=env["PATH"]) or r"D:\git\depot_tools\ninja.exe"
    ctest = str(Path(cmake).with_name("ctest.exe"))
    commands = []
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()

    def run(command: list[str], name: str) -> int:
        path = logs / (name + ".log")
        with path.open("wb") as output:
            p = subprocess.run(command, cwd=ROOT, env=env, stdout=output, stderr=subprocess.STDOUT)
        commands.append({"name": name, "command": command, "exit_code": p.returncode,
                         "log": str(path), "log_sha256": digest(path)})
        print(name, "exit", p.returncode, flush=True)
        if p.returncode:
            tail = path.read_bytes()[-7000:].decode("utf-8", "replace")
            print(tail.encode("ascii", "backslashreplace").decode("ascii"), flush=True)
        return p.returncode

    configure = [cmake, "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_MAKE_PROGRAM={ninja}",
                 f"-DCMAKE_C_COMPILER={env['CC']}", f"-DCMAKE_CXX_COMPILER={env['CXX']}",
                 "-DCANVAS_BUILD_POC01=OFF", "-DCANVAS_BUILD_POC02=OFF", "-DCANVAS_BUILD_POC03=OFF",
                 "-DCANVAS_BUILD_POC05=OFF", "-DCANVAS_BUILD_INK_PLAYGROUND=OFF",
                 "-DCANVAS_BUILD_RF01=OFF", "-DCANVAS_BUILD_RENDER=OFF", "-DCANVAS_BUILD_SEMANTIC=ON",
                 "-DBUILD_TESTING=ON", "-DCANVAS_SKIA_SDK_VARIANT=release",
                 "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=" + ("OFF" if args.no_protobuf else "ON")]
    rc = 0
    if not args.no_protobuf:
        facts_path = logs / "semantic-environment.json"
        rc = run([sys.executable, "tools/setup_build_environment.py", "--semantic", "--offline",
                  "--facts-output", str(facts_path)], "sdk-resolution")
        if rc == 0:
            facts = json.loads(facts_path.read_bytes())["environment"]
            configure += consumer_cmake_arguments(Path(facts["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(facts["AXIOM_PROTOC"]))
    if rc == 0:
        rc = run(configure, "configure")
    if rc == 0 and not args.configure_only:
        command = [cmake, "--build", str(build), "--parallel", "4"]
        if args.target:
            command += ["--target", *args.target]
        rc = run(command, "build")
    if rc == 0 and not args.configure_only and not args.build_only:
        command = [ctest, "--test-dir", str(build), "--output-on-failure"]
        if args.test_regex:
            command += ["-R", args.test_regex]
        rc = run(command, "test")
    report = {"task_id": "FC-01", "profile": args.profile, "source_revision": source,
              "working_tree_dirty": bool(dirty), "commands": commands, "exit_code": rc,
              "physical_claim": False, "full_closure_claim": False,
              "compiler": env["CXX"], "compiler_sha256": digest(Path(env["CXX"]))}
    (logs / "result.json").write_bytes((json.dumps(report, indent=2) + "\n").encode())
    return rc


if __name__ == "__main__":
    sys.exit(main())
