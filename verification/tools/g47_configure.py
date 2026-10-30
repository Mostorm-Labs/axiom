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
parser.add_argument("platform", choices=["web"])
parser.add_argument("--facts", type=Path, required=True)
parser.add_argument("--build", required=True)
parser.add_argument("--skia", type=Path, required=True)
args = parser.parse_args()
env = json.loads(args.facts.read_text(encoding="utf-8"))["environment"]
command = ["emcmake", "cmake"] if args.platform == "web" else ["cmake"]
if args.platform == "web":
    command += ["-DAXIOM_BUILD_QUALIFICATION_WEB_HOST=ON"]
command += ["-S", str(ROOT), "-B", args.build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
    "-DCANVAS_BUILD_POC01=OFF", "-DCANVAS_BUILD_POC02=OFF", "-DCANVAS_BUILD_POC03=OFF",
    "-DCANVAS_BUILD_RF01=ON", "-DCANVAS_BUILD_RENDER=ON",
    "-DAXIOM_BUILD_RUNTIME_QUALIFICATION=ON", "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON",
    "-DCANVAS_RENDER_ENABLE_SKIA_PROGRAMMABLE_BRUSH=ON", "-DAXIOM_BUILD_DEBUG_UI=OFF",
    "-DBUILD_TESTING=OFF", f"-DCANVAS_SKIA_SDK_ROOT={args.skia.resolve().as_posix()}",
    *consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(env["AXIOM_PROTOC"]))]
subprocess.run(command, cwd=ROOT, check=True)
