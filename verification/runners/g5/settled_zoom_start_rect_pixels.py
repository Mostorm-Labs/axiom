"""Offline same-Snapshot/operation/pixel checks on the frozen r2 buffers.

The native boundary tool must already have produced offline-pixel/*.rgba/edges.
This opens no browser and makes no SDK calls. Wire checks corroborate the real
SnapshotCodec restore/capture; they do not replace the semantic codec authority.
"""
from pathlib import Path
import base64
import hashlib
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "out/G5-settled-zoom-start-rect-repair-r2"
RES = ROOT / ".aegis/results/G5-settled-zoom-start-rect-repair-r2"
SPEC = importlib.util.spec_from_file_location(
    "wire_fields", ROOT / "verification/runners/g5/idle_input_feedback_analyze.py")
WIRE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WIRE)


def sha(value):
    return hashlib.sha256(value).hexdigest()


def main():
    request = json.loads((OUT / "requests/SZ2-IAB.json").read_text())
    all_rows = []
    for name in ["web/SZ2-CHROME-1", "web/SZ2-CHROME-2", "iab/SZ2-IAB"]:
        raw = json.loads((OUT / name / "raw.json").read_text())
        assert not raw["errors"]
        all_rows.extend((name, row) for row in raw["rows"])
    states = []
    for window, row in all_rows:
        before = base64.b64decode(row["snapshots"]["before-zoom"]["base64"])
        after = base64.b64decode(row["snapshots"]["after-zoom"]["base64"])
        restore = next(value["snapshot"] for value in request["restores"]
                       if value["name"] == row["q"]["name"]
                       and value["pixel"] == bool(row["q"].get("pixel")))
        original = base64.b64decode(restore["base64"])
        assert WIRE.objects(original) == WIRE.objects(before) == WIRE.objects(after)
        assert WIRE.first(original, 1) == WIRE.first(before, 1) == WIRE.first(after, 1)
        assert not row["operations"] and not row["operationBuffers"] and not row["errors"]
        assert row["status"] == "OBSERVED" and row["closed"] and row["navRestored"]
        assert not row.get("closeError") and not row.get("cleanupError")
        phase = row["phases"][-1]
        assert phase["preNavigationStable"]["state"]["camera"] == {"scale": 1, "x": 0, "y": 0}
        assert row["end"]["camera"] == {"scale": .5, "x": 299.99993896484375, "y": 200}
        assert phase["before"]["generation"] == row["end"]["generation"]
        assert row["end"]["generation"] == row["end"]["lastCommit"]["presentedGeneration"]
        observer = row["observer"]
        assert all(value["result"].get("ok") is not False
                   for value in observer["calls"]["values"] + observer["terminals"]["values"])
        states.append({"window": window, "lane": row["lane"], "case": row["q"]["name"],
                       "pixel": bool(row["q"].get("pixel")), "objects": len(WIRE.objects(before)),
                       "source_snapshot_sha256": sha(original), "source_objects_unchanged": True,
                       "camera_equal": True, "navigation_operations": 0,
                       "no_false_canonical_mutation_receipt": True,
                       "closed": True,
                       "ring_loss": {key: value["lost"] for key, value in observer.items()
                                     if isinstance(value, dict) and "lost" in value}})
    pixel_results = []
    iab_rows = [row for window, row in all_rows if window.startswith("iab/") and row["q"].get("pixel")]
    for name in ["outside-one-stroke", "outside-three-strokes-masked", "central-short"]:
        lanes = {row["lane"]: row for row in iab_rows if row["q"]["name"] == name}
        captures = {lane: {pixel["label"]: pixel for pixel in row["pixels"]}
                    for lane, row in lanes.items()}
        rgba = {lane: base64.b64decode(pixels["final"]["base64"])
                for lane, pixels in captures.items()}
        edge = (OUT / "offline-pixel" / (name + ".edges")).read_bytes()
        cpu = (OUT / "offline-pixel" / (name + ".rgba")).read_bytes()
        assert len(cpu) == 2400 * 1600 * 4 and len(edge) == 2400 * 1600
        assert len({pixels["before-zoom"]["base64"] for pixels in captures.values()}) == 1
        assert rgba["r1"] == rgba["bc40"]
        changed_channels = [i for i, (a, b) in enumerate(zip(rgba["bc40"], rgba["r2"])) if a != b]
        changed_pixels = sorted({i // 4 for i in changed_channels})
        assert all(edge[index] for index in changed_pixels)
        full_parity = {}
        for lane, buffer in rgba.items():
            assert len(buffer) == len(cpu)
            outside = [i for i, (a, b) in enumerate(zip(cpu, buffer)) if a != b and not edge[i // 4]]
            assert not outside
            full_parity[lane] = {"outside_fixed_boundary_mismatched_channels": len(outside),
                                 "outside_fixed_boundary_max_delta": 0}
        samples = {}
        for lane, pixels in captures.items():
            assert lanes[lane]["pixelRAFRestored"]
            before = pixels["before-zoom"]["base64"]
            values = []
            for label, pixel in pixels.items():
                metadata = pixel["metadata"]
                assert metadata["status"] == "VALID" and not metadata["context"]["lost"]
                assert metadata["restoration"]["status"] == "RESTORED"
                assert metadata["canvas"]["width"] == 2400 and metadata["canvas"]["height"] == 1600
                if label.startswith("opportunity-"):
                    values.append({"label": label, "camera": pixel["camera"],
                                   "same_RGBA_as_old_ROI": pixel["base64"] == before,
                                   "sha256": sha(base64.b64decode(pixel["base64"]))})
            samples[lane] = values
        pixel_results.append({
            "case": name, "full_rgba_bytes": len(cpu), "before_RGBA_exact_all_lanes": True,
            "final_RGBA_exact": not changed_channels, "r1_final_RGBA_exact_baseline": True,
            "r2_final_changed_pixels": len(changed_pixels), "r2_final_changed_channels": len(changed_channels),
            "max_changed_channel_delta": max((abs(rgba["bc40"][i] - rgba["r2"][i])
                                               for i in changed_channels), default=0),
            "all_differences_on_independent_fixed_boundary": True,
            "all_outside_boundary_exact_CPU": full_parity,
            "final_RGBA_sha256": {lane: sha(buffer) for lane, buffer in rgba.items()},
            "CPU_sha256": sha(cpu), "fixed_boundary_sha256": sha(edge),
            "first_three_opportunities_ROI": samples,
        })
    report = {
        "schema": "axiom-rectangle-zoom-semantic-pixel-v1", "state_rows": states,
        "pixel_results": pixel_results,
        "all_semantic_preserved": True,
        "scoped_pixel_qualification": "Original source-only fixed1devicepixel contour/mask band; every nonedge pixel exact independent CPU. Not fullRGBA exact and not the native2LSB quantization allowance.",
        "acceptance_clarification": ".aegis/results/G5-settled-zoom-start-rect-control-r2/final-pixel-acceptance-clarification.json",
        "limits": [
            "First3 opportunities are finite160x1600 centralROI only; ROI movement is not complete first-frame/photon proof.",
            "Native independent cold full-grid+retained-image oracle establishes first complete viewport at DPR1/2/4; no skipped strips.",
            "Real SnapshotCodec handles restore/capture. Offline field comparisons corroborate payload preservation, not a new codec oracle.",
            "GL error queue not consumed. Target identity, no lost context and restored GL read state witnessed.",
            "Reported build sourceRevision can predate snapshot source. Exact SDK/source/manifests bind measured assets.",
        ],
    }
    (RES / "semantic-pixel-observation.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"state_rows": len(states), "pixel_cases": len(pixel_results),
                      "differing_pixels": [r["r2_final_changed_pixels"] for r in pixel_results],
                      "nonedge_exact_CPU": True}))


if __name__ == "__main__":
    main()
