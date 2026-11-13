"""Compile the one measurement TU with accepted existing CMake command/library inputs.

Never invokes a build, CMake regeneration, dependency resolver, or browser.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    root = Path(__file__).resolve().parents[3]
    build = root / "out/G5-CPUW-checkpoint-20261113/build-native"
    output = root / "out/G5-CPUW6a-tail-attribution-r1"
    output.mkdir(parents=True, exist_ok=True)
    source = root / "runtime/composition/tests/cpu_tail_attribution_test.cpp"
    original = "runtime/composition/tests/cpu_pending_runtime_test.cpp"
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(row for row in commands if row["file"] == str(root / original))
    compile_args = shlex.split(entry["command"])
    old_object = compile_args[compile_args.index("-o") + 1]
    new_object = output / "cpu_tail_attribution.o"
    binary = output / "cpu_tail_attribution"
    compile_args[compile_args.index("-o") + 1] = str(new_object)
    compile_args[compile_args.index("-c") + 1] = str(source)
    compile_args = [f'-DAXIOM_SOURCE_ROOT="{root}"' if x.startswith("-DAXIOM_SOURCE_ROOT=") else x for x in compile_args]
    ninja_commands = subprocess.check_output(
        ["ninja", "-C", str(build), "-t", "commands", "axiom_cpu_pending_runtime_tests"], text=True)
    tokens = shlex.split(ninja_commands.splitlines()[-1])
    linker = tokens.index("/usr/bin/c++")
    link_args = tokens[linker:tokens.index("&&", linker)]
    link_args[link_args.index(old_object)] = str(new_object)
    link_args[link_args.index("-o") + 1] = str(binary)
    libraries = sorted({str((build / x).resolve()) for x in link_args if x.endswith(".a")})
    readonly = libraries + [str(build / "CMakeCache.txt"), str(build / "compile_commands.json"), str(root / original)]
    before = {name: sha(Path(name)) for name in readonly}
    record = {"compile": compile_args, "link": link_args, "cwd": str(build),
              "source_sha256": sha(source), "helper_sha256": sha(Path(__file__)), "readonly_hashes": before}
    (output / "compile.json").write_text(json.dumps(record, indent=2) + "\n")
    for label, args in [("compile", compile_args), ("link", link_args)]:
        with (output / (label + ".log")).open("w") as log:
            p = subprocess.run(args, cwd=build, stdout=log, stderr=subprocess.STDOUT)
        record[label + "_returncode"] = p.returncode
        if p.returncode:
            (output / "compile.json").write_text(json.dumps(record, indent=2) + "\n")
            raise SystemExit(label + " failed; inspect preserved log")
    record["binary"] = str(binary)
    record["binary_sha256"] = sha(binary)
    record["readonly_unchanged"] = before == {name: sha(Path(name)) for name in readonly}
    (output / "compile.json").write_text(json.dumps(record, indent=2) + "\n")
    assert record["readonly_unchanged"]
    print(json.dumps({"binary": str(binary), "sha256": record["binary_sha256"], "readonly_libraries": len(libraries)}))


if __name__ == "__main__":
    main()
