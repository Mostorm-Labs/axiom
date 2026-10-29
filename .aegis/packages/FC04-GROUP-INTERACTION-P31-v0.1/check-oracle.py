import json
from pathlib import Path
p=Path(__file__).resolve().parent
a=json.loads((p/"oracle-candidate.json").read_text())["cases"][1]; w=a["fixture"]["group_world"]
for k in ("A","B"):
 t=a["fixture"][k+"_local"]
 got=[w[0],w[1],w[2],w[3],w[0]*t[4]+w[2]*t[5]+w[4],w[1]*t[4]+w[3]*t[5]+w[5]]
 if got!=a["expected"][k+"_world"]: raise SystemExit("arithmetic mismatch: "+k)
print("PASS independent arithmetic; this is not product verification")
