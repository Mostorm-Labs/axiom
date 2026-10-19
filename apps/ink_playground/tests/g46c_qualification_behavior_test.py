import json
import pathlib
import subprocess
import sys
import tempfile

if sys.argv[1] == "--artifact":
    data = json.loads(pathlib.Path(sys.argv[2]).read_text())
else:
    runner = pathlib.Path(sys.argv[1]).resolve()
    out = pathlib.Path(tempfile.mkdtemp(prefix="g46c-contract-"))
    proc = subprocess.run([str(runner), str(out), "contract", "--small"], capture_output=True, text=True, timeout=180)
    if proc.returncode != 0:
        raise SystemExit(f"runner failed unexpectedly: {proc.stdout}{proc.stderr}")
    data = json.loads((out / "qualification-run.json").read_text())
required = {"select-deselect", "move-resize-rotate", "slow-fast-pan-zoom-fit", "handle-hover", "multi-select", "move-snap-guide", "continuous-zoom", "zoom-settle", "zoom-to-fit", "100K-active-ink-laser", "100K-transform-overlay"}
ids = {entry["id"] for entry in data["runs"]}
missing = required - ids
if missing:
    raise AssertionError(f"missing frozen scenario selectors: {sorted(missing)}")
for entry in data["runs"]:
    for key in ("selected_count", "camera_generation_after", "transform_committed", "guide_count", "replay_digest"):
        if key not in entry:
            raise AssertionError(f"{entry['id']} lacks behavior metric {key}")
    assert entry["passed"], entry
runs = {entry["id"]: entry for entry in data["runs"]}
assert runs["select-deselect"]["selected_count"] == 0
assert runs["multi-select"]["selected_peak"] == 2
assert runs["handle-hover"]["handle_hits"] == 9
assert runs["move-resize-rotate"]["transform_committed"] == 3
assert runs["100K-transform-overlay"]["transform_committed"] == 3
assert runs["100K-transform-overlay"]["transient_transform_peak"] == 1
assert runs["move-snap-guide"]["guide_count"] >= 1
assert runs["continuous-zoom"]["camera_generation_after"] > runs["continuous-zoom"]["camera_generation_before"]
assert runs["zoom-settle"]["settled_camera_unchanged"]
assert runs["100K-active-ink-laser"]["laser_canonical_mutation"] is False
assert runs["100K-active-ink-laser"]["pen_commit_count"] == 1
print("PASS")
