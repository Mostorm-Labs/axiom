"""Source-pinned FC01-RED-01 oracle; expected values live in the C++ test."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "2942c63b375366ed977b834aa0a48fe2fb551c6e"
VCVARS = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main() -> int:
    output = ROOT / "out/fc01/source-loss"
    output.mkdir(parents=True, exist_ok=True)
    environment = subprocess.run(
        f'call "{VCVARS}" >nul && set', shell=True, capture_output=True,
        text=True, check=True,
    )
    import os
    env = dict(os.environ)
    for line in environment.stdout.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key.upper()] = value
    compiler = shutil.which("cl.exe", path=env.get("PATH"))
    if compiler is None:
        raise RuntimeError("vcvars64 did not expose cl.exe")
    compiler_version = subprocess.run([compiler, "/Bv"], env=env, capture_output=True)
    (output / "compiler-version.log").write_bytes(compiler_version.stdout + compiler_version.stderr)
    baseline_root = output / "baseline"
    files = subprocess.check_output(
        ["git", "ls-tree", "-r", "--name-only", BASELINE, "runtime/input/include",
         "runtime/input/src/active_pointer_registry.cpp",
         "runtime/input/src/platform_interaction_ingress.cpp"], cwd=ROOT, text=True,
    ).splitlines()
    for relative in files:
        path = baseline_root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(subprocess.check_output(["git", "show", BASELINE + ":" + relative], cwd=ROOT))
    source_revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain", "--",
                                    "runtime/input", "verification/fc01"], cwd=ROOT, text=True).strip()
    rows = []
    for name, source in [("baseline", baseline_root), ("candidate", ROOT)]:
        build = output / (name + "-build")
        build.mkdir(exist_ok=True)
        binary = build / "source-loss.exe"
        command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/UNDEBUG",
                   "/I" + str(source / "runtime/input/include"),
                   str(ROOT / "verification/fc01/red_source_loss_test.cpp"),
                   str(source / "runtime/input/src/active_pointer_registry.cpp"),
                   str(source / "runtime/input/src/platform_interaction_ingress.cpp"),
                   "/Fe:" + str(binary)]
        compiled = subprocess.run(command, env=env, cwd=build, capture_output=True)
        (output / (name + "-build.log")).write_bytes(compiled.stdout + compiled.stderr)
        if compiled.returncode:
            print(compiled.stdout + compiled.stderr)
            return compiled.returncode
        observed = subprocess.run([str(binary)], env=env, cwd=build, capture_output=True)
        (output / (name + "-run.log")).write_bytes(observed.stdout + observed.stderr)
        rows.append({"profile": name, "source_revision": BASELINE if name == "baseline" else (None if dirty else source_revision),
                     "source_base_revision": source_revision if name == "candidate" else BASELINE,
                     "source_is_working_tree": name == "candidate", "uncommitted_source_changes": bool(dirty) if name == "candidate" else False, "command": command,
                     "build_exit_code": compiled.returncode, "run_exit_code": observed.returncode,
                     "binary_sha256": sha(binary), "source_sha256": sha(source / "runtime/input/src/platform_interaction_ingress.cpp"),
                     "oracle_sha256": sha(ROOT / "verification/fc01/red_source_loss_test.cpp")})
        print(name, observed.returncode, observed.stdout + observed.stderr)
    regressions = []
    for test in ["platform_interaction_ingress_test", "c1_platform_fixture_test"]:
        build = output / "regression-build"
        build.mkdir(exist_ok=True)
        binary = build / (test + ".exe")
        command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/UNDEBUG",
                   "/I" + str(ROOT / "runtime/input/include"),
                   str(ROOT / ("runtime/input/tests/" + test + ".cpp")),
                   str(ROOT / "runtime/input/src/active_pointer_registry.cpp"),
                   str(ROOT / "runtime/input/src/platform_interaction_ingress.cpp"),
                   "/Fe:" + str(binary)]
        compiled = subprocess.run(command, env=env, cwd=build, capture_output=True)
        observed = subprocess.run([str(binary)], env=env, cwd=build, capture_output=True) if compiled.returncode == 0 else None
        (output / (test + ".log")).write_bytes(compiled.stdout + compiled.stderr +
                    (observed.stdout + observed.stderr if observed else b"not run"))
        regressions.append({"test": test, "command": command, "build_exit_code": compiled.returncode,
                            "run_exit_code": observed.returncode if observed else None,
                            "binary_sha256": sha(binary) if compiled.returncode == 0 else None})
        print(test, regressions[-1]["build_exit_code"], regressions[-1]["run_exit_code"])
    baseline_log = (output / "baseline-run.log").read_bytes()
    report = {"id": "FC01-RED-01", "baseline": BASELINE, "runs": rows, "regressions": regressions,
              "provider": {"kind": "local-windows-native", "compiler": compiler,
                           "compiler_sha256": sha(Path(compiler)),
                           "version_command": [compiler, "/Bv"], "version_exit_code": compiler_version.returncode,
                           "vcvars": str(VCVARS), "vcvars_sha256": sha(VCVARS),
                           "windows_sdk_version": env.get("WINDOWSSDKVERSION")},
              "expected_baseline_red": rows[0]["run_exit_code"] != 0 and
                 b"Assertion failed: ingress.activeCount() == 1" in baseline_log,
              "candidate_green": rows[1]["run_exit_code"] == 0,
              "full_FC01_B03_closed": False, "red_before_first_production_mutation_proven": False,
              "physical_claim": False}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if (report["expected_baseline_red"] and report["candidate_green"] and
                 all(r["build_exit_code"] == 0 and r["run_exit_code"] == 0 for r in regressions)) else 1

if __name__ == "__main__":
    sys.exit(main())
