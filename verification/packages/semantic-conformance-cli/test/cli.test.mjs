import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const main = fileURLToPath(new URL("../dist/main.js", import.meta.url));
const generator = fileURLToPath(new URL("../../../tools/generate_g1_04_c4_evidence.mjs", import.meta.url));

function inputs(openPolicy = false) {
  const dir = mkdtempSync(join(tmpdir(), "c4-cli-"));
  const id = "CASE";
  const files = {
    case: { format: "axiom-g1-04-c-case-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", id, operationFamily: "x", authorityRuleRefs: [], inputRef: "input", expectedRef: "expected", blocking: true },
    expected: { format: "axiom-g1-04-c-expected-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", caseId: id, authorityRuleRefs: [], mutationExpected: false, disposition: "PLAN_READY", terminalPhase: "PREPARE", ...(openPolicy ? { openPolicy: true } : {}) },
    reference: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "reference", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: {}, afterProjection: {} },
    indexed: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "indexed", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: {}, afterProjection: {} },
  };
  const paths = {};
  for (const [name, value] of Object.entries(files)) { paths[name] = join(dir, `${name}.json`); writeFileSync(paths[name], JSON.stringify(value)); }
  return { dir, paths };
}

function run(data, decision = "UNRESOLVED", extra = []) {
  const args = [main, "compare-case", "--case", data.paths.case, "--expected", data.paths.expected, "--reference", data.paths.reference, "--indexed", data.paths.indexed, "--reference-ref", "ref", "--indexed-ref", "idx", "--open-authority", decision, ...extra];
  return spawnSync(process.execPath, args, { encoding: "utf8" });
}

test("valid closed inputs print exactly one PASS result JSON", () => { const p = run(inputs()); assert.equal(p.status, 0); const lines = p.stdout.trim().split(/\n/); assert.equal(lines.length, 1); assert.equal(JSON.parse(lines[0]).status, "PASS"); });
test("current OPEN prints observation-only result", () => { const p = run(inputs(true), "CURRENT_OPEN"); assert.equal(p.status, 0); assert.equal(JSON.parse(p.stdout).status, "OBSERVATION_ONLY"); });
test("missing args fail deterministically", () => { const p = spawnSync(process.execPath, [main, "compare-case"], { encoding: "utf8" }); assert.equal(p.status, 2); assert.match(p.stderr, /^usage:/); });
test("golden mutation options are forbidden", () => { for (const unsafe of ["--bless", "--update-golden", "--accept-current-output"]) { const p = run(inputs(), "UNRESOLVED", [unsafe, "x"]); assert.equal(p.status, 2); assert.match(p.stderr, /^usage:/); } });
test("evidence generator is deterministic and proves accepted inventory facts", () => { const dir = mkdtempSync(join(tmpdir(), "c4-evidence-")); const a = join(dir, "a.json"); const b = join(dir, "b.json"); for (const out of [a, b]) { const p = spawnSync(process.execPath, [generator, "--source-ref", "SOURCE", "--output", out], { encoding: "utf8" }); assert.equal(p.status, 0, p.stderr); } assert.equal(readFileSync(a, "utf8"), readFileSync(b, "utf8")); const e = JSON.parse(readFileSync(a, "utf8")); assert.equal(e.syntheticScenarioCount, 10); assert.equal(e.syntheticScenarioPassCount, 10); assert.equal(e.acceptedExpectedCount, 90); assert.equal(e.acceptedOpenPolicyCount, 0); assert.equal(e.acceptedExpectedAllAuthorityManual, true); assert.equal(e.expectedTruthWrites, 0); assert.equal(e.productionSemanticDependencies, 0); assert.equal(e.providerOutputUsedAsExpected, false); });
