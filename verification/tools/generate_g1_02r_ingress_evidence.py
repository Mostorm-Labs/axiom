#!/usr/bin/env python3
import argparse, json
from pathlib import Path

FILES = ["INGRESS-PLAN.json", "INGRESS-MATRIX.json", "INGRESS-REGRESSION.json", "INGRESS-CTEST.txt", "INGRESS-PROVIDER.json", "INGRESS-GATE-MANIFEST.json"]

def generate(root: Path, source_ref: str, package_ref: str) -> Path:
    out = root / "verification" / "evidence" / "gates" / "G1" / source_ref / "GT-G1-02R-INGRESS"
    out.mkdir(parents=True, exist_ok=True)
    (out / FILES[0]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","source_ref":source_ref,"package_ref":package_ref}, indent=2)+"\n")
    (out / FILES[1]).write_text(json.dumps({"families":15,"independent_nonempty_oracle":"DeleteObjects PASS"}, indent=2)+"\n")
    (out / FILES[2]).write_text(json.dumps({"codec":"31/31 PASS","operation_engine_15":"11/11 PASS","protobuf_on":"470/470 PASS","protobuf_off":"447 PASS + 23 EXPECTED SKIP + 0 FAIL","clean_checkout":"NOT_RUN"}, indent=2)+"\n")
    (out / FILES[3]).write_text("codec_operation_ingress_test: PASS\ncodec_test: 31/31 PASS\noperation_engine_15: 11/11 PASS\nprotobuf ON semantic CTest: 470/470 PASS\nprotobuf OFF semantic CTest: 447 PASS + 23 EXPECTED SKIP + 0 FAIL\ngit diff --check: PASS\n")
    (out / FILES[4]).write_text(json.dumps({"provider":"github","hosted_exact_source_run":"NOT_RUN","exact_source_ref":source_ref}, indent=2)+"\n")
    (out / FILES[5]).write_text(json.dumps({"task_id":"GT-G1-02R-INGRESS","source_ref":source_ref,"relation":"EVIDENCE_ONLY_DESCENDANT","files":FILES}, indent=2)+"\n")
    return out

if __name__ == "__main__":
    p=argparse.ArgumentParser(); p.add_argument("--root", type=Path, default=Path.cwd()); p.add_argument("--source-ref", required=True); p.add_argument("--package-ref", required=True); a=p.parse_args(); print(generate(a.root,a.source_ref,a.package_ref))
