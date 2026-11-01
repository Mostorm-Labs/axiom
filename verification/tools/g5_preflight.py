#!/usr/bin/env python3
"""Read-only G5 prerequisite audit and opt-in, source-bound Web SDK measurements.

This tool prepares evidence. It never signs Gate acceptance or authorizes G5 P32.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import time
import shutil

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
REFERENCE_PATHS = {
    "ReferenceObjectStore": "runtime/semantic/src/reference_object_store.cpp",
    "FullSceneCompiler": "runtime/scene/src/full_scene_compiler.cpp",
    "LinearSpatialIndex": "runtime/scene/src/linear_spatial_index.cpp",
    "DirectCanonicalNonTiled": "runtime/render/src/direct_reference_source.cpp",
    "SharedSkiaDrawing": "runtime/render/src/skia_scene_renderer.cpp",
    "HeadlessReference": "runtime/render/src/skia_headless_backend.cpp",
}
RECORD_PATHS = {
    "G4-registry": ".aegis/gates.json",
    "G4.5-final-vector": ".aegis/results/GT-G4.5-FINAL-VECTOR-CLOSURE/result.json",
    "PX0-07": ".aegis/results/PX0-07/result.json",
    "G4.6-B": ".aegis/results/GT-G4.6-B/p34-control-review.json",
    "G4.6-C": ".aegis/results/GT-G4.6-C/result.json",
    "G4.7": ".aegis/results/GT-G4.7/result.json",
    "FC-01-DEV": ".aegis/results/FC-01-DEV/execution-result.json",
    "FC-03-DEV": ".aegis/results/FC-03-DEV/p32-obligation-closure.json",
    "FC-05-DEV": ".aegis/results/FC-05-DEV/result.json",
}
REQUIRED = ["G4", "G4.4", "G4.5", "G4.6", "G4.7", "PX0-07-original-P34",
            *[f"FC-{i:02d}-independent-DEV" for i in range(1, 13)],
            "FC-13-original-physical", "FC-13-integrated-physical", "exact-integration-ancestry"]
SDK_FILES = ["axiom.mjs", "axiom.d.mts", "package.json", "axiom.wasm", "axiom.data",
             "internal/axiom.mjs", "pointer_input.mjs", "text_input.mjs"]
SOURCE_PREFIXES = ("runtime/", "sdk/", "schema/", "apps/", "assets/", "cmake/", "tools/",
                   "verification/runners/g5/", "verification/sdk_web_external/consumer/",
                   "verification/corpus/performance/g5/")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical(value) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()


def skia_identity(root: Path, lock: dict, target: str) -> dict:
    manifest = json.loads((root / "manifest.json").read_text())
    expected = lock["targets"][target]["variants"]["release"]["sdk_id"]
    if manifest.get("sdk_id") != expected:
        raise ValueError("Skia SDK identity does not match the locked release target")
    return {"root": str(root.resolve()), "target": target, "sdk_id": expected,
            "manifest_sha256": sha256(root / "manifest.json")}


def git(root, *args):
    return subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)


def source_identity(root: Path) -> dict:
    listed = git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard")
    if listed.returncode:
        raise RuntimeError(listed.stderr)
    files = {}
    for name in sorted(set(listed.stdout.split("\0")) - {""}):
        if not (name.startswith(SOURCE_PREFIXES) or name == "CMakeLists.txt" or name.endswith(".lock.json")
                or name in {"AGENTS.md", "verification/tools/g5_preflight.py"}):
            continue
        file = root / name
        if file.is_file():
            files[name] = sha256(file)
    head = git(root, "rev-parse", "HEAD")
    return {"commit": head.stdout.strip() if not head.returncode else None,
            "dirty": bool(git(root, "status", "--porcelain").stdout),
            "tree_sha256": hashlib.sha256(canonical(files)).hexdigest(), "files": files}


def audit(root: Path) -> dict:
    records = []
    for name, relative in RECORD_PATHS.items():
        path = root / relative
        record = {"id": name, "path": relative, "present": path.is_file()}
        if path.is_file():
            data = json.loads(path.read_text())
            record["sha256"] = sha256(path)
            record["recorded_status"] = {key: data[key] for key in (
                "status", "verdict", "terminal_state", "result_revision", "claim_boundary", "gate_claim",
                "official_gate_pass_claimed", "official_p34_overall_pass", "official_g4_6_gate_pass",
                "official_g4_7_gate_pass", "g5_authorization") if key in data}
            if name == "G4-registry":
                record["recorded_decisions"] = [decision for decision in data.get("decisions", [])
                                                if decision.get("gate_id") == "g4-completion"]
            revision = data.get("result_revision")
            if revision:
                known = git(root, "cat-file", "-e", revision)
                record["revision_is_ancestor_of_head"] = None if known.returncode else (
                    git(root, "merge-base", "--is-ancestor", revision, "HEAD").returncode == 0)
            record["interpretation"] = "Historical local record; not a substitute for exact current independent acceptance."
        records.append(record)
    refs = [{"owner": owner, "path": relative, "present": (root / relative).is_file(),
             "sha256": sha256(root / relative) if (root / relative).is_file() else None}
            for owner, relative in REFERENCE_PATHS.items()]
    inventory = []
    for directory in sorted((root / ".aegis/results").glob("FC*")):
        if not directory.is_dir():
            continue
        for path in sorted(directory.glob("*.json")):
            if any(token in path.name for token in ("result", "control-review", "terminal")):
                value = json.loads(path.read_text())
                inventory.append({"path": path.relative_to(root).as_posix(), "sha256": sha256(path),
                                  "status": {key: value[key] for key in ("status", "verdict", "terminal_state", "result_revision", "gate_claim") if key in value}})
    by_id = {r["id"]: r for r in records}
    obligations = []
    mapping = {"G4": "G4-registry", "G4.5": "G4.5-final-vector", "G4.6": "G4.6-C",
               "G4.7": "G4.7", "PX0-07-original-P34": "PX0-07",
               "FC-01-independent-DEV": "FC-01-DEV", "FC-03-independent-DEV": "FC-03-DEV",
               "FC-05-independent-DEV": "FC-05-DEV"}
    for item in REQUIRED:
        record = by_id.get(mapping.get(item))
        status = "NOT_LOCATED_IN_INSPECTED_SOURCES"
        if record and record["present"]:
            status = "INDEPENDENT_ACCEPTANCE_NOT_BOUND" if item.startswith("FC-") else "RECORDED_PENDING_OR_BLOCKED"
            if item == "G4" and record.get("recorded_decisions"):
                status = "RECORDED_HISTORICAL_FUNCTIONAL_PASS_WITH_FINDINGS"
        obligations.append({"id": item, "status": status, "local_record": record["path"] if record else None})
    return {"schema_version": "g5-preparation-v1", "g5_admission": "BLOCKED_UNVERIFIED_PREREQUISITES",
            "official_gate_pass_claimed": False,
            "scope": "User-authorized preparation only; no G5 optimization, authority refreeze or acceptance mutation.",
            "required_acceptance_bindings": obligations,
            "local_records": records, "fc_result_inventory": inventory, "reference_paths": refs,
            "missing_reference_paths": [r["path"] for r in refs if not r["present"]],
            "limitations": ["Inspected local records are not an exhaustive search of independent control decisions.",
                            "Source presence and ancestry do not establish acceptance or output parity.",
                            "FC policy v0.4 supersedes the older policy linked by the G5 v0.3 roadmap."]}


def summarize(samples) -> dict:
    if any(isinstance(x, bool) or not isinstance(x, (int, float)) or not math.isfinite(x) or x < 0 for x in samples):
        raise ValueError("timings must be finite nonnegative numbers")
    values = sorted(samples)
    def percentile(p):
        return values[max(0, math.ceil(p * len(values)) - 1)] if values else None
    return {"count": len(values), "p50": percentile(.5), "p95": percentile(.95), "p99": percentile(.99),
            "max": max(values) if values else None, "mean": sum(values) / len(values) if values else None}


def measurement_status(before, after, measurement) -> str:
    if before["tree_sha256"] != after["tree_sha256"]:
        return "SOURCE_CHANGED"
    if (measurement.get("classification") != "MEASURED" or not measurement.get("artifact_binding")
            or not measurement.get("checks") or not all(measurement["checks"].values())):
        return "BLOCKED_EVIDENCE"
    return "MEASURED"


def valid_component_run(run) -> bool:
    try:
        expected = math.ceil(run["samples"] / run["batch_size"])
        summarize(run["append_ms"])
        summarize([run["seal_ms"]])
        return (run["confirmed_samples"] == run["samples"] and len(run["append_ms"]) == expected
                and bool(run["seal_digest"]) and run["seal_digest"] == run["replay_digest"])
    except (KeyError, ValueError, TypeError, ZeroDivisionError):
        return False


def write(path: Path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


def prepare_output(output: Path) -> Path:
    """Move prior result files aside; never let a retry consume stale evidence."""
    archive = output / "previous"
    archive.mkdir(parents=True, exist_ok=True)
    names = ["browser.json", "browser-after.png", "result.json", "component.json", "manifest.json",
             "source-before.json", "source-built.json", "source-after.json", "sdk-artifacts.json",
             "build-environment.json", "skia-manifest.json", "configure.log", "build.log", "browser.log",
             "run-errors.log"]
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) + f"-{time.time_ns()}"
    destination = archive / stamp
    moved = False
    for name in names:
        source = output / name
        if source.is_file():
            destination.mkdir(parents=True, exist_ok=True)
            shutil.move(str(source), str(destination / name))
            moved = True
    return destination if moved else archive


def run_logged(command, output, root=ROOT):
    with output.open("w") as log:
        log.write(json.dumps([str(x) for x in command]) + "\n")
        log.flush()
        result = subprocess.run([str(x) for x in command], cwd=root, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}); see {output}")


def run_web(args):
    from tools.setup_build_environment import setup_environment
    from tools.semantic.cmake_consumer import consumer_cmake_arguments
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    prepare_output(out)
    before = source_identity(ROOT)
    write(out / "source-before.json", before)
    result = {"classification": "BLOCKED_EVIDENCE", "official_gate_pass_claimed": False,
              "artifact_binding": None, "checks": {}, "host": platform.platform(), "started_unix": time.time()}
    try:
        skia = skia_identity(args.skia, json.loads((ROOT / "r1-full-skia-sdk.lock.json").read_text()), "web-wasm-webgl2")
        skia["lock_sha256"] = sha256(ROOT / "r1-full-skia-sdk.lock.json")
        write(out / "skia-manifest.json", skia)
        facts = setup_environment(core=False, semantic=True, target="web-wasm32", offline=True)
        write(out / "build-environment.json", facts)
        env = facts["environment"]
        build = out / "build"
        command = ["cmake", "-S", ROOT, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                   f"-DCMAKE_TOOLCHAIN_FILE={args.web_toolchain.resolve()}", "-DCANVAS_BUILD_POC01=OFF",
                   "-DCANVAS_BUILD_RF01=ON", "-DCANVAS_BUILD_RENDER=ON", "-DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON",
                   "-DCANVAS_RENDER_ENABLE_SKIA_HEADLESS=ON", "-DBUILD_TESTING=OFF",
                   "-DAXIOM_BUILD_RUNTIME_QUALIFICATION=OFF", "-DAXIOM_BUILD_JS_SDK=ON",
                   "-DAXIOM_INSTALL_DEVELOPER_COMPONENTS=OFF", "-DCANVAS_SKIA_SDK_VARIANT=release",
                   f"-DCANVAS_SKIA_SDK_ROOT={args.skia.resolve()}",
                   *consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(env["AXIOM_PROTOC"]))]
        run_logged(command, out / "configure.log")
        run_logged(["cmake", "--build", build, "--target", "axiom_js_runtime", "-j", str(args.jobs)], out / "build.log")
        built = source_identity(ROOT)
        write(out / "source-built.json", built)
        if before["tree_sha256"] != built["tree_sha256"]:
            raise RuntimeError("SOURCE_CHANGED during build: rerun on a stable source tree")
        sdk = build / "sdk/web"
        hashes = {name: sha256(sdk / name) for name in SDK_FILES}
        write(out / "sdk-artifacts.json", hashes)
        run_logged(["node", ROOT / "verification/runners/g5/browser_runner.mjs", sdk, args.browser,
                    out / "browser.json", "--renderer", args.renderer, "--samples", args.samples,
                    "--repeats", str(args.repeats), "--profile", args.profile, "--accumulate", str(args.accumulate)], out / "browser.log")
        browser = json.loads((out / "browser.json").read_text())
        result.update(classification=browser["classification"], checks=browser["checks"],
                      artifact_binding=hashlib.sha256(canonical(hashes)).hexdigest())
        if hashes != {name: sha256(sdk / name) for name in SDK_FILES} or hashes != browser["sdk_files"]:
            raise RuntimeError("SDK artifacts changed between build and browser execution")
        result["metrics"] = browser.get("summary")
        result["accumulation"] = browser.get("accumulation")
    except Exception as error:
        result["error"] = str(error)
        result["classification"] = "BLOCKED_EVIDENCE"
    after = source_identity(ROOT)
    write(out / "source-after.json", after)
    result["classification"] = measurement_status(before, after, result)
    result["source_tree_sha256"] = before["tree_sha256"]
    write(out / "result.json", result)
    files = {p.name: sha256(p) for p in sorted(out.iterdir()) if p.is_file() and p.name != "manifest.json"}
    write(out / "manifest.json", {"source_commit": before["commit"], "source_tree_sha256": before["tree_sha256"], "files": files})
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["classification"] == "MEASURED" else 2


def run_native(args):
    from tools.setup_build_environment import setup_environment
    from tools.semantic.cmake_consumer import consumer_cmake_arguments
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    prepare_output(out)
    before = source_identity(ROOT)
    write(out / "source-before.json", before)
    result = {"classification": "BLOCKED_EVIDENCE", "official_gate_pass_claimed": False,
              "artifact_binding": None, "checks": {}, "scope": "native BrushSession component only",
              "host": platform.platform()}
    try:
        # The component path can run on each native target, but must bind its
        # accepted SDK identity. The current local run is macOS/arm64.
        skia_manifest = json.loads((args.skia / "manifest.json").read_text())
        target = skia_manifest.get("identity", {}).get("target")
        skia = skia_identity(args.skia, json.loads((ROOT / "r1-full-skia-sdk.lock.json").read_text()), target)
        skia["lock_sha256"] = sha256(ROOT / "r1-full-skia-sdk.lock.json")
        write(out / "skia-manifest.json", skia)
        facts = setup_environment(core=False, semantic=True, target="native", offline=True)
        write(out / "build-environment.json", facts)
        env = facts["environment"]
        build = out / "build"
        run_logged(["cmake", "-S", ROOT / "verification/runners/g5/native", "-B", build, "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release", "-DCANVAS_SKIA_SDK_VARIANT=release",
                    f"-DCANVAS_SKIA_SDK_ROOT={args.skia.resolve()}",
                    *consumer_cmake_arguments(Path(env["AXIOM_SEMANTIC_RUNTIME_ROOT"]), Path(env["AXIOM_PROTOC"]))], out / "configure.log")
        run_logged(["cmake", "--build", build, "--target", "g5_brush_measurement", "-j", str(args.jobs)], out / "build.log")
        built = source_identity(ROOT)
        write(out / "source-built.json", built)
        if before["tree_sha256"] != built["tree_sha256"]:
            raise RuntimeError("SOURCE_CHANGED during native build")
        executable = build / ("g5_brush_measurement.exe" if sys.platform == "win32" else "g5_brush_measurement")
        binary_hash = sha256(executable)
        with (out / "component.json").open("w") as target, (out / "run-errors.log").open("w") as errors:
            run = subprocess.run([str(executable), args.samples, str(args.repeats), args.batch_sizes, args.profiles], stdout=target, stderr=errors)
        if run.returncode:
            raise RuntimeError("native measurement failed; see run-errors.log")
        raw = json.loads((out / "component.json").read_text())
        result["checks"] = {"all_runs_valid": bool(raw["runs"]) and all(valid_component_run(r) for r in raw["warmup"] + raw["runs"]),
                            "executable_unchanged": sha256(executable) == binary_hash}
        groups = {}
        for run in raw["runs"]:
            groups.setdefault((run["profile"], run["samples"], run["batch_size"]), []).append(run)
        result["summary"] = [{"profile": key[0], "samples": key[1], "batch_size": key[2], "repeats": len(runs),
                              "append_ms": summarize([x for r in runs for x in r["append_ms"]]),
                              "seal_ms": summarize([r["seal_ms"] for r in runs]),
                              "replay_ms": summarize([r["replay_ms"] for r in runs]),
                              "max_copied_historical_samples": max(r["metrics"]["max_copied_historical_samples"] for r in runs)}
                             for key, runs in groups.items()]
        result.update(classification="MEASURED", artifact_binding=binary_hash)
    except Exception as error:
        result["error"] = str(error)
    after = source_identity(ROOT)
    write(out / "source-after.json", after)
    result["classification"] = measurement_status(before, after, result)
    result["source_tree_sha256"] = before["tree_sha256"]
    write(out / "result.json", result)
    write(out / "manifest.json", {"source_commit": before["commit"], "source_tree_sha256": before["tree_sha256"],
          "files": {p.name: sha256(p) for p in sorted(out.iterdir()) if p.is_file() and p.name != "manifest.json"}})
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["classification"] == "MEASURED" else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    audit_parser = sub.add_parser("audit")
    audit_parser.add_argument("--output", type=Path, required=True)
    web = sub.add_parser("web")
    web.add_argument("--output", type=Path, required=True)
    web.add_argument("--web-toolchain", type=Path, required=True)
    web.add_argument("--skia", type=Path, required=True)
    web.add_argument("--browser", type=Path, required=True)
    web.add_argument("--renderer", choices=["default", "swiftshader"], default="default")
    web.add_argument("--samples", default="64,256,1024,4096")
    web.add_argument("--repeats", type=int, default=5)
    web.add_argument("--jobs", type=int, default=4)
    web.add_argument("--profile", default="round-handdrawn")
    web.add_argument("--accumulate", type=int, default=0)
    native = sub.add_parser("native", help="isolated native BrushSession attribution; not SDK/device evidence")
    native.add_argument("--output", type=Path, required=True)
    native.add_argument("--skia", type=Path, required=True)
    native.add_argument("--samples", default="64,256,1024,4096")
    native.add_argument("--repeats", type=int, default=5)
    native.add_argument("--batch-sizes", default="1,8,64")
    native.add_argument("--profiles", default="round-handdrawn")
    native.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.command == "audit":
        report = audit(ROOT)
        report["source"] = source_identity(ROOT)
        write(args.output, report)
        print(json.dumps({"g5_admission": report["g5_admission"], "output": str(args.output)}))
        return 0  # audit execution success is separate from G5 admission
    return run_web(args) if args.command == "web" else run_native(args)


if __name__ == "__main__":
    raise SystemExit(main())
