"""Execute selected FC-01 tests without turning aggregate green into Gate closure.

Uses an already SDK-configured CMake build. Expected values remain in the
source-pinned tests/frozen corpus; this runner records observations and identities.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--vcvars", type=Path)
    parser.add_argument("--target", nargs="+", default=[])
    parser.add_argument("--test-regex", required=True)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    # Retain failed attempts. Never overwrite an earlier run's evidence.
    output.mkdir(parents=True, exist_ok=False)
    environment = os.environ.copy()
    if args.vcvars:
        if os.name != "nt" or not args.vcvars.is_file():
            parser.error("--vcvars requires an existing Windows native toolchain script")
        toolchain = subprocess.check_output(
            f'cmd.exe /d /s /c ""{args.vcvars.resolve()}" >nul && set"', text=True)
        for line in toolchain.splitlines():
            if "=" in line and not line.startswith("="):
                key, value = line.split("=", 1)
                environment[key] = value
    report = {"started_at": datetime.now(timezone.utc).isoformat(),
              "repository": git("remote", "get-url", "origin"),
              "source_revision": git("rev-parse", "HEAD"),
              "working_tree_changes": git("status", "--porcelain").splitlines(),
              "build": str(build), "commands": [], "tests": [],
              "status": "RUNNING", "physical_claim": False, "gate_claim": None}

    def save() -> None:
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    def run(command: list[str], name: str) -> int:
        with (output / (name + ".log")).open("w", encoding="utf-8") as log:
            result = subprocess.run(command, env=environment, cwd=ROOT, stdout=log,
                                    stderr=subprocess.STDOUT, text=True)
        report["commands"].append({"name": name, "command": command, "exit_code": result.returncode})
        save()
        print(name, result.returncode, flush=True)
        return result.returncode

    save()
    if args.target and run(["cmake", "--build", str(build), "--target", *args.target,
                            "--parallel", "4"], "build"):
        report["status"] = "BUILD_FAIL"
        save()
        return 1
    inventory = subprocess.check_output(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                                       env=environment, text=True)
    (output / "ctest-inventory.json").write_text(inventory, encoding="utf-8")
    selected = [test for test in json.loads(inventory)["tests"]
                if re.search(args.test_regex, test["name"])]
    if not selected:
        report["status"] = "NO_TESTS_SELECTED"
        save()
        return 1
    binaries = {}
    for test in selected:
        command = test.get("command", [])
        if command and Path(command[0]).is_file():
            binary = Path(command[0])
            binaries[str(binary)] = {"sha256": digest(binary), "bytes": binary.stat().st_size}
    report["binaries"] = binaries
    report["selected_tests"] = [test["name"] for test in selected]
    junit = output / "ctest.xml"
    rc = run(["ctest", "--test-dir", str(build), "--output-on-failure", "--no-tests=error",
              "--output-junit", str(junit), "-R", args.test_regex], "test")
    if junit.is_file():
        for case in ET.parse(junit).getroot().iter("testcase"):
            report["tests"].append({"name": case.attrib["name"],
                "result": "FAIL" if case.find("failure") is not None else
                          "NOT_RUN" if case.find("skipped") is not None else "PASS",
                "seconds": case.attrib.get("time")})
    stable = all(Path(path).is_file() and digest(Path(path)) == fact["sha256"]
                 for path, fact in binaries.items())
    report["binaries_unchanged_during_run"] = stable
    complete = len(report["tests"]) == len(selected) and all(
        test["result"] == "PASS" for test in report["tests"])
    report["status"] = "TEST_EXECUTION_PASS_NOT_FC01_CLOSURE" if rc == 0 and complete and stable else "TEST_FAIL"
    report["finished_at"] = datetime.now(timezone.utc).isoformat()
    save()
    print(report["status"], len(report["tests"]), flush=True)
    return 0 if report["status"] == "TEST_EXECUTION_PASS_NOT_FC01_CLOSURE" else 1


if __name__ == "__main__":
    sys.exit(main())
