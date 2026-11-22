"""Offline actual-owner work accounting; no browser run or inferred GPU time."""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RAW = ROOT / "out/G5-DPR4-postup-navigation-r1/DN1-stream"
observation = json.loads((RAW / "observation.json").read_text())
stack, callbacks = [], []
for event in json.loads((RAW / "native-console.json").read_text()):
    line = event["text"]
    if line.startswith("PN_CALLBACK "):
        row = json.loads(line.split(" ", 1)[1])
        if row["edge"] == "enter":
            stack.append({"kind": row["kind"], "id": row["id"], "start": row["at"], "records": []})
        else:
            current = stack.pop()
            assert (current["kind"], current["id"]) == (row["kind"], row["id"])
            callbacks.append(dict(current, end=row["at"]))
    elif stack and line.startswith(("AXIOM_PRESSURE_WORK ", "AXIOM_FRAME_TIMING ", "AXIOM_IF_EVENT ", "AXIOM_SNR_PROGRESS ")):
        stack[-1]["records"].append(line)
assert not stack
rows = []
for phase in observation["cases"][0]["data"]["repeats"]:
    lower, upper = phase["panStart"], phase["generationFrame"]
    pressure, prepared, resets, mask = Counter(), Counter(), Counter(), Counter()
    material, actual_tiles = [], Counter()
    for cb in callbacks:
        if not lower <= cb["start"] < upper:
            continue
        for line in cb["records"]:
            prefix, payload = line.split(" ", 1)
            if prefix == "AXIOM_PRESSURE_WORK":
                row = json.loads(payload)
                if row["role"] == "canonical":
                    for key in ("visited", "queued", "range_calls", "direct_calls", "enqueue_ns"):
                        pressure[key] += row[key]
                    pressure["commandProducingOpportunities"] += row["queued"] > 0
            elif prefix == "AXIOM_FRAME_TIMING":
                row = json.loads(payload)
                if row["role"] == "canonical":
                    for key in ("coverage_steps", "gpu_coverage_tiles", "cpu_coverage_tiles", "render_alloc_denials", "history_draw_primitives", "mask_primitive_draws", "prepare_resets"):
                        mask[key] += row["counts"].get(key, 0)
            elif prefix == "AXIOM_SNR_PROGRESS":
                row = json.loads(payload)
                if row["phase"] == "reset-before":
                    resets[row["reason"]] += 1
                if row["phase"] == "prepare-yield":
                    actual_tiles[(row["semantic"], row["scene"], row["camera"], row["target_epoch"], tuple(row["transform"]), row["cursor"], row["tile_x"], row["tile_y"])] += 1
            elif prefix == "AXIOM_IF_EVENT" and "nc_prepare role=transient phase=material" in line:
                material.append({"atMs": int(line.split("t_ns=")[1].split()[0]) / 1e6, "line": line})
    rows.append({"lane": phase["name"], "canonicalPressure": dict(pressure), "canonicalWork": dict(mask), "resetReasonsAcrossWindow": dict(resets),
                 "observedYieldDeviceTileIdentities": [{"semantic": k[0], "scene": k[1], "camera": k[2], "targetEpoch": k[3], "transform": k[4], "entryCursor": k[5], "tileX": k[6], "tileY": k[7], "yieldCount": v} for k, v in actual_tiles.items()],
                 "terminalMaterialProgress": material})
result = {"rows": rows,
          "actualNativeReduction": {"before": "native-alpha.json ordered-clear-ink-only: 825 pressure commands in 9 opportunities", "after": "final-mechanism-green.json ordered-clear-ink-only: 0 pressure commands in 1 opportunity; independent full null-cache pixels exactly equal"},
          "remaining": ["Canonical target matrix/grid changes at pan/pinch. Counts are actual queued procedural draws, not prepared knot compilation.", "Ready pixel bypass exists under full body/context/matrix/grid/material identity. Integer-translation grid aliasing has no independent correctness proof in this package and was left unchanged.", "Yield tile identities are cursor observations, not complete per-primitive command stamps; repeated observations can mean continued work. They do not prove duplicate replay.", "GPU API flush/enqueue durations are CPU API time. Hardware duration is unknown. Inter-RAF wall time remains unattributed.", "Terminal material and nonempty exposure still exceed the fixed active-gesture opportunity window. No new budget or cadence policy was introduced."]}
path = ROOT / ".aegis/results/G5-DPR4-postup-navigation-r1/work-accounting.json"
path.write_text(json.dumps(result, indent=2) + "\n")
print(path)
