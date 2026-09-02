import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";

const repoRoot = fileURLToPath(new URL("../../../../", import.meta.url));
const generator = fileURLToPath(new URL("../../../tools/generate_g1_04_c_p36_golden_repair_evidence.mjs", import.meta.url));
const executionStartRef = "666082e114310503b019873fb9e12744c19bcc1e";
const historicalObservations = join(repoRoot, "verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json");

function generate(outputDir, observations) {
  return spawnSync(process.execPath, [generator,
    "--source-ref", executionStartRef,
    "--observations", observations,
    "--output-dir", outputDir,
  ], { cwd: repoRoot, encoding: "utf8" });
}

test("P36 golden materializer reports observed core-corpus results without writing expected truth", () => {
  assert.equal(existsSync(historicalObservations), true);
  const observationDir = mkdtempSync(join(tmpdir(), "p36-golden-observations-"));
  const observations = join(observationDir, "observations.json");
  const currentShape = JSON.parse(readFileSync(historicalObservations, "utf8"));
  currentShape.expectedTruthReads = 0;
  currentShape.semanticCodecCalls = 0;
  currentShape.decodedInputAudit = {
    negativeZero: {
      caseId: "C1-TRANSFORM-NEGATIVE-ZERO",
      requiredBits: "8000000000000000",
      byProvider: {
        reference: ["8000000000000000"],
        indexed: ["8000000000000000"],
      },
    },
  };
  writeFileSync(observations, `${JSON.stringify(currentShape, null, 2)}\n`);
  const firstDir = mkdtempSync(join(tmpdir(), "p36-golden-evidence-"));
  const first = generate(firstDir, observations);
  assert.equal(first.status, 0, first.stderr);
  const secondDir = mkdtempSync(join(tmpdir(), "p36-golden-evidence-"));
  const second = generate(secondDir, observations);
  assert.equal(second.status, 0, second.stderr);

  for (const name of ["C-PLAN-PROJECTION.json", "C-NO-MUTATION.json", "C-CORE-CORPUS.json", "P36-VERIFICATION-GOLDEN-REPAIR.json"]) {
    assert.equal(readFileSync(join(firstDir, name), "utf8"), readFileSync(join(secondDir, name), "utf8"));
  }

  const summary = JSON.parse(readFileSync(join(firstDir, "P36-VERIFICATION-GOLDEN-REPAIR.json"), "utf8"));
  assert.equal(summary.executionStartRef, executionStartRef);
  assert.equal(summary.sourceRef, executionStartRef);
  assert.equal(summary.authorityManualExpected, true);
  assert.equal(summary.expectedTruthWrites, 0);
  assert.equal(summary.providerOutputUsedAsExpected, false);
  assert.deepEqual(summary.manualGolden, { pass: 50, fail: 40, observationOnly: 0 });
  assert.deepEqual(summary.negativeZeroDecoderBoundary, currentShape.decodedInputAudit.negativeZero);
  assert.equal(summary.focusedTests.result, "PASS");
  assert.deepEqual(summary.remainingFindings, []);
  const manifest = spawnSync("git", ["show", `${executionStartRef}:verification/corpus/semantic/v1/g1-04-c/generated/manifest.json`], { cwd: repoRoot });
  assert.equal(manifest.status, 0);
  assert.equal(summary.fixtureManifest.sha256, createHash("sha256").update(manifest.stdout).digest("hex"));
});
