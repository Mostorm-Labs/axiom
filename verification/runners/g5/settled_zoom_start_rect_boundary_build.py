"""Build only the offline boundary tool with the existing locked native SDKs.

Example: python3 verification/runners/g5/settled_zoom_start_rect_boundary_build.py
  --build out/G5-CPUW-checkpoint-20261113/build-native
  --output out/G5-settled-zoom-start-rect-repair-r2/offline-pixel/reproducible
No CMake configure, dependency build, or browser run is performed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    original = ROOT / "runtime/render/tests/corner_join_test.cpp"
    source = ROOT / "verification/runners/g5/settled_zoom_start_rect_boundary.cpp"
    old_object = "runtime/render/CMakeFiles/axiom_corner_join_tests.dir/tests/corner_join_test.cpp.o"
    new_object = output / "boundary.o"
    binary = output / "boundary"
    commands = subprocess.check_output(
        ["ninja", "-C", str(build), "-t", "commands", "axiom_corner_join_tests"],
        text=True,
    ).splitlines()
    compile_line = next(line for line in commands if f" -c {original}" in line)
    compile_args = [str(source) if v == str(original) else str(new_object)
                    if v == old_object else v for v in shlex.split(compile_line)]
    link_line = commands[-1]
    assert link_line.startswith(": && ") and link_line.endswith(" && :")
    link_args = shlex.split(link_line[5:-5])
    link_args = [str(new_object) if v == old_object else str(binary)
                 if v == "runtime/render/axiom_corner_join_tests" else v for v in link_args]
    recipe = {
        "source": str(source.relative_to(ROOT)),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "cwd": str(build),
        "steps": [],
    }
    for name, argv in [("compile", compile_args), ("link", link_args)]:
        result = subprocess.run(argv, cwd=build, capture_output=True, text=True)
        log = output / f"{name}.log"
        log.write_text(result.stdout + result.stderr)
        recipe["steps"].append({"step": name, "argv": argv, "exit": result.returncode,
                                "log": str(log.relative_to(ROOT))})
        (output / "build-recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")
        result.check_returncode()
    recipe["binary"] = str(binary.relative_to(ROOT))
    recipe["binary_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
    (output / "build-recipe.json").write_text(json.dumps(recipe, indent=2) + "\n")
    print(json.dumps({"binary": str(binary), "sha256": recipe["binary_sha256"]}))


if __name__ == "__main__":
    main()
