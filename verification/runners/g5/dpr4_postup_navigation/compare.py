"""Offline comparison of the two fixed ordinary SDK observations."""
from pathlib import Path
from hashlib import sha256
import json

ROOT = Path(__file__).resolve().parents[4]
RAW = ROOT / "out/G5-DPR4-postup-navigation-r1"
RESULT = ROOT / ".aegis/results/G5-DPR4-postup-navigation-r1/ordinary-comparison.json"
observations = {tag: json.loads((RAW / ("ordinary-" + tag) / "observation.json").read_text()) for tag in ("B", "C")}
assert all(o["status"] == "OBSERVED" and len(o["cases"]) == 1 and o["cases"][0]["id"] == "wax16-default" for o in observations.values())
a, b = (RAW / ("ordinary-" + tag) for tag in ("B", "C"))
names = sorted(p.name for p in a.glob("*.bin")) + ["wax16-default-final.axsnap", "wax16-default-final.rgba"]
artifacts = []
for name in names:
    x, y = (p.joinpath(name).read_bytes() for p in (a, b))
    row = {"name": name, "B": {"bytes": len(x), "sha256": sha256(x).hexdigest()}, "C": {"bytes": len(y), "sha256": sha256(y).hexdigest()}, "equal": x == y}
    if name.endswith(".rgba") and x != y:
        row.update(differentChannels=sum(i != j for i, j in zip(x, y)), maximumDifference=max(abs(i-j) for i, j in zip(x, y)))
    artifacts.append(row)
summary = []
for index in range(2):
    row = {"lane": observations["B"]["cases"][0]["data"]["repeats"][index]["name"]}
    for tag, observation in observations.items():
        case = observation["cases"][0]
        lane, metric = case["data"]["repeats"][index], case["metrics"][index]
        row[tag] = {key: metric.get(key) for key in ("callbacks", "p95", "maximum", "upToGenerationFrame", "terminalConvergence", "inverseConvergence", "activeGestureWriteGap", "firstPanActivity")}
        row[tag].update(gestureContentWrites=sum(p["visible"] and p["kind"] == "content" and lane["panStart"] <= p["started"] <= lane["panEnded"] for p in lane["colorOps"]), cameraReceiptMatches=lane["generationFrame"] is not None)
    row["bands"] = {"pan3s" if index == 0 else "pinch5s": "NOT_REACHED"}
    summary.append(row)
result = {"classification": "CORRECTNESS_EQUAL_WEB_FEEDBACK_UNMET", "driverSame": observations["B"]["driverSHA256"] == observations["C"]["driverSHA256"], "summary": summary,
          "pair": [{"build": tag, "metrics": o["cases"][0]["metrics"], "buildInfo": o["cases"][0]["data"]["build"]} for tag, o in observations.items()],
          "artifacts": artifacts, "allArtifactsEqual": all(r["equal"] for r in artifacts),
          "limitation": "Activity does not prove correct intermediate feedback. Native independent sampling oracle is separate; no relative performance PASS."}
assert result["driverSame"] and len(artifacts) == 20
RESULT.write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"driverSame": result["driverSame"], "allArtifactsEqual": result["allArtifactsEqual"], "summary": summary}))
