"""Check bounded Section A source scope and immutable frozen inputs."""
import argparse
import fnmatch
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GIT = "D:/Softwares/Git/cmd/git.exe"
PKG = ".aegis/packages/FC-08-SECTION-A-v0.1"

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    assert Path(args.repo).resolve() == ROOT
    def git(*argv):
        return subprocess.check_output([GIT, *argv], cwd=ROOT).decode().strip()
    package = json.loads((ROOT/PKG/"package.json").read_text())
    contract = json.loads((ROOT/PKG/"execution-contract.json").read_text())
    amendment_path = PKG+"/authorization-scope-amendment-001.json"
    amendment = json.loads((ROOT/amendment_path).read_text())
    assert amendment["parent_package_sha256"] == digest((ROOT/PKG/"package.json").read_bytes())
    assert git("show", amendment["parent_package_revision"]+":"+PKG+"/package.json") == (ROOT/PKG/"package.json").read_text().strip()
    base = package["source_revision"]
    subprocess.run([GIT,"merge-base","--is-ancestor",base,"HEAD"],cwd=ROOT,check=True)
    changed = sorted(set(git("diff","--name-only",base).splitlines() + git("ls-files","--others","--exclude-standard").splitlines()))
    allowed = contract["allowed_files"]+amendment["added_allowed_files"]
    errors = ["Outside authorized source scope: "+s for s in changed if not any(fnmatch.fnmatchcase(s,p) for p in allowed)]
    artifact_refs=[]
    corpus_path = ".aegis/materialized/FC-08/attempts/v0.3-control-remediation-02/corpus-manifest.json"
    corpus = json.loads((ROOT/corpus_path).read_text())
    for row in corpus["variants"]:
        for item in [row["input"],row["expected"],*row.get("wire",[])]:
            if "path" not in item:
                artifact_refs.append({**item,"verification":"RECIPE_OUTPUT_NOT_MATERIALIZED_IN_A"})
                continue
            actual=digest((ROOT/item["path"]).read_bytes())
            if actual != item["sha256"]:errors.append("Frozen hash mismatch: "+item["path"])
            artifact_refs.append({"path":item["path"],"sha256":actual})
    for name in ["descriptor.lock.pb","descriptor.lock.sha256"]:
        path="schema/axiom/v1/descriptor/"+name
        old=subprocess.check_output([GIT,"show",base+":"+path],cwd=ROOT)
        if old != (ROOT/path).read_bytes():errors.append("Historical descriptor changed: "+path)
    historical=json.loads((ROOT/".aegis/packages/FC-08-PRODUCT-P31-v0.1/package.json").read_text())
    # The original P31 flag is immutable; this new package is the only A authorization.
    if historical.get("P32_authorized") is not False or historical.get("may_start_code_execution") is not False:
        errors.append("Historical authorization changed")
    report={"status":"PASS_SLICE_A_SCOPE_AND_PRESERVATION" if not errors else "BLOCKED",
        "source_base":base,"source_head":git("rev-parse","HEAD"),
        "execution_ref":git("symbolic-ref","HEAD"),"package_sha256":digest((ROOT/PKG/"package.json").read_bytes()),
        "scope_amendment":{"path":amendment_path,"sha256":digest((ROOT/amendment_path).read_bytes())},
        "changed_paths":changed,"changed_file_hashes":[{"path":s,"sha256":digest((ROOT/s).read_bytes())} for s in changed if (ROOT/s).is_file() and not s.startswith(".aegis/results/")],
        "frozen_logical_cases":len({v["case_id"] for v in corpus["variants"]}),
        "frozen_variants":len(corpus["variants"]),"frozen_artifacts":artifact_refs,
        "errors":errors,"B_E_status":"NOT_RUN"}
    out=ROOT/args.output
    out.parent.mkdir(parents=True,exist_ok=True)
    with out.open("x",encoding="utf-8",newline="\n") as stream:stream.write(json.dumps(report,indent=2)+"\n")
    print(json.dumps({"status":report["status"],"errors":errors,"changed_paths":len(changed),"frozen_variants":len(corpus["variants"])}))
    return int(bool(errors))

if __name__ == "__main__":
    raise SystemExit(main())
