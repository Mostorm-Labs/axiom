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
from tools.semantic.toolchain import target_cmake_arguments
from tools.semantic.contract import load_profile


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def windows_environment() -> dict[str, str]:
    env = {k.upper(): v for k, v in os.environ.items()}
    vcvars = Path(env.get("FC01_VCVARS", r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"))
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
    parser.add_argument("--scene", action="store_true")
    parser.add_argument("--playground", action="store_true",
                        help="Enable the common Ink Playground host and its focused tests")
    parser.add_argument("--skia-root", type=Path)
    parser.add_argument("--emscripten-root", type=Path,
                        help="Pinned Emscripten root for the web-wasm profile")
    parser.add_argument("--android-ndk", type=Path,
                        help="Pinned Android NDK root for the android-native profile")
    parser.add_argument("--windows-host", action="store_true")
    parser.add_argument("--debug-ui", action="store_true")
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    if args.profile == "windows-host" and not args.windows_host:
        args.windows_host = True
    if args.profile == "web-wasm" and args.emscripten_root is None:
        raise RuntimeError("web-wasm requires --emscripten-root for the pinned Emscripten toolchain")
    if args.profile == "android-native" and args.android_ndk is None:
        raise RuntimeError("android-native requires --android-ndk for the pinned NDK")
    build = args.build_dir.resolve()
    logs = ROOT / "out/fc01/runs" / args.label
    logs.mkdir(parents=True, exist_ok=True)
    env = windows_environment()
    if args.profile == "web-wasm":
        emsdk = args.emscripten_root.resolve().parent.parent
        emscripten = args.emscripten_root.resolve()
        env["EMSDK"] = str(emsdk)
        env["EMSDK_PYTHON"] = str(emsdk / "python/3.13.3_64bit/python.exe")
        env["PATH"] = str(emscripten) + os.pathsep + str(emsdk / "upstream/bin") + os.pathsep + env["PATH"]
        env["CC"] = str(emscripten / "emcc.exe")
        env["CXX"] = str(emscripten / "em++.exe")
    elif args.profile == "android-native":
        env["ANDROID_NDK_ROOT"] = str(args.android_ndk.resolve())
        host_bin = args.android_ndk.resolve() / "toolchains/llvm/prebuilt/windows-x86_64/bin"
        env["CC"] = str(host_bin / "clang.exe")
        env["CXX"] = str(host_bin / "clang++.exe")
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

    cxx_flags = "/UNDEBUG" if args.profile in {"core", "windows-host"} else "-fexceptions"
    release_flags = "/O2 /Ob2" if args.profile in {"core", "windows-host"} else "-O3"
    configure = [cmake, "-S", str(ROOT), "-B", str(build), "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_CXX_FLAGS={cxx_flags}",
                 f"-DCMAKE_CXX_FLAGS_RELEASE={release_flags}", f"-DCMAKE_MAKE_PROGRAM={ninja}",
                 "-DCANVAS_BUILD_POC01=OFF", "-DCANVAS_BUILD_POC02=OFF", "-DCANVAS_BUILD_POC03=OFF",
                 "-DCANVAS_BUILD_POC05=OFF",
                 "-DCANVAS_BUILD_INK_PLAYGROUND=" + ("ON" if args.playground else "OFF"),
                 "-DCANVAS_BUILD_RF01=" + ("ON" if args.scene else "OFF"), "-DCANVAS_BUILD_RENDER=OFF", "-DCANVAS_BUILD_SEMANTIC=ON",
                 "-DBUILD_TESTING=ON", "-DCANVAS_SKIA_SDK_VARIANT=release",
                 "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=" + ("OFF" if args.no_protobuf else "ON")]
    if args.profile in {"core", "windows-host"}:
        configure += [f"-DCMAKE_C_COMPILER={env['CC']}", f"-DCMAKE_CXX_COMPILER={env['CXX']}"]
    else:
        configure += ["-DBUILD_TESTING=OFF", "-DCANVAS_BUILD_ARC=ON",
                      "-DCANVAS_BUILD_RF01=ON", "-DCANVAS_BUILD_RENDER=ON",
                      "-DCANVAS_BUILD_INK_PLAYGROUND=ON",
                      "-DAXIOM_BUILD_DEBUG_UI=" + ("ON" if args.debug_ui else "OFF"),
                      "-DAXIOM_DEBUG_UI_PROFILE=" + ("full" if args.debug_ui else "off")]
    if args.skia_root:
        configure += [f"-DCANVAS_SKIA_SDK_ROOT={args.skia_root.resolve().as_posix()}",
                      "-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON"]
    if args.windows_host:
        configure += ["-DCANVAS_BUILD_ARC=ON", "-DCANVAS_BUILD_RF01=ON", "-DCANVAS_BUILD_RENDER=ON",
                      "-DCANVAS_BUILD_INK_PLAYGROUND=ON", "-DAXIOM_BUILD_DEBUG_UI=ON",
                      "-DAXIOM_DEBUG_UI_PROFILE=full"]
    rc = 0
    if not args.no_protobuf:
        facts_path = logs / "semantic-environment.json"
        target = "web-wasm32" if args.profile == "web-wasm" else \
            "android-arm64-v8a" if args.profile == "android-native" else None
        resolution = [sys.executable, "tools/setup_build_environment.py", "--semantic", "--offline"]
        if target:
            resolution += ["--target", target]
        resolution += ["--facts-output", str(facts_path)]
        rc = run(resolution,
                 "sdk-resolution")
        if rc == 0:
            facts = json.loads(facts_path.read_bytes())["environment"]
            configure += consumer_cmake_arguments(Path(facts["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(facts["AXIOM_PROTOC"]))
            if args.profile == "web-wasm":
                configure += target_cmake_arguments(
                    load_profile(), "web-wasm32",
                    emscripten=args.emscripten_root.resolve())
            elif args.profile == "android-native":
                configure += target_cmake_arguments(
                    load_profile(), "android-arm64-v8a",
                    ndk=args.android_ndk.resolve())
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
