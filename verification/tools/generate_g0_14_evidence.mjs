#!/usr/bin/env node

import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { cp, mkdir, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { aggregatePrDecision, classifyChanges } from "../packages/platform-harness-runner/dist/index.js";

const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repositoryRoot = resolve(verificationRoot, "..");
const output = resolve(verificationRoot, "evidence/g0/gt-g0-14");
const semanticInput = process.env.AXIOM_G0_14_SEMANTIC_SUMMARY;
const hostedRunsInput = process.env.AXIOM_G0_14_HOSTED_RUNS;
const sourceCommit = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "WORKTREE";
if (!semanticInput) throw new Error("AXIOM_G0_14_SEMANTIC_SUMMARY is required");

const sha256 = (value) => createHash("sha256").update(value).digest("hex");
const writeJson = (path, value) => writeFile(path, `${JSON.stringify(value, null, 2)}\n`);
const record = (layer, subject, status) => ({
  format: "axiom-pr-layer-record-v1", formatVersion: 1, layer, subject, attempt: 1,
  status, evidenceSha256: sha256(`${layer}:${subject}:${status}`), diagnostics: [],
});

function validateHostedRuns(value) {
  if (value.format !== "axiom-gt-g0-14-hosted-runs-v1" || value.formatVersion !== 1) {
    throw new Error("hosted runs input has an unsupported format");
  }
  if (value.implementationCommit !== sourceCommit) {
    throw new Error("hosted runs implementation commit does not match Evidence source commit");
  }
  const validateRun = (run, expectedConclusion) => {
    if (run.headCommit !== sourceCommit || run.conclusion !== expectedConclusion) {
      throw new Error(`hosted run ${run.runId} does not match the expected commit/conclusion`);
    }
    if (!/^https:\/\/github\.com\/Mostorm-Labs\/axiom\/actions\/runs\/[0-9]+$/u.test(run.url)) {
      throw new Error(`hosted run ${run.runId} has an invalid URL`);
    }
    if (!/^sha256:[0-9a-f]{64}$/u.test(run.artifact.digest)) {
      throw new Error(`hosted run ${run.runId} has an invalid artifact digest`);
    }
  };
  validateRun(value.normalRun, "success");
  if (value.normalRun.decision !== "PASS" || value.normalRun.failedLayer !== null) {
    throw new Error("normal hosted run did not produce PASS");
  }
  const expected = new Map([
    ["schema", "INVALID_EVIDENCE"],
    ["protocol", "FAIL"],
    ["semantic", "FAIL"],
    ["platform", "FAIL"],
  ]);
  if (!Array.isArray(value.deliberateFailureRuns) || value.deliberateFailureRuns.length !== expected.size) {
    throw new Error("hosted runs input must contain four deliberate failures");
  }
  for (const run of value.deliberateFailureRuns) {
    validateRun(run, "failure");
    if (run.failedLayer !== run.injectedLayer || run.decision !== expected.get(run.injectedLayer)) {
      throw new Error(`hosted deliberate failure ${run.runId} was attributed incorrectly`);
    }
    expected.delete(run.injectedLayer);
  }
  if (expected.size !== 0) throw new Error("hosted runs input has duplicate deliberate failure layers");
}

await rm(output, { recursive: true, force: true });
await mkdir(join(output, "failure-attribution"), { recursive: true });
let changedPaths = execFileSync("git", ["diff", "--name-only", "HEAD"], { cwd: repositoryRoot, encoding: "utf8" })
  .split(/\r?\n/u).filter(Boolean);
if (changedPaths.length === 0) {
  changedPaths = execFileSync("git", ["diff-tree", "--no-commit-id", "--name-only", "-r", "HEAD"], { cwd: repositoryRoot, encoding: "utf8" })
    .split(/\r?\n/u).filter(Boolean);
}
const runSet = classifyChanges(changedPaths);
await writeJson(join(output, "run-set.json"), runSet);

const semantic = JSON.parse(await readFile(resolve(semanticInput), "utf8"));
if (semantic.status !== "PASS" || semantic.authority !== "GT-G0-14_BOOTSTRAP_ONLY") {
  throw new Error("semantic bootstrap input is not a passing GT-G0-14 bootstrap summary");
}
await cp(resolve(semanticInput), join(output, "semantic-bootstrap-summary.json"));

let hostedRuns;
if (hostedRunsInput) {
  hostedRuns = JSON.parse(await readFile(resolve(hostedRunsInput), "utf8"));
  validateHostedRuns(hostedRuns);
  await writeJson(join(output, "hosted-runs.json"), hostedRuns);
}

const cases = {
  "schema-failure": [record("schema", "schema", "INVALID_EVIDENCE")],
  "protocol-failure": [record("schema", "schema", "PASS"), record("protocol", "protocol-seed-v0.1", "FAIL")],
  "semantic-failure": [record("schema", "schema", "PASS"), record("protocol", "protocol-seed-v0.1", "PASS"), record("semantic", "g0-bootstrap", "FAIL")],
  "platform-failure": [record("schema", "schema", "PASS"), record("protocol", "protocol-seed-v0.1", "PASS"), record("semantic", "g0-bootstrap", "PASS"), record("platform", "web", "PASS"), record("platform", "android", "FAIL")],
  "all-pass": [record("schema", "schema", "PASS"), record("protocol", "protocol-seed-v0.1", "PASS"), record("semantic", "g0-bootstrap", "PASS"), record("platform", "web", "PASS")],
};
for (const [name, records] of Object.entries(cases)) {
  await writeJson(join(output, "failure-attribution", `${name}.json`), aggregatePrDecision(records));
}

await writeJson(join(output, "summary.json"), {
  format: "axiom-gt-g0-14-evidence-summary-v1", formatVersion: 1,
  taskId: "GT-G0-14", sourceCommit,
  status: sourceCommit === "WORKTREE"
    ? "WORKTREE_VALIDATED"
    : hostedRuns ? "COMMIT_BOUND_HOSTED_VALIDATED" : "COMMIT_BOUND_VALIDATED",
  workflow: ".github/workflows/g0-pr-ci-dag.yml",
  hostedValidation: hostedRuns ? "PASS" : "PENDING",
  hostedRunCount: hostedRuns ? 1 + hostedRuns.deliberateFailureRuns.length : 0,
  selectedPlatforms: runSet.selectedPlatforms,
  deliberateFailureLayers: ["schema", "protocol", "semantic", "platform"],
  semanticAuthority: semantic.authority,
  limitations: [
    ...(hostedRuns ? [] : ["HOSTED_PR_DAG_PENDING"]),
    "NOT_G1_SEMANTIC_ACCEPTANCE",
    "NOT_G0_GATE_REPORT",
  ],
});

const files = [];
async function collect(directory, prefix = "") {
  for (const entry of (await readdir(directory, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
    if (entry.name === "manifest.json") continue;
    const path = join(directory, entry.name);
    const relative = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory()) await collect(path, relative);
    else files.push({ path: relative, sha256: sha256(await readFile(path)) });
  }
}
await collect(output);
await writeJson(join(output, "manifest.json"), {
  format: "axiom-gt-g0-14-evidence-manifest-v1", sourceCommit, files,
});
console.log(`GT-G0-14 evidence: ${files.length} files; ${sourceCommit}`);
