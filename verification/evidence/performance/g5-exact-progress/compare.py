"""Compare the unchanged workload against the committed cold/warm baseline.

Usage: compare.py <baseline-evidence> <new-diagnostic> <new-default> <output>
New folders can be the original JSON output or the compressed evidence copy.
"""
import gzip
import json
import math
from pathlib import Path
import sys


def read(folder, name):
    path = folder / name
    return json.loads(path.read_text() if path.exists() else gzip.decompress(
        path.with_suffix(path.suffix + ".gz").read_bytes()))


def stats(values):
    values = sorted(values)
    assert values and all(math.isfinite(v) and v >= 0 for v in values)
    return {"min": min(values), "p50": values[(len(values)-1)//2], "max": max(values)}


baseline, diagnostic, default, output = map(Path, sys.argv[1:])
groups, pixel_checks = [], []
before_summary = read(baseline / "diagnostic", "summary.json")
after_summary = read(diagnostic, "summary.json")
assert after_summary["status"] == "MEASURED"
for workload in ("long-line", "image", "table", "sticky"):
    for selected in (False, True):
        group = dict(workload=workload, selected=selected)
        records = []
        for repeat in range(3):
            name = f"r{repeat}-{workload}-{'selected' if selected else 'unselected'}.json"
            old = read(baseline / "default", name)
            new = read(default, name)
            trace = read(diagnostic, name)
            assert old["status"] == new["status"] == trace["status"] == "MEASURED"
            for point in ("initial", "cold", "warm"):
                a = old["setup"]["initialPixels"] if point == "initial" else old["raw"][point]["pixels"]
                b = new["setup"]["initialPixels"] if point == "initial" else new["raw"][point]["pixels"]
                matched = a == b
                pixel_checks.append(dict(id=new["id"], point=point, matched=matched,
                                         before_sha256=a["sha256"], after_sha256=b["sha256"]))
                assert matched, (name, point)
            records.append((old, new, trace, read(baseline / "diagnostic", name)))
        for phase in ("cold", "warm"):
            before = stats([r[0]["summary"][phase]["settle_ms"] for r in records])
            after = stats([r[1]["summary"][phase]["settle_ms"] for r in records])
            attempts_before = stats([r[3]["summary"][phase]["exactAttempts"] for r in records])
            attempts_after = stats([r[2]["summary"][phase]["exactAttempts"] for r in records])
            assert attempts_after["max"] < attempts_before["min"], "no deterministic attempt reduction"
            group[phase] = dict(before_receipt_ms=before, after_receipt_ms=after,
                                median_reduction_percent=100*(1-after["p50"]/before["p50"]),
                                before_attempts=attempts_before, after_attempts=attempts_after)
        groups.append(group)
result = dict(classification="MEASURED", official_gate_pass_claimed=False,
              baseline_source_commit=before_summary["sourceCommit"],
              baseline_source_tree_sha256=before_summary["sourceTreeSha256"],
              result_source_commit=after_summary["sourceCommit"],
              result_source_tree_sha256=after_summary["sourceTreeSha256"],
              baseline_after_pixel_checks=len(pixel_checks), all_pixels_match=True,
              workload_groups=groups, pixel_checks=pixel_checks,
              limits=["Private checkpoint ceiling changed; 8ms cooperative allowance and 100ms idle gate preserved.",
                      "Same corpus and environment, separate sequential runs; timings are engineering evidence, not SLOs.",
                      "No GPU execution or physical display latency is measured."])
output.mkdir(parents=True, exist_ok=True)
(output / "before-after.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(dict(groups=len(groups), pixel_checks=len(pixel_checks), all_pixels_match=True)))
