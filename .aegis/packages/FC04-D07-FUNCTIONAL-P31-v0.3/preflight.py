#!/usr/bin/env python3
"""Functional D07 P31 v0.3 preflights. Read-only: never edits product source."""
import argparse, json, pathlib, subprocess, sys, traceback

P = pathlib.Path(__file__).resolve().parent
ROOT = P.parents[2]
BRANCH = "codex/fc04-d07-functional-p31-v0-3"
TASK_ANCHOR = "15e76458f3872be4d1b51d4aac05eb8402719014"
FEATURE_ANCHOR = "c7f874395d55952e9d5288e2a336a21747f37f11"
CURSOR = "b5072f2c7dd796b2ad7aed3a6bfa5d86b6058cad"

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()
def read(path):
    return json.loads(path.read_text(encoding="utf-8"))
def require(ok, message):
    if not ok: raise AssertionError(message)
def ancestor(a, b):
    subprocess.run(["git", "merge-base", "--is-ancestor", a, b], cwd=ROOT, check=True)
def blob(revision, path):
    return git("rev-parse", f"{revision}:{path}")
def verify_lock(lock_path, revision):
    data = read(lock_path)
    require(data["subject_revision"] == CURSOR, f"lock subject mismatch: {lock_path.name}")
    for item in data["files"] if "files" in data else data["historical_inputs"]:
        actual = blob(revision, item["path"])
        require(actual == item["git_blob_sha"], f"locked blob mismatch {revision}:{item['path']}: {actual} != {item['git_blob_sha']}")
def package_binding():
    package = read(P / "package.json")
    require(git("branch", "--show-current") == BRANCH, "wrong execution branch")
    remote = git("remote", "get-url", "origin").lower().removesuffix(".git")
    require(remote in ("https://github.com/mostorm-labs/axiom", "git@github.com:mostorm-labs/axiom"), "wrong origin: " + remote)
    head = git("rev-parse", "HEAD")
    for rev in (TASK_ANCHOR, FEATURE_ANCHOR, CURSOR):
        ancestor(rev, head)
    require(package["task_anchor"]["revision"] == TASK_ANCHOR, "task anchor mismatch")
    require(package["feature_anchor"]["revision"] == FEATURE_ANCHOR, "feature anchor mismatch")
    require(package["resume_cursor"]["revision"] == CURSOR, "resume cursor mismatch")
    verify_lock(P / "source-lock.json", CURSOR)
    verify_lock(P / "input-lock.json", CURSOR)
    integrity = read(P / "integrity.json")
    for item in integrity["files"]:
        require(blob(head, P.relative_to(ROOT).as_posix() + "/" + item["path"]) == item["git_blob_sha"], "package integrity mismatch: " + item["path"])
    require(package["p32_authorized_if_four_preflights_pass"] is True, "P32 authorization binding missing")
    require(package["overall_d07_pass_authorized"] is False and package["sdk_or_upstream_changes_authorized"] is False, "forbidden authorization bit")
    print("PASS PackageBindingPreflight: branch, origin, anchors, resume cursor, input/source locks and package integrity; HEAD=" + head)
def evidence_contract():
    verify_lock(P / "source-lock.json", CURSOR)
    verify_lock(P / "input-lock.json", CURSOR)
    matrix = read(P / "test-matrix.json")
    require(len((ROOT / matrix["frozen_regressions"]["d07_original_14"]).read_text().splitlines()) == 14, "frozen D07 list is not 14 IDs")
    require(len((ROOT / matrix["frozen_regressions"]["selection_160"]).read_text().splitlines()) == 160, "frozen Selection list is not 160 IDs")
    require(matrix["overall_blocker"]["status"] == "KNOWN_BLOCKING_OUTSIDE_THIS_FUNCTIONAL_PACKAGE", "OOM blocker not preserved")
    report = (ROOT / matrix["overall_blocker"]["evidence"][0]).read_text(encoding="utf-8")
    require("NEW_HIGH_IMPACT_FAILURE_MODE" in report and "TIMEOUT / failed" in report, "known OOM failure evidence not bound")
    require("fault-index-100-stack.log" in "\n".join(matrix["overall_blocker"]["evidence"]), "fault stack not bound")
    print("PASS EvidenceContractPreflight: exact regression list cardinalities and known OOM blocker retained")
def design():
    package = read(P / "package.json")
    context = read(P / "implementation-context.json")
    matrix = read(P / "test-matrix.json")
    closure = (P / "closure.md").read_text(encoding="utf-8")
    design_text = (P / "design.md").read_text(encoding="utf-8")
    require("one user-visible Undo unit" in closure and "all supported" in closure, "functional closure incomplete")
    require("No Protobuf/Abseil source edits" in closure, "upstream boundary missing")
    require("overall D07/release remains" in design_text, "OOM non-waiver missing")
    require("runtime/semantic/src/codec.cpp" in context["forbidden_paths"], "codec forbidden scope missing")
    require("semantic-sdk.lock.json" in context["forbidden_paths"], "SDK lock forbidden scope missing")
    require(matrix["terminal_statuses"]["overall"] == "BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG", "overall terminal status mismatch")
    print("PASS DesignPreflight: functional scope, source boundary and separate OOM terminal status bound")
def functional_scope():
    package = read(P / "package.json")
    context = read(P / "implementation-context.json")
    matrix = read(P / "test-matrix.json")
    required = {t["id"] for t in matrix["functional_required_tests"]}
    require(len(required) == len(matrix["functional_required_tests"]), "duplicate functional test IDs")
    require("SCENE-PREP-01" in required and "SCENE-PREP-06" in required, "Scene matrix incomplete")
    require("fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit" in required, "Intent positive missing")
    require("FC04D07PostcommitReceiptGrowthFault" in required and "FC04D07PostcommitFault" in required, "existing OOM protections missing")
    allow = set(context["allowed_modification_paths"])
    require("runtime/semantic/src/codec.cpp" not in allow, "codec accidentally allowed")
    require("semantic-sdk.lock.json" not in allow, "SDK lock accidentally allowed")
    require(package["overall_d07_pass_authorized"] is False, "overall D07 pass incorrectly authorized")
    print("PASS FunctionalScopePreflight: required functional matrix and forbidden upstream/SDK scope bound")
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--check", choices=["PackageBindingPreflight","EvidenceContractPreflight","DesignPreflight","FunctionalScopePreflight","all"], default="all")
    args=parser.parse_args()
    try:
        require(pathlib.Path(args.repo).resolve()==ROOT.resolve(), "--repo must identify this worktree root")
        checks={"PackageBindingPreflight":package_binding,"EvidenceContractPreflight":evidence_contract,"DesignPreflight":design,"FunctionalScopePreflight":functional_scope}
        for name in checks if args.check=="all" else [args.check]:
            checks[name]()
    except Exception:
        traceback.print_exc()
        sys.exit(2)
if __name__ == "__main__":
    main()
