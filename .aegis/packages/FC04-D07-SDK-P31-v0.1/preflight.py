#!/usr/bin/env python3
"""Control-only P31 binding checks; does not build or modify product code."""
from pathlib import Path
import argparse, json, subprocess, sys

P = Path(__file__).resolve().parent
ROOT = P.parents[2]
ANCHOR = "3d15645d54d44c47474b4b0dd6ca20ade1e3c633"
FEATURE = "c7f874395d55952e9d5288e2a336a21747f37f11"
PACKAGE_REL = ".aegis/packages/FC04-D07-SDK-P31-v0.1"

def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()

def read(path):
    return json.loads(path.read_text(encoding="utf-8"))

def require(ok, message):
    if not ok:
        raise AssertionError(message)

def verify_blob(revision, path, expected):
    actual = git("rev-parse", f"{revision}:{path}")
    require(actual == expected, f"blob mismatch at {revision}:{path}: {actual} != {expected}")

def package_binding():
    package = read(P / "package.json")
    require(git("branch", "--show-current") == "codex/fc04-d07-selection-audit", "wrong execution branch")
    remote = git("remote", "get-url", "origin").lower().removesuffix(".git")
    require(remote in ("https://github.com/mostorm-labs/axiom", "git@github.com:mostorm-labs/axiom"), "wrong repository: " + remote)
    head = git("rev-parse", "HEAD")
    git("merge-base", "--is-ancestor", ANCHOR, head)
    git("merge-base", "--is-ancestor", FEATURE, head)
    require(package["task_anchor"]["revision"] == ANCHOR, "task anchor mismatch")
    require(package["feature_anchor"]["revision"] == FEATURE, "feature anchor mismatch")
    require(package["sdk_release_promotion_authorized"] is False, "release promotion must remain unauthorized")
    require(package["semantic_sdk_lock_update_authorized"] is False, "SDK lock update must remain unauthorized")
    for item in read(P / "source-lock.json")["files"]:
        verify_blob(ANCHOR, item["path"], item["git_blob_sha"])
    for item in read(P / "input-lock.json")["historical_inputs"]:
        verify_blob(ANCHOR, item["path"], item["git_blob_sha"])
    for item in read(P / "integrity.json")["files"]:
        verify_blob(head, PACKAGE_REL + "/" + item["path"], item["git_blob_sha"])
    changed = git("diff", "--name-only", f"{ANCHOR}..{head}").splitlines()
    require(all(path.startswith(PACKAGE_REL + "/") for path in changed), "non-package changes in P31 materialization: " + repr(changed))
    print("PASS: repository, branch, anchors, source/input locks and package integrity; HEAD=" + head)

def evidence_contract():
    decision = read(ROOT / ".aegis/results/FC04-D07-ROUTE-A-INVESTIGATION/raw/decision.json")
    require(decision["decision"] == "SDK_EXCEPTION_SAFETY_FIX_REQUIRES_UPSTREAM_RELEASE", "Route A decision mismatch")
    matrix = read(P / "test-matrix.json")
    ids = [item["id"] for item in matrix["required_cases"]]
    require(len(ids) == len(set(ids)) and len(ids) >= 12, "test matrix IDs invalid")
    required = {"ONCE-OOM-RETURN","ONCE-WAITER-2","PROTO-PARTIAL-STATE","PROTO-RETRY-BYTES","FULL-SWEEP","PATCH-IDENTITY"}
    require(required <= set(ids), "required exception-safety cases missing")
    report = (ROOT / ".aegis/results/FC04-D07-ROUTE-A-INVESTIGATION/report.md").read_text(encoding="utf-8")
    require("SDK_EXCEPTION_SAFETY_FIX_REQUIRES_UPSTREAM_RELEASE" in report, "route report mismatch")
    print("PASS: Route A decision and exception-safety matrix bound")

def design():
    ctx = read(P / "implementation-context.json")
    require(ctx["allowed_new_artifacts"] == ["tools/semantic/patches/patch-set.json","tools/semantic/patches/abseil-20250512.1/**","tools/semantic/patches/protobuf-v36.0/**"], "patch artifact scope mismatch")
    require("runtime/semantic/src/codec.cpp" in ctx["forbidden"] and "semantic-sdk.lock.json" in ctx["forbidden"], "forbidden scope missing")
    require(ctx["release_promotion_authorized"] is False and ctx["consumer_lock_update_authorized"] is False, "release boundary mismatch")
    text = (P / "design.md").read_text(encoding="utf-8").lower()
    for phrase in ("descriptortable::is_initialized", "no descriptor prewarming", "byte-identical", "waiters"):
        require(phrase in text, "design invariant missing: " + phrase)
    print("PASS: patch scope, invariants and release boundary bound")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-binding", action="store_true")
    parser.add_argument("--evidence-contract", action="store_true")
    parser.add_argument("--design", action="store_true")
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()
    selected = args.all or not any((args.package_binding,args.evidence_contract,args.design))
    try:
        if selected or args.package_binding: package_binding()
        if selected or args.evidence_contract: evidence_contract()
        if selected or args.design: design()
    except Exception as exc:
        print(f"BLOCKED: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 2
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
