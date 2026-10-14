"""P20 read-only validator for exact repository-resolvable Authority bindings."""
from __future__ import annotations
import argparse, hashlib, json, subprocess
from pathlib import Path

def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    args=parser.parse_args()
    root=Path(args.root).resolve()
    design=json.loads((root/".aegis/packages/FC-00X-RUNTIME-MIN-CLOSURE/p20-verification-design.json").read_text(encoding="utf-8"))
    observations=[]
    for binding in design["authority_reconciliation"]["bindings"]:
        path=root/binding["materialized_path"]
        if not path.is_file(): raise SystemExit(f"missing materialized authority: {binding['id']}: {path}")
        sha256=hashlib.sha256(path.read_bytes()).hexdigest()
        blob=subprocess.check_output(["git","hash-object",str(path)],text=True).strip()
        if sha256 != binding["materialized_sha256"]: raise SystemExit(f"sha256 mismatch: {binding['id']}")
        if blob != binding["materialized_git_blob_sha1"]: raise SystemExit(f"git blob mismatch: {binding['id']}")
        observations.append({"id":binding["id"],"path":binding["materialized_path"],"git_blob_sha1":blob,"sha256":sha256,"status":"RESOLVED"})
    print(json.dumps({"oracle":"FC00X-P20-AUTHORITY-BINDING-v1","bindings":observations},sort_keys=True))
    return 0
if __name__ == "__main__": raise SystemExit(main())
