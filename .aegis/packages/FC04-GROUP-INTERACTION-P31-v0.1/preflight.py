import argparse, hashlib, json, subprocess, sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument("--repo",required=True);args=p.parse_args()
root=Path(args.repo).resolve();here=Path(__file__).resolve().parent
pkg=json.loads((here/"package.json").read_text(encoding="utf-8"));lock=json.loads((here/"source-lock.json").read_text(encoding="utf-8"))
remote=subprocess.check_output(["git","remote","get-url","origin"],cwd=root,text=True).strip()
if remote.removesuffix(".git").lower()!="https://github.com/mostorm-labs/axiom":raise SystemExit("BLOCKED_REPOSITORY_IDENTITY: origin mismatch")
if subprocess.call(["git","merge-base","--is-ancestor",pkg["source_revision"],"HEAD"],cwd=root)!=0:raise SystemExit("BLOCKED_EXECUTION_DIVERGENCE: anchor not ancestor")
for row in lock["sources"]:
 path=root/row["path"]
 if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=row["sha256"]:raise SystemExit("BLOCKED_SOURCE_BINDING: "+row["path"])
 blob=subprocess.check_output(["git","rev-parse",pkg["source_revision"]+":"+row["path"]],cwd=root,text=True).strip()
 if blob!=row["git_blob"]:raise SystemExit("BLOCKED_SOURCE_BINDING: git blob mismatch "+row["path"])
for name,key in [("source-lock.json","source_lock_sha256"),("verification-candidate.json","verification_candidate_sha256"),("owner-bindings.md","owner_bindings_sha256"),("oracle-candidate.json","oracle_candidate_sha256")]:
 if hashlib.sha256((here/name).read_bytes()).hexdigest()!=pkg[key]:raise SystemExit("BLOCKED_PACKAGE_INTEGRITY: "+name)
print("PASS candidate repository, anchor, source and candidate integrity")
print("BLOCKED P32 readiness:")
for issue in pkg["blockers"]:print("- "+issue)
sys.exit(2)
