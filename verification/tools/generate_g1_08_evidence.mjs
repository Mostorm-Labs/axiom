import { readFileSync, mkdirSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { execFileSync } from "node:child_process";

export const TASK_ID = "GT-G1-08";
export const PACKAGE_REF = "notion://3d64c57a-590c-8196-813d-ffd71b4f52f7/GT-G1-08-P31-v0.2";
export const TASK_ANCHOR = "a2c3bfa05930b53739886d4d151be31dbcd6be15";
export const REQUIRED = ["G1-08-CORRECTNESS.json", "G1-08-LOCALITY.json", "G1-08-CTEST-ON.xml", "G1-08-CTEST-OFF.xml", "G1-08-RUN-MANIFEST.json"];
const sha = v => typeof v === "string" && /^[0-9a-f]{40}$/.test(v);
export function validateFacts(facts, sourceRef) {
  if (facts.taskId !== TASK_ID || facts.packageRef !== PACKAGE_REF || facts.taskAnchor !== TASK_ANCHOR) throw new Error("identity mismatch");
  if (!sha(sourceRef) || facts.sourceRef !== sourceRef) throw new Error("source mismatch");
  if (facts.acceptance?.AC08_L01 !== "PASS" || facts.acceptance?.AC08_L02 !== "PASS" || facts.acceptance?.AC08_L03 !== "FAIL_MEASURED") throw new Error("acceptance facts incomplete");
  if (!facts.provider?.runId || !facts.provider?.jobId || facts.provider?.sourceSha !== sourceRef) throw new Error("provider facts incomplete");
  return facts;
}
export function generateEvidence({sourceRef, factsPath, out, repositoryRoot = process.cwd()}) {
  const facts = validateFacts(JSON.parse(readFileSync(factsPath, "utf8")), sourceRef);
  const target = resolve(repositoryRoot, out); mkdirSync(target, {recursive:true});
  const root = {taskId:TASK_ID, packageRef:PACKAGE_REF, taskAnchor:TASK_ANCHOR, sourceRef, provider:facts.provider};
  writeFileSync(resolve(target, "G1-08-CORRECTNESS.json"), JSON.stringify({...root, acceptance:facts.acceptance, correctness:facts.correctness}, null, 2)+"\n");
  writeFileSync(resolve(target, "G1-08-LOCALITY.json"), JSON.stringify({...root, locality:facts.locality, knownRisk:"G1-08-RISK-DELETE-REVERSE-SCAN"}, null, 2)+"\n");
  writeFileSync(resolve(target, "G1-08-RUN-MANIFEST.json"), JSON.stringify({...root, evidenceInventory:REQUIRED, sourceToMaterialized:"EVIDENCE_ONLY_DESCENDANT", p34Claimed:false}, null, 2)+"\n");
  writeFileSync(resolve(target, "G1-08-CTEST-ON.xml"), facts.ctestOn);
  writeFileSync(resolve(target, "G1-08-CTEST-OFF.xml"), facts.ctestOff);
  return target;
}
if (process.argv[1] === new URL(import.meta.url).pathname) generateEvidence({sourceRef:process.argv[2], factsPath:process.argv[3], out:process.argv[4]});
