#!/usr/bin/env python3
"""Exercise a real installed SDK from a fresh directory outside the source tree."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--include-dir", required=True, type=Path)
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--runtime-dir", type=Path)
    parser.add_argument("--shared", action="store_true")
    parser.add_argument("--generator")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    include = args.include_dir.resolve()
    library = args.library.resolve()
    header = include / "canvas_runtime_api_v1.h"
    result = {"status": "NOT_RUN", "checks": [], "input": {"include": str(include),
              "library": str(library), "shared": args.shared}}

    def finish(code):
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"status": result["status"], "output": str(args.output)}))
        return code

    if include.is_relative_to(repo) or library.is_relative_to(repo):
        result.update(status="BLOCKED_INPUT", reason="SDK header and library must be installed outside the repository")
        return finish(2)
    if not header.is_file() or not library.is_file():
        result.update(status="BLOCKED_INPUT", reason="installed SDK header or library missing")
        return finish(2)
    if args.runtime_dir and not args.runtime_dir.is_dir():
        result.update(status="BLOCKED_INPUT", reason="runtime directory missing")
        return finish(2)
    result["input"].update(header_sha256=digest(header), library_sha256=digest(library))
    env = os.environ.copy()
    if args.runtime_dir:
        env["PATH"] = str(args.runtime_dir.resolve()) + os.pathsep + env.get("PATH", "")
    source = Path(__file__).resolve().parent / "external-consumer"
    with tempfile.TemporaryDirectory(prefix="axiom-sdk01-consumer-") as temp:
        external = Path(temp) / "consumer"
        build = Path(temp) / "build"
        shutil.copytree(source, external)
        junit = Path(temp) / "results.xml"
        commands = [["cmake", "-S", str(external), "-B", str(build),
                     "-DAXIOM_SDK_INCLUDE_DIR=" + str(include),
                     "-DAXIOM_SDK_LIBRARY=" + str(library),
                     "-DAXIOM_SDK_SHARED=" + ("ON" if args.shared else "OFF")],
                    ["cmake", "--build", str(build), "--config", "Release"],
                    ["ctest", "--test-dir", str(build), "-C", "Release", "-R",
                     "^SDK01External(CSmoke|CppLink)$", "--output-on-failure",
                     "--no-tests=error", "--output-junit", str(junit)]]
        if args.generator:
            commands[0].extend(["-G", args.generator])
        for command in commands:
            try:
                run = subprocess.run(command, capture_output=True, text=True, env=env, check=False)
            except OSError as exc:
                result["checks"].append({"command": command, "error": str(exc)})
                result["status"] = "BLOCKED_ENVIRONMENT"
                return finish(2)
            result["checks"].append({"command": command, "exit_code": run.returncode,
                                     "stdout": run.stdout, "stderr": run.stderr})
            if run.returncode:
                result["status"] = "FAILED_OR_BLOCKED"
                return finish(1)
        try:
            root = ET.parse(junit).getroot()
            cases = root.findall(".//testcase")
            names = {case.get("name") for case in cases}
            expected = {"SDK01ExternalCSmoke", "SDK01ExternalCppLink"}
            if len(cases) != 2 or names != expected or any(
                    case.find("failure") is not None or case.find("error") is not None or
                    case.find("skipped") is not None for case in cases):
                raise ValueError("expected two distinct passing tests, with no skips")
            result["junit"] = junit.read_text(encoding="utf-8")
        except (OSError, ET.ParseError, ValueError) as exc:
            result.update(status="FAILED_EVIDENCE", reason=str(exc))
            return finish(1)
    result["status"] = "PASS_SCOPED_EXTERNAL_SMOKE"
    return finish(0)


if __name__ == "__main__":
    sys.exit(main())
