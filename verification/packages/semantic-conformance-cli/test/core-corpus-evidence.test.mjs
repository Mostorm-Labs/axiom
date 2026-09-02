import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const generator = fileURLToPath(new URL("../../../tools/generate_g1_04_c5_evidence.mjs", import.meta.url));
const C5_SOURCE_REF = "1342dc52aaeb7e1bf09a38e08d44ef72523a9d79";

function run(args) {
  return spawnSync(process.execPath, [generator, ...args], { cwd: repoRoot, encoding: "utf8" });
}

function generatedEvidence() {
  const dir = mkdtempSync(join(tmpdir(), "c5-evidence-"));
  const output = join(dir, "core.json");
  const sourceRef = C5_SOURCE_REF;
  const result = run(["--source-ref", sourceRef, "--output", output]);
  assert.equal(result.status, 0, result.stderr);
  return { output, evidence: JSON.parse(readFileSync(output, "utf8")), sourceRef };
}

test("C5 evidence generator is deterministic and reports complete core corpus facts", () => {
  const first = generatedEvidence();
  const dir = mkdtempSync(join(tmpdir(), "c5-evidence-repeat-"));
  const output = join(dir, "repeat.json");
  const result = run(["--source-ref", first.sourceRef, "--output", output]);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(readFileSync(first.output, "utf8"), readFileSync(output, "utf8"));
  const evidence = first.evidence;
  assert.equal(evidence.format, "axiom-gt-g1-04-c-core-corpus-v1");
  assert.equal(evidence.packageRef, "12832ab5df9d8638ad1712b182d402d2a0e4d311");
  assert.equal(evidence.sourceRef, first.sourceRef);
  assert.equal(evidence.suiteId, "GT-G1-04-C-CORE");
  assert.equal(evidence.selectedCaseCount, 90);
  assert.equal(evidence.selectedExpectedCount, 90);
  assert.equal(evidence.selectedObservationCount, 180);
  assert.equal(evidence.operationFamilyCount, 15);
  assert.deepEqual(evidence.missingMandatoryFamilies, []);
  assert.deepEqual(evidence.wrongFamilyCaseIds, []);
  assert.deepEqual(evidence.unselectedCaseIds, []);
  assert.equal(evidence.acceptedOpenPolicyCount, 0);
  assert.equal(evidence.acceptedExpectedAllAuthorityManual, true);
  assert.equal(evidence.expectedTruthWrites, 0);
  assert.equal(evidence.providerOutputUsedAsExpected, false);
  assert.equal(evidence.productionSemanticDeltaFromC3, 0);
  assert.equal(evidence.results.length, 90);
});

test("C5 evidence generator rejects missing, unknown, unsafe, or forbidden output arguments", () => {
  const dir = mkdtempSync(join(tmpdir(), "c5-evidence-args-"));
  const output = join(dir, "core.json");
  assert.notEqual(run(["--output", output]).status, 0);
  assert.notEqual(run(["--source-ref", "HEAD"]).status, 0);
  assert.notEqual(run(["--source-ref", "HEAD", "--output", output, "--unknown", "x"]).status, 0);
  assert.notEqual(run(["--source-ref", "HEAD", "--output", join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring/blocked.json")]).status, 0);
});

test("C5 evidence generator rejects source refs outside the accepted immutable lineage", () => {
  const dir = mkdtempSync(join(tmpdir(), "c5-evidence-lineage-"));
  const output = join(dir, "core.json");
  assert.notEqual(run(["--source-ref", "14b0a6ed30d62983c7c067902e236b78a28e61c5", "--output", output]).status, 0);
  assert.notEqual(run(["--source-ref", "c78cb67", "--output", output]).status, 0);
});
