import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const generator = fileURLToPath(new URL("../../../tools/generate_g1_04_c6_evidence.mjs", import.meta.url));
const PACKAGE_REF = "8a43af6ebb9b88ecefb79141eb90b5680405a449";

function sourceRef() {
  const result = spawnSync("git", ["log", "-1", "--format=%H", "--", "verification/packages/semantic-conformance-cli/src/cross-cutting.ts", "verification/packages/semantic-conformance-cli/test/cross-cutting.test.mjs", "verification/packages/semantic-conformance-cli/test/cross-cutting-evidence.test.mjs", "verification/tools/generate_g1_04_c6_evidence.mjs"], { cwd: repoRoot, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

function run(args) {
  return spawnSync(process.execPath, [generator, ...args], { cwd: repoRoot, encoding: "utf8" });
}

test("C6 evidence generator is deterministic and emits exactly four durable artifacts", () => {
  const ref = sourceRef();
  const firstDir = mkdtempSync(join(tmpdir(), "c6-evidence-a-"));
  const secondDir = mkdtempSync(join(tmpdir(), "c6-evidence-b-"));
  const first = run(["--source-ref", ref, "--output-dir", firstDir]);
  const second = run(["--source-ref", ref, "--output-dir", secondDir]);
  assert.equal(first.status, 0, first.stderr);
  assert.equal(second.status, 0, second.stderr);
  const names = ["C-IDEMPOTENCY.json", "C-NO-MUTATION.json", "C-PLAN-PROJECTION.json", "C-OPEN-RECONCILIATION.json"];
  assert.deepEqual(readdirSync(firstDir).sort(), names.slice().sort());
  assert.deepEqual(readdirSync(secondDir).sort(), names.slice().sort());
  for (const name of names) assert.equal(readFileSync(join(firstDir, name), "utf8"), readFileSync(join(secondDir, name), "utf8"));
  const noMutation = JSON.parse(readFileSync(join(firstDir, "C-NO-MUTATION.json"), "utf8"));
  assert.equal(noMutation.packageRef, PACKAGE_REF);
  assert.equal(noMutation.sourceRef, ref);
  assert.equal(noMutation.acceptedCases, 90);
  assert.equal(noMutation.observationCount, 180);
  assert.equal(noMutation.beforeAfterEqual, "180/180");
  const open = JSON.parse(readFileSync(join(firstDir, "C-OPEN-RECONCILIATION.json"), "utf8"));
  assert.equal(open.geometry.checkedAddition, "INTEGER_OVERFLOW");
  assert.equal(open.geometry.historicalOverflowCaseUsedAsArithmeticOracle, false);
});

test("C6 evidence generator enforces its CLI, lineage, and forbidden output boundary", () => {
  const ref = sourceRef();
  const dir = mkdtempSync(join(tmpdir(), "c6-evidence-args-"));
  assert.notEqual(run(["--source-ref", ref]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", dir, "--unknown", "x"]).status, 0);
  assert.notEqual(run(["--source-ref", PACKAGE_REF, "--output-dir", dir]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, "verification/corpus/semantic/v1/g1-04-c/generated/blocked")]).status, 0);
  assert.notEqual(run(["--source-ref", ref, "--output-dir", join(repoRoot, "runtime/blocked")]).status, 0);
});
