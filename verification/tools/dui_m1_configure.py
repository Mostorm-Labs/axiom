"""Configure GT-DUI-M1 with the repository's locked SDK consumer path."""
from __future__ import annotations
import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=("windows", "web", "android"))
    parser.add_argument("--build", required=True)
    parser.add_argument("--skia", type=Path)
    parser.add_argument("--facts", type=Path)
    parser.add_argument("--gradle-arguments-output", type=Path,
                        help="Write resolved Android CMake -D arguments for Gradle")
    args = parser.parse_args()
    command = ["cmake", "-S", str(ROOT), "-B", args.build, "-G", "Ninja",
               "-DCANVAS_BUILD_POC01=OFF", "-DCANVAS_BUILD_POC02=OFF",
               "-DCANVAS_BUILD_POC03=OFF", "-DCANVAS_BUILD_RF01=ON",
               "-DCANVAS_BUILD_RENDER=ON", "-DCANVAS_BUILD_ARC=ON",
               "-DCANVAS_BUILD_INK_PLAYGROUND=ON", "-DCANVAS_BUILD_CONTROL=ON",
               "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON", "-DAXIOM_BUILD_DEBUG_UI=ON",
               "-DAXIOM_DEBUG_UI_PROFILE=full", "-DBUILD_TESTING=ON"]
    if args.skia:
        command.append(f"-DCANVAS_SKIA_SDK_ROOT={args.skia.resolve()}")
    if args.facts:
        import json
        facts = json.loads(args.facts.read_text(encoding="utf-8"))["environment"]
        command += [f"-DAXIOM_PROTOC={facts['AXIOM_PROTOC']}",
                    f"-DCMAKE_PREFIX_PATH={facts['CMAKE_PREFIX_PATH']};{facts['CMAKE_PREFIX_PATH']}/lib/cmake",
                    f"-DProtobuf_DIR={facts['CMAKE_PREFIX_PATH']}/lib/cmake/protobuf",
                    f"-Dabsl_DIR={facts['CMAKE_PREFIX_PATH']}/lib/cmake/absl",
                    f"-Dutf8_range_DIR={facts['CMAKE_PREFIX_PATH']}/lib/cmake/utf8_range"]
    if args.platform == "web":
        command[0:1] = ["emcmake.bat", "cmake"]
    elif args.platform == "android":
        ndk = Path(__import__("os").environ.get("ANDROID_NDK_ROOT", ""))
        if not ndk.exists():
            print("ANDROID_NDK_ROOT is required for Android", file=sys.stderr)
            return 2
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build/cmake/android.toolchain.cmake'}")
        command += ["-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-26"]
        if args.gradle_arguments_output:
            command = [arg for arg in command if arg != "-DBUILD_TESTING=ON"]
            command.append("-DBUILD_TESTING=OFF")
    if args.gradle_arguments_output:
        if args.platform != "android":
            parser.error("--gradle-arguments-output is Android-only")
        args.gradle_arguments_output.parent.mkdir(parents=True, exist_ok=True)
        import json
        args.gradle_arguments_output.write_text(
            json.dumps([arg for arg in command if arg.startswith("-D")], indent=2) + "\n",
            encoding="utf-8")
        return 0
    return subprocess.call(command, cwd=ROOT,
                           shell=os.name == "nt" and args.platform == "web")

if __name__ == "__main__":
    raise SystemExit(main())
