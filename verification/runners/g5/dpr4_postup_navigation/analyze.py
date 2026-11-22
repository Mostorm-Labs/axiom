"""Join the previously captured WASM owner records to browser RAF/input scopes."""
from collections import Counter
from hashlib import sha256
from pathlib import Path
import json

ROOT = Path.cwd()
RAW = ROOT / "out/G5-DPR4-postup-navigation-r1"
OUT = ROOT / ".aegis/results/G5-DPR4-postup-navigation-control-r1/retained-attribution.json"

def binding(path):
    data = path.read_bytes()
    return {"path": str(path), "bytes": len(data), "sha256": sha256(data).hexdigest()}

def rollup(callbacks, observation, lower, upper):
    elapsed = upper - lower
    scopes = Counter()
    callback_ms = Counter()
    reasons = Counter()
    nav_work = Counter()
    canonical_work = Counter()
    body_work = Counter()
    pressure_work = Counter()
    canonical_stages = Counter()
    preparation_phases = Counter()
    callback_intervals = []
    align = Counter()
    for cb in callbacks:
        start, end = cb["start"], cb["end"]
        if end < lower or start >= upper:
            continue
        if start < lower or end > upper:
            align["boundarySpanningCallbacks"] += 1
        scopes[cb["kind"]] += 1
        callback_ms[cb["kind"]] += max(0, min(end, upper) - max(start, lower))
        if cb["kind"] == "raf":
            callback_intervals.append((max(start, lower), min(end, upper)))
        for s in cb["records"]:
            if s.startswith("AXIOM_IF_EVENT "):
                if "nc_render role=navigation" in s:
                    reasons["navigation " + s.split(" code=", 1)[1]] += 1
                elif "nc_render role=canonical" in s:
                    reasons["canonical " + (s.split(" reason=", 1)[1] or "accepted")] += 1
                elif "ien_reuse " in s:
                    reasons["reuse " + s.split("reason=", 1)[1].split()[0]] += 1
                elif "nc_prepare role=canonical " in s:
                    words = dict(x.split("=", 1) for x in s.split()[4:])
                    preparation_phases[f'{words["phase"]} code={words["code"]}'] += 1
                continue
            prefix, payload = s.split(" ", 1)
            record = json.loads(payload)
            if "start_ns" in record:
                native_ms = int(record["start_ns"]) / 1e6
                align["timedRecords"] += 1
                align["insideWrapper"] += start - 0.3 <= native_ms <= end + 0.3
            if prefix == "AXIOM_NAVIGATION_TIMING":
                for key in ("elapsed_ns", "query_ns", "initialization_ns", "contour_ns",
                            "material_ns", "knots", "queries", "initialized", "built",
                            "cancelled", "discarded_knots", "throttled", "coverage",
                            "presented", "prepared_hits", "prepared_builds", "regional_tile_proofs"):
                    nav_work[key] += record[key]
                nav_work["retained_region_max"] = max(nav_work["retained_region_max"],
                                                       record["retained_valid_regions"])
            elif prefix == "AXIOM_FRAME_TIMING":
                canonical = record["role"] == "canonical"
                role = record["role"]
                scopes[role] += 1
                if role not in ("canonical", "owner"):
                    continue
                for key in ("attempt", "canonical_frame", "navigation_frame"):
                    inclusive = record["stages"].get(key, {}).get("inclusive_ns", 0)
                    callback_ms[role + "." + key] += inclusive / 1e6
                if canonical:
                    canonical_stages.update({k: v["exclusive_ns"] for k, v in record["stages"].items()})
                    for key in ("coverage_steps", "gpu_coverage_tiles", "cpu_coverage_tiles",
                                "mask_primitive_draws", "render_alloc_denials"):
                        canonical_work[key] += record["counts"][key]
                    for key in ("resource_pending_release", "resource_gpu_epochs"):
                        canonical_work["max_" + key] = max(canonical_work["max_" + key],
                                                            record["counts"][key])
                    stages = record["stages"]
                    canonical_work["coverageStepOpportunities"] += record["counts"]["coverage_steps"] > 0
                    canonical_work["gpuTileOpportunities"] += record["counts"]["gpu_coverage_tiles"] > 0
                    canonical_work["coverageFlushOpportunities"] += "coverage_flush" in stages
            elif prefix == "AXIOM_PRESSURE_WORK" and record["role"] == "canonical":
                pressure_work["samples"] += 1
                pressure_work["withQueued"] += record["queued"] > 0
                pressure_work["maxQueued"] = max(pressure_work["maxQueued"], record["queued"])
                for key in ("direct_calls", "range_calls", "visited", "queued", "enqueue_ns"):
                    pressure_work[key] += record[key]
            elif prefix == "AXIOM_PREPARED_BODY_WORK" and record["purpose"] == "canonical":
                body_work["samples"] += 1
                body_work["withAdvances"] += record["advances"] > 0
                for key in ("canonical_queries", "job_admissions", "advances", "support_knots",
                            "distance_inputs", "compile_inputs", "path_primitives"):
                    body_work[key] += record[key]
    return {
        "fromMs": lower, "toMs": upper, "elapsedMs": round(elapsed, 3),
        "visibleContentWrites": sum(lower <= row["started"] < upper and row["kind"] == "content"
                                    and row["visible"] for row in observation["colorOps"]),
        "scopes": dict(scopes), "callbackMilliseconds": {k: round(v, 3) for k,v in callback_ms.items()},
        "outsideObservedRafMs": round(elapsed - callback_ms["raf"], 3),
        "rafStartIntervalMedianMs": round(sorted([b[0]-a[0] for a,b in zip(callback_intervals,
                        callback_intervals[1:])])[max(0, (len(callback_intervals)-2)//2)], 3)
                        if len(callback_intervals) >= 2 else None,
        "clockAlignment": dict(align), "reasons": dict(reasons), "preparation": dict(preparation_phases),
        "navigationWork": dict(nav_work), "canonicalWork": dict(canonical_work),
        "canonicalPreparedGeometry": dict(body_work), "canonicalPressure": dict(pressure_work),
        "canonicalExclusiveMilliseconds": {k: round(v / 1e6, 3) for k,v in canonical_stages.most_common()},
    }

def sample(name):
    p = RAW / name
    opath, cpath = p / "observation.json", p / "native-console.json"
    observation = json.loads(opath.read_bytes())
    callbacks, active, unscoped = [], [], Counter()
    tracked = ("AXIOM_NAVIGATION_TIMING ", "AXIOM_FRAME_TIMING ", "AXIOM_IF_EVENT ",
               "AXIOM_PRESSURE_WORK ", "AXIOM_PREPARED_BODY_WORK ")
    for event in json.loads(cpath.read_bytes()):
        line = event["text"]
        if line.startswith("PN_CALLBACK "):
            row = json.loads(line.split(" ", 1)[1])
            if row["edge"] == "enter":
                active.append({"kind": row["kind"], "id": row["id"], "start": row["at"],
                               "phase": row["phase"], "records": []})
            else:
                cb = active.pop()
                assert (cb["kind"], cb["id"]) == (row["kind"], row["id"])
                callbacks.append(dict(cb, end=row["at"]))
        elif line.startswith(tracked):
            if active:
                active[-1]["records"].append(line)
            else:
                unscoped[line.split(" ", 1)[0]] += 1
    assert not active
    callbacks.sort(key=lambda x: x["start"])
    rows = []
    for row in observation["cases"][0]["data"]["repeats"]:
        up, start, end, receipt = row["up"]["ended"], row["panStart"], row["panEnded"], row["generationFrame"]
        assert up <= start < end < receipt <= row["gestureQuietAt"]
        windows = [("activeGesture", start, end), ("quietToReceipt", end, receipt), ("upToReceipt", up, receipt)]
        phases = {phase: rollup(callbacks, row, lower, upper) for phase,lower,upper in windows}
        assert all(p["clockAlignment"]["insideWrapper"] == p["clockAlignment"]["timedRecords"]
                   for p in phases.values()), (name, row["name"])
        rows.append({"name": row["name"], "upDispatchMs": round(up - row["up"]["started"], 3),
                      "contacts": row["panContacts"], "expectedGeneration": row["eraseGeneration"],
                      "camera": row["afterPan"]["camera"], "windows": phases})
    ledger = ROOT / ".aegis/results/G5-DPR4-postup-navigation-r1" / (name + "-ledger.json")
    return {"name": name, "observation": binding(opath), "console": binding(cpath),
            "launchLedger": binding(ledger), "driverSHA256": observation["driverSHA256"],
            "unscopedTrackedRecords": dict(unscoped), "rows": rows}


result=sample("DN0-current")
result["firstGuard"]={"reason":"mask-value", "count":39, "contentOwnerRefusals":0}
result["limitations"]=["CPU API durations do not measure hardware GPU execution.", "Inter-callback wall time is observed; its scheduler/compositor cause remains unknown.", "Same total pressure draws is not evidence of duplicate same-grid draws."]
p=ROOT/".aegis/results/G5-DPR4-postup-navigation-r1/DN0-attribution.json"
p.write_text(json.dumps(result,indent=2)+"\n")
for row in result["rows"]:
 print(row["name"],json.dumps({k:row["windows"][k] for k in ("activeGesture","upToReceipt")}))
