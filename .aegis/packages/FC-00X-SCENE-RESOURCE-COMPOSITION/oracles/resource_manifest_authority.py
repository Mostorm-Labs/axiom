#!/usr/bin/env python3
from pathlib import Path
import argparse, sys

def main():
 p=argparse.ArgumentParser(); p.add_argument("--root", type=Path, required=True); p.add_argument("--expect", choices=("baseline","bound"), required=True); a=p.parse_args(); r=a.root.resolve()
 needles=[]
 for path in [r/"runtime/semantic/include/canvas/semantic/snapshot.hpp",r/"runtime/semantic/include/canvas/semantic/object_content.hpp",r/"schema/axiom/v1/proto/auditoryworks/axiom/v1/snapshot.proto"]:
  text=path.read_text(encoding="utf-8")
  needles.append((str(path.relative_to(r)), "ResourceManifest" in text or "resource_manifest" in text))
 if a.expect=="baseline":
  if any(v for _,v in needles):
   print("BOUND: manifest marker found", needles); return 0
  print("BLOCKED_AUTHORITY: no typed ResourceManifest in accepted semantic snapshot/object/protobuf sources", needles); return 2
 print("PASS" if all(v for _,v in needles) else "FAIL: manifest source not fully bound"); return 0 if all(v for _,v in needles) else 1
if __name__=="__main__": sys.exit(main())
