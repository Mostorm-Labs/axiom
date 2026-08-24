import assert from "node:assert/strict";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { generatePlatformScenarioEvidence } from "../tools/generate_platform_scenario_evidence.mjs";

test("platform scenario evidence contains all four reproducible reports", async () => {
  const outputRoot = await mkdtemp(resolve(tmpdir(), "axiom-platform-evidence-"));
  try {
    const result = await generatePlatformScenarioEvidence({ outputRoot, sourceCommit: "WORKTREE" });
    assert.deepEqual(result.files.map((entry) => entry.name), [
      "case-requirement-capability-matrix.json",
      "fixture-integrity-report.json",
      "scenario-validation-report.json",
      "suite-manifest.json",
    ]);
    for (const entry of result.files) assert.match(entry.sha256, /^[0-9a-f]{64}$/);

    const validation = JSON.parse(await readFile(resolve(outputRoot, "scenario-validation-report.json"), "utf8"));
    assert.equal(validation.status, "PASS");
    assert.equal(validation.scenarios.valid, 28);
    assert.equal(validation.scenarios.expected, 28);

    const manifest = JSON.parse(await readFile(resolve(outputRoot, "suite-manifest.json"), "utf8"));
    assert.equal(manifest.scenarios.length, 28);
    assert.equal(manifest.scenarios[0].id, "PLAT-CREATE-CANVAS-001");

    const matrix = JSON.parse(await readFile(resolve(outputRoot, "case-requirement-capability-matrix.json"), "utf8"));
    assert.equal(matrix.rows.length, 28);
    assert.deepEqual(matrix.rows[0].requirementIds, ["PC-I02", "VER-20"]);

    const fixtures = JSON.parse(await readFile(resolve(outputRoot, "fixture-integrity-report.json"), "utf8"));
    assert.equal(fixtures.status, "PASS");
    assert.deepEqual(fixtures.fixtures.map((entry) => entry.id), [
      "OP-SETTRANSFORMS-VALID-001",
      "POINTER-PEN-MOVE-COALESCED-001",
      "REPLAY-MIXED-OPERATIONS-001",
    ]);
  } finally {
    await rm(outputRoot, { recursive: true, force: true });
  }
});
