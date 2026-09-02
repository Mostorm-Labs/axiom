import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const generator = fileURLToPath(new URL("../../../tools/generate_g1_04_c7_evidence.mjs", import.meta.url));
const packageRef = "e4facb7ab786b994dd5c9bb4bc3e4d57fe95d18e";
const p36SourceRef = "492d2f914f078a6e4ac8b567e07f7ec813c10107";
const p36MaterializedRef = "9b73be589ae070bc602b8989f83d89745a54774e";
const sourcePaths = [
  "verification/packages/semantic-conformance-cli/src/provider-diff.ts",
  "verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs",
  "verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs",
  "verification/tools/generate_g1_04_c7_evidence.mjs",
];

function sourceRef() {
  const result = spawnSync("git", ["log", "-1", "--format=%H", "--", ...sourcePaths], { cwd: repoRoot, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

function run(args) {
  return spawnSync(process.execPath, [generator, ...args], { cwd: repoRoot, encoding: "utf8" });
}

function assertGateSchema(value, ref) {
  assert.deepEqual(Object.keys(value).sort(), ["authorityRefs", "format", "formatVersion", "gateId", "provenance", "resultRefs", "sourceRef", "status"]);
  assert.equal(value.format, "axiom-g1-04-c-gate-v1");
  assert.equal(value.formatVersion, 1);
  assert.equal(value.provenance, "GATE_EVIDENCE");
  assert.equal(value.gateId, "GT-G1-04-C");
  assert.equal(value.status, "PASS");
  assert.equal(value.sourceRef, ref);
  assert.ok(Array.isArray(value.resultRefs) && value.resultRefs.length > 0);
  assert.equal(new Set(value.resultRefs).size, value.resultRefs.length);
  assert.ok(value.resultRefs.every((item) => typeof item === "string" && item.length > 0));
  assert.ok(Array.isArray(value.authorityRefs));
  assert.equal(new Set(value.authorityRefs).size, value.authorityRefs.length);
}

test("C7-R2 generator is deterministic, source-bound, and requires all seven Gate conditions to pass", () => {
  const ref = sourceRef();
  const firstDir = mkdtempSync(join(tmpdir(), "c7-evidence-a-"));
  const secondDir = mkdtempSync(join(tmpdir(), "c7-evidence-b-"));
  const first = run(["--source-ref", ref, "--output-dir", firstDir]);
  const second = run(["--source-ref", ref, "--output-dir", secondDir]);
  assert.equal(first.status, 0, first.stderr);
  assert.equal(second.status, 0, second.stderr);
  const names = ["C-GATE.json", "C-PROVIDER-DIFF.json"];
  assert.deepEqual(readdirSync(firstDir).sort(), names);
  assert.deepEqual(readdirSync(secondDir).sort(), names);
  for (const name of names) assert.equal(readFileSync(join(firstDir, name), "utf8"), readFileSync(join(secondDir, name), "utf8"));

  const differential = JSON.parse(readFileSync(join(firstDir, "C-PROVIDER-DIFF.json"), "utf8"));
  assert.equal(differential.packageRef, packageRef);
  assert.equal(differential.sourceRef, ref);
  assert.equal(differential.status, "PASS");
  assert.equal(differential.providerAgreement, "90/90");
  assert.equal(differential.divergenceCount, 0);
  assert.equal(differential.goldenPassCount, 90);
  assert.equal(differential.goldenFailCount, 0);
  assert.equal(differential.observationOnlyCount, 0);
  assert.deepEqual(differential.goldenMismatchCaseIds, []);
  assert.equal(differential.manualGoldenCorrectness, "PASS");
  assert.equal(differential.trustedDependencies.p36SourceRef, p36SourceRef);
  assert.equal(differential.trustedDependencies.p36MaterializedRef, p36MaterializedRef);
  assert.equal(differential.authorityManualExpected, true);
  assert.equal(differential.expectedTruthWrites, 0);
  assert.equal(differential.providerOutputUsedAsExpected, false);
  assert.equal(differential.productionSemanticDelta, 0);
  assert.equal(differential.fixtureReproducibility.byteIdentical, true);
  assert.equal(differential.fixtureReproducibility.caseCount, 90);
  assert.equal(differential.gateSummary.status, "PASS");
  assert.deepEqual(differential.gateSummary.failedConditions, []);
  assertGateSchema(JSON.parse(readFileSync(join(firstDir, "C-GATE.json"), "utf8")), ref);
});

test("C7 generator rejects invalid CLI usage, invalid lineage, and forbidden output roots", () => {
  const ref = sourceRef();
  const dir = mkdtempSync(join(tmpdir(), "c7-evidence-args-"));
  assert.notEqual(run(["--source-ref", ref]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", dir, "--unknown", "x"]).status, 0);
  assert.notEqual(run(["--source-ref", packageRef, "--output-dir", dir]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring/blocked")]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/generated/blocked")]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, "runtime/blocked")]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, `verification/evidence/gates/G1/${p36SourceRef}/GT-G1-04-C`)]).status, 0);
});
