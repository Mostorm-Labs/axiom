"""Summarize prepared geometry work, using actual per-case diagnostic records.

Usage: analyze.py <exact-progress-evidence> <diagnostic> <default> <vector-on> <vector-off> <output>
Accepts plain JSON or the evidence directory's .json.gz files.
"""
import gzip
import json
import math
from pathlib import Path
import sys


def read(folder, name):
    p = folder / name
    return json.loads(p.read_text() if p.exists() else gzip.decompress(p.with_suffix(p.suffix + ".gz").read_bytes()))


def stats(values):
    values = sorted(values)
    assert values and all(math.isfinite(v) and v >= 0 for v in values)
    return dict(min=min(values), p50=values[(len(values)-1)//2], max=max(values))


baseline, diagnostic, default, vector_on, vector_off, output = map(Path, sys.argv[1:])
groups = []
for workload in ("long-line", "image", "table", "sticky"):
    for selected in (False, True):
        label = "selected" if selected else "unselected"
        before = [read(baseline / "diagnostic", f"r{i}-{workload}-{label}.json") for i in range(3)]
        after = [read(diagnostic, f"r{i}-{workload}-{label}.json") for i in range(3)]
        ordinary = [read(default, f"r{i}-{workload}-{label}.json") for i in range(3)]
        group = dict(workload=workload, selected=selected)
        for phase in ("cold", "warm"):
            prepared = [r["summary"][phase]["navigation"]["prepared"] for r in after]
            assert all(p["builds"] == (2 if phase == "cold" else 0) and p["denied"] == 0 for p in prepared)
            assert all(p["peakResidentBytes"] > 0 and p["contourDrawn"] > 0 for p in prepared)
            group[phase] = dict(
                navigation_before_cpu_ms=stats([r["summary"][phase]["navigation"]["cpu_ms"]["total"] for r in before]),
                navigation_after_cpu_ms=stats([r["summary"][phase]["navigation"]["cpu_ms"]["total"] for r in after]),
                default_receipt_ms=stats([r["summary"][phase]["settle_ms"] for r in ordinary]),
                prepared={key:stats([p[key] for p in prepared]) for key in prepared[0]},
                tiles=stats([r["summary"][phase]["navigation"]["built"] for r in after]),
            )
        groups.append(group)
vector_checks = []
for selected in (False, True):
    name = f"r0-long-line-{'selected' if selected else 'unselected'}.json"
    a, b = read(vector_on, name), read(vector_off, name)
    assert a["status"] == b["status"] == "MEASURED"
    assert a["setup"]["profile"] == b["setup"]["profile"] == "vector-solid-v1"
    assert a["setup"]["revision"] == b["setup"]["revision"] == 1
    for point in ("initial", "cold", "warm"):
        first = a["setup"]["initialPixels"] if point == "initial" else a["raw"][point]["pixels"]
        second = b["setup"]["initialPixels"] if point == "initial" else b["raw"][point]["pixels"]
        assert first == second
        vector_checks.append(dict(id=a["id"], point=point, matched=True, sha256=first["sha256"]))
    for record in (a, b):
        assert all(all(record["raw"][phase]["checks"].values()) for phase in ("cold", "warm"))
    for phase in ("cold", "warm"):
        p = a["summary"][phase]["navigation"]["prepared"]
        assert p["vectorUses"] > 0 and p["denied"] == 0
        assert p["builds"] == (2 if phase == "cold" else 0)
for folder in (diagnostic, default, vector_on, vector_off):
    summary = read(folder, "summary.json")
    assert summary["status"] == "MEASURED" and summary["sourceStable"] and summary["artifactStable"]
assert len({read(folder, "summary.json")["sourceTreeSha256"] for folder in (diagnostic, default, vector_on, vector_off)}) == 1
result = dict(classification="MEASURED_WITH_BASELINE_FINDINGS", official_gate_pass_claimed=False,
              source_commit=read(diagnostic, "summary.json")["sourceCommit"],
              source_tree_sha256=read(diagnostic, "summary.json")["sourceTreeSha256"],
              groups=groups, vector_pixel_checks=vector_checks, vector_scope="one repetition, two selection states, cold/warm, timing ON/OFF; functional prepared-path evidence",
              notes=["Prepared memory is modeled retained source/path storage, not process RSS or hidden GPU driver residency.",
                     "Knot-step counters retain their old logical-progress meaning for cached v4; use primitive counters for local draw work.",
                     "Sequential CPU timings are observations; no universal speedup, physical latency, GPU execution or SLO claim.",
                     "Warm still builds 16 tiles. Scene/reference payload copying and stable chunk indexing remain separate work."])
output.mkdir(parents=True, exist_ok=True)
(output / "geometry-analysis.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(dict(groups=len(groups), vector_pixel_checks=len(vector_checks), source_stable=True)))
