#!/usr/bin/env python3
"""Validate candidate bytes and fail closed on absent SDK-01 execution authority."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--check", required=True, choices=(
        "integrity", "PackageBindingPreflight", "EvidenceContractPreflight",
        "DesignPreflight", "all"))
    args = parser.parse_args()
    root = args.repo.resolve()
    package_dir = root / ".aegis/packages/AXIOM-SDK-01-v0.2"
    failures = []

    def git(*argv):
        return subprocess.run(["git", "-C", str(root), *argv], capture_output=True, check=False)

    try:
        remote = git("remote", "get-url", "origin")
        url = remote.stdout.decode().strip().lower().removesuffix(".git")
        if remote.returncode or url not in ("https://github.com/mostorm-labs/axiom",
                                             "git@github.com:mostorm-labs/axiom"):
            raise ValueError("repository origin does not match Mostorm-Labs/axiom")
        entry = json.loads((package_dir / "package.json").read_text(encoding="utf-8"))
        if entry["repository"] != {"provider": "github", "full_name": "Mostorm-Labs/axiom"}:
            raise ValueError("package repository identity mismatch")
        anchor = entry["task_anchor"]["revision"]
        if entry["task_anchor"]["relation"] != "ancestor" or git("merge-base", "--is-ancestor", anchor, "HEAD").returncode:
            raise ValueError("task anchor is not an ancestor of current HEAD")
        for key in ("authority_lock_ref", "verification_lock_ref", "execution_contract_ref",
                    "implementation_context_ref", "evidence_contract_ref", "preflight_ref"):
            ref = entry[key]
            path = (package_dir / ref["path"]).resolve()
            if not path.is_relative_to(package_dir.resolve()) or not path.is_file() or sha(path) != ref["sha256"]:
                raise ValueError(f"package ref mismatch: {key}")
        for ref in entry["review_documents"]:
            path = (root / ref["path"]).resolve()
            if not path.is_relative_to(root) or not path.is_file() or sha(path) != ref["sha256"]:
                raise ValueError(f"review document mismatch: {ref['path']}")
            blob = git("show", ref["revision"] + ":" + ref["path"])
            if blob.returncode or hashlib.sha256(blob.stdout).hexdigest() != ref["sha256"]:
                raise ValueError(f"review revision blob mismatch: {ref['path']}")
        authority = json.loads((package_dir / "authority.lock.json").read_text(encoding="utf-8"))
        verification = json.loads((package_dir / "verification.lock.json").read_text(encoding="utf-8"))
        for ref in [entry["supersedes_candidate"], verification["case_binding"], *verification["actual_evidence"]]:
            path = (root / ref["path"]).resolve()
            if not path.is_relative_to(root) or not path.is_file() or sha(path) != ref["sha256"]:
                raise ValueError(f"evidence or predecessor mismatch: {ref['path']}")
            blob = git("show", ref["revision"] + ":" + ref["path"])
            if blob.returncode or hashlib.sha256(blob.stdout).hexdigest() != ref["sha256"]:
                raise ValueError(f"evidence or predecessor revision mismatch: {ref['path']}")
        for item in authority["sources"] + authority["source_facts"] + [authority["accepted_user_decision"]]:
            path = (root / item["path"]).resolve()
            if not path.is_relative_to(root) or not path.is_file() or sha(path) != item["sha256"]:
                raise ValueError(f"source/worktree hash mismatch: {item['path']}")
            blob = git("show", item["revision"] + ":" + item["path"])
            if blob.returncode or hashlib.sha256(blob.stdout).hexdigest() != item["sha256"]:
                raise ValueError(f"locked revision blob mismatch: {item['path']}")
        print("INTEGRITY_OK: identity, anchor, package refs, review docs and locked source bytes")
    except (OSError, KeyError, ValueError, json.JSONDecodeError) as exc:
        print(f"BLOCKED_INTEGRITY: {exc}")
        return 2

    if args.check == "integrity":
        print("DRAFT_ONLY: integrity does not establish Authority, Verification or P32 approval")
        return 0
    if args.check in ("PackageBindingPreflight", "all"):
        if not entry.get("p32_authorized"):
            failures.append("PackageBindingPreflight: P32 authorization and exact accepted package absent")
        if authority.get("unresolved"):
            failures.append("PackageBindingPreflight: public ABI/target/threading and multi-selection Authority unresolved")
    if args.check in ("EvidenceContractPreflight", "all"):
        if verification.get("p20_blockers"):
            failures.append("EvidenceContractPreflight: public multi-selection and Windows/Web drivers/oracles not materialized or accepted")
    if args.check in ("DesignPreflight", "all"):
        if authority.get("unresolved"):
            failures.append("DesignPreflight: public types, handles and lifecycle require accepted decisions")
    for failure in failures:
        print("BLOCKED: " + failure)
    if failures:
        return 2
    print("PREFLIGHT_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
