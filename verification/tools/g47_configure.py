"""Configure a G4.7 consumer using locked, previously resolved SDK facts."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.semantic.cmake_consumer import consumer_cmake_arguments

parser = argparse.ArgumentParser()
parser.add_argument("platform", choices=["web", "android"])
parser.add_argument("--facts", type=Path, required=True)
parser.add_argument("--build", required=True)
parser.add_argument("--skia", type=Path, required=True)
parser.add_argument("--ndk", type=Path)
parser.add_argument("--gradle-arguments-output", type=Path,
                    help="Write resolved CMake -D arguments for the Android app instead of configuring")
args = parser.parse_args()
env = json.loads(args.facts.read_text(encoding="utf-8"))["environment"]
command = ["emcmake", "cmake"] if args.platform == "web" else ["cmake"]
command += ["-S", str(ROOT), "-B", args.build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
    "-DCANVAS_BUILD_POC01=OFF", "-DCANVAS_BUILD_POC02=OFF", "-DCANVAS_BUILD_POC03=OFF",
    "-DCANVAS_BUILD_RF01=ON", "-DCANVAS_BUILD_RENDER=ON", "-DCANVAS_BUILD_ARC=ON",
    "-DCANVAS_BUILD_INK_PLAYGROUND=ON", "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON",
    "-DCANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH=ON", "-DAXIOM_BUILD_DEBUG_UI=OFF",
    "-DBUILD_TESTING=OFF", f"-DCANVAS_SKIA_SDK_ROOT={args.skia.resolve().as_posix()}",
    *consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(env["AXIOM_PROTOC"]))]
if args.platform == "android":
    if args.ndk is None:
        parser.error("--ndk is required for Android")
    command += [f"-DCMAKE_TOOLCHAIN_FILE={args.ndk.resolve().as_posix()}/build/cmake/android.toolchain.cmake",
                "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-26"]
if args.gradle_arguments_output:
    if args.platform != "android":
        parser.error("--gradle-arguments-output is Android-only")
    args.gradle_arguments_output.parent.mkdir(parents=True, exist_ok=True)
    args.gradle_arguments_output.write_text(
        json.dumps([arg for arg in command if arg.startswith("-D")], indent=2) + "\n",
        encoding="utf-8")
else:
    raise SystemExit(subprocess.call(command, cwd=ROOT, shell=sys.platform == "win32" and args.platform == "web"))
