"""Rebuild the aggregate and pixel checks from unmodified per-case measurements.

Usage: python3.12 analyze.py <diagnostic-dir> <default-control-dir> <output-dir>
Directories may contain plain JSON or deterministic gzip-compressed JSON.
"""
import collections
import gzip
import hashlib
import json
import math
from pathlib import Path
import sys


def read_bytes(folder, name):
    path = folder / name
    if path.exists():
        return path.read_bytes()
    return gzip.decompress(path.with_suffix(path.suffix + ".gz").read_bytes())


def read(folder, name):
    return json.loads(read_bytes(folder, name))


def stats(values):
    values = sorted(values)
    assert all(math.isfinite(v) and v >= 0 for v in values)
    percentile = lambda q: values[max(0, math.ceil(q * len(values)) - 1)] if values else None
    return dict(count=len(values), p50=percentile(.5), p95=percentile(.95),
                p99=percentile(.99), max=max(values) if values else None,
                mean=sum(values) / len(values) if values else None)


def aggregate(records, phase):
    summaries = [r["summary"][phase] for r in records]
    raw = [r["raw"][phase] for r in records]
    result = dict(
        repetitions=len(records),
        raf_ms=stats([s["interval"] for p in raw for s in p["samples"]]),
        input_display_gap_max_ms=stats([s["input_display_submission_gap_ms"]["max"] for s in summaries]),
        input_dispatch_ms=stats([s["dispatchMs"] for p in raw for s in p["samples"]]),
        settle_ms=stats([s["settle_ms"] for s in summaries]),
        updated_inputs=sum(s["updatedInputs"] for s in summaries),
        input_count=sum(s["inputCount"] for s in summaries),
        long_tasks=[t for p in raw for t in p["longTasks"]],
        client_wait_max_cpu_ms=stats([s["client_wait_cpu_ms"]["max"] for s in summaries
                                    if s["client_wait_cpu_ms"]["max"] is not None]),
        maximum_fence_timeout=max(s["maximum_fence_timeout"] for s in summaries),
    )
    if all(s["navigation"] is not None for s in summaries):
        result["navigation_per_phase"] = {
            key: stats([s["navigation"][key] for s in summaries])
            for key in ("queries", "built", "knots", "prefix_knots", "fallback", "cancelled",
                        "discarded_knots", "throttled", "prewarm", "calls")}
        for key in ("cpu_ms", "query_cpu_ms", "overlay_cpu_ms", "initialization_cpu_ms"):
            result["navigation_per_phase"][key + "_total"] = stats(
                [s["navigation"][key]["total"] for s in summaries])
        result["input_navigation_per_phase"] = {
            key: stats([s["inputNavigation"][key] for s in summaries])
            for key in ("queries", "built", "knots", "prefix_knots", "fallback")}
        exact = []
        for record in records:
            rows = [r["value"] for r in record["rows"] if r["kind"] == "frame" and r["phase"] == phase]
            assert rows and all(r["settle"] for r in rows)
            assert rows[-1]["outcome"] == "accepted"
            assert all(r["outcome"] == "retry" for r in rows[:-1])
            counts = collections.Counter()
            for row in rows:
                assert sum(s["exclusive_ns"] for s in row["stages"].values()) == row["stages"]["attempt"]["inclusive_ns"]
                counts.update(row["counts"])
            cpu = sum(r["stages"]["attempt"]["inclusive_ns"] for r in rows) / 1e6
            span = (rows[-1]["start_ns"] + rows[-1]["stages"]["attempt"]["inclusive_ns"] - rows[0]["start_ns"]) / 1e6
            assert span >= cpu - .001
            exact.append(dict(attempts=len(rows), cpu_ms=cpu, span_ms=span,
                              between_attempts_ms=max(0, span-cpu), counters=dict(counts)))
        result["exact_per_phase"] = {key: stats([e[key] for e in exact])
                                      for key in ("attempts", "cpu_ms", "span_ms", "between_attempts_ms")}
        result["exact_per_phase"]["counters"] = {
            key: stats([e["counters"].get(key, 0) for e in exact])
            for key in sorted(set().union(*(e["counters"] for e in exact)))}
        result["exact_exclusive_cpu_ms_per_phase"] = {
            key: stats([s["exactStages"].get(key, {}).get("exclusive_ms", 0) for s in summaries])
            for key in sorted(set().union(*(s["exactStages"] for s in summaries)))}
    return result


def main():
    diagnostic, default, output = map(Path, sys.argv[1:])
    summaries = [read(p, "summary.json") for p in (diagnostic, default)]
    for folder, summary in zip((diagnostic, default), summaries):
        for name, expected in read(folder, "sha256.json").items():
            assert hashlib.sha256(read_bytes(folder, name)).hexdigest() == expected, name
        before, after = [read(folder, "source-" + point + ".json") for point in ("before", "after")]
        assert before["files"] == after["files"] and before["commit"] == after["commit"]
        canonical = json.dumps(before["files"], sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode()
        assert hashlib.sha256(canonical).hexdigest() == before["tree_sha256"] == summary["sourceTreeSha256"]
    assert all(s["status"] == "MEASURED" and s["sourceStable"] and s["artifactStable"] for s in summaries)
    assert summaries[0]["expectTiming"] and not summaries[1]["expectTiming"]
    assert summaries[0]["sourceTreeSha256"] == summaries[1]["sourceTreeSha256"]
    cases = [[read(folder, c["id"] + ".json") for c in summary["cases"]]
             for folder, summary in zip((diagnostic, default), summaries)]
    assert len(cases[0]) == len(cases[1]) == 24
    parity = []
    for instrumented, ordinary in zip(*cases):
        assert instrumented["id"] == ordinary["id"]
        assert instrumented["status"] == ordinary["status"] == "MEASURED"
        for point in ("initial", "cold", "warm"):
            a = instrumented["setup"]["initialPixels"] if point == "initial" else instrumented["raw"][point]["pixels"]
            b = ordinary["setup"]["initialPixels"] if point == "initial" else ordinary["raw"][point]["pixels"]
            matched = a == b
            parity.append(dict(id=ordinary["id"], point=point, matched=matched, diagnostic=a, default=b))
            assert matched, f"ON/OFF pixel mismatch: {ordinary['id']} / {point}"
        for record in (instrumented, ordinary):
            assert record["close"]["closed"] and not record["exceptions"] and not record["raw"]["errors"]
            assert all(all(record["raw"][p]["checks"].values()) for p in ("cold", "warm"))
    groups = []
    for workload in ("long-line", "image", "table", "sticky"):
        for selected in (False, True):
            group = dict(workload=workload, selected=selected)
            for label, records in zip(("diagnostic", "default"), cases):
                matching = [r for r in records if r["workload"] == workload and r["selected"] == selected]
                assert len(matching) == 3
                group[label] = {phase: aggregate(matching, phase) for phase in ("cold", "warm")}
            groups.append(group)
    result = dict(schema="g5-web-cold-warm-analysis-v1", classification="MEASURED",
                  official_gate_pass_claimed=False, source_commit=summaries[0]["sourceCommit"],
                  source_tree_sha256=summaries[0]["sourceTreeSha256"], browser=summaries[0]["browserVersion"],
                  cases_per_build=24, phases_per_build=48, on_off_pixel_checks=len(parity),
                  all_on_off_pixels_match=True, aggregates=groups,
                  limits=["Fresh Runtime navigation cache cold; setup already renders exact geometry. Not OS/driver cold.",
                          "Headless Chrome, synthetic DOM input, hardware WebGL2; not physical input-to-photon or GPU timing.",
                          "Fallback counts are coverage resolver uses, not unique frames.",
                          "WASM timers can be quantized to 0.1ms. Zero timer samples do not prove zero cost.",
                          "Three repetitions characterize this bounded corpus, not a scalability SLO or other-platform PASS."])
    output.mkdir(parents=True, exist_ok=True)
    (output / "analysis.json").write_text(json.dumps(result, indent=2) + "\n")
    (output / "pixel-parity.json").write_text(json.dumps(dict(matched=True, checks=parity), indent=2) + "\n")
    print(json.dumps(dict(classification=result["classification"], pixel_checks=len(parity), groups=len(groups))))


if __name__ == "__main__":
    main()
