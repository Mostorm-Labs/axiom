
from pathlib import Path
import json,sys,hashlib
root=Path(sys.argv[1])
host=(root/"apps/ink_playground/common/ink_playground_host.cpp").read_text(encoding="utf-8")
win=(root/"apps/ink_playground/platform/windows/main.cpp").read_text(encoding="utf-8")
web=(root/"apps/ink_playground/platform/web/bridge.cpp").read_text(encoding="utf-8")
obs={
 "INT-RED-HOST-DISPATCH":{"violations":[]},
 "INT-RED-WINDOWS-HUD":{"violations":[]},
 "INT-RED-WEB-PLATFORM-COMMIT":{"violations":[]},
}
for marker in ["if (selectionMode_)", "if (creationMode_ || creationActive())", "beginBrushSession(sample.key.pointer", "eraserBegin(sample.key.pointer)", "finishBrushSession(sample.key.pointer)"]:
 if marker in host: obs["INT-RED-HOST-DISPATCH"]["violations"].append(marker)
for marker in ["TextOutW(", "preview_present:", "Arc preview:", "runtime preview:", "SPACE = preview"]:
 if marker in win: obs["INT-RED-WINDOWS-HUD"]["violations"].append(marker)
for marker in ["phase == 2", "presentCanonicalFrame", "phase == 3", "cancelBrushSession(pointer)"]:
 if marker in web: obs["INT-RED-WEB-PLATFORM-COMMIT"]["violations"].append(marker)
for item in obs.values():
 item["baseline_defect_observed"]=bool(item["violations"])
 item["source_scope"]="read-only source boundary oracle"
red=any(v["baseline_defect_observed"] for v in obs.values())
payload={"oracle":"FC-00X-INTERACTION-INTERNALIZATION-RED-v1","expect":"baseline-red","status":"RED" if red else "GREEN","observations":obs}
print(json.dumps(payload,indent=2))
raise SystemExit(1 if red else 0)
