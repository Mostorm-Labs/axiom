#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "./validate_platform_scenarios.mjs";

const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const platformRoot = resolve(verificationRoot, "platform/v1");
const fixtureRoot = resolve(platformRoot, "fixtures");

function sha256(value) {
  return createHash("sha256").update(value).digest("hex");
}

function canonical(value) {
  return JSON.stringify(value);
}

function collectStrings(value, result = []) {
  if (typeof value === "string") result.push(value);
  else if (Array.isArray(value)) value.forEach((item) => collectStrings(item, result));
  else if (value && typeof value === "object") Object.values(value).forEach((item) => collectStrings(item, result));
  return result;
}

function fixtureIds(scenarios) {
  const ids = new Set();
  for (const scenario of scenarios) {
    for (const value of collectStrings(scenario)) {
      if (/^(?:REPLAY|OP|POINTER)-[A-Z0-9-]+-\d+$/.test(value)) ids.add(value);
    }
  }
  return [...ids].sort();
}

async function fixturePath(id) {
  for (const category of ["semantic", "input"]) {
    const path = resolve(fixtureRoot, category, `${id}.json`);
    try {
      await readFile(path, "utf8");
      return path;
    } catch (error) {
      if (error.code !== "ENOENT") throw error;
    }
  }
  throw new Error(`fixture does not resolve: ${id}`);
}

async function writeJson(outputRoot, name, value) {
  const body = `${JSON.stringify(value, null, 2)}\n`;
  const path = resolve(outputRoot, name);
  await writeFile(path, body, "utf8");
  return { name, sha256: sha256(body), bytes: Buffer.byteLength(body) };
}

export async function generatePlatformScenarioEvidence({ outputRoot, sourceCommit = "UNKNOWN" } = {}) {
  if (!outputRoot) throw new Error("outputRoot is required");
  await mkdir(outputRoot, { recursive: true });
  const result = await validatePlatformSeed();
  const { suite, scenarios, digest: seedDigest } = result;
  const scenarioById = new Map(scenarios.map((scenario) => [scenario.id, scenario]));
  const scenarioEntries = [];
  for (const ref of suite.scenarioRefs) {
    const id = ref.split("/").at(-2);
    const body = await readFile(resolve(verificationRoot, ref.replace(/^verification\//, "")), "utf8");
    scenarioEntries.push({ id, path: ref, sha256: sha256(body), bytes: Buffer.byteLength(body) });
  }

  const validationReport = {
    format: "axiom-platform-scenario-validation-report-v1",
    formatVersion: 1,
    sourceCommit,
    suiteId: suite.id,
    status: "PASS",
    scenarios: { expected: 28, discovered: scenarios.length, valid: scenarios.length, invalid: 0 },
    seedSha256: seedDigest,
    stableOrder: scenarioEntries.map((entry) => entry.id),
  };

  const manifest = {
    format: "axiom-platform-suite-manifest-v1",
    formatVersion: 1,
    sourceCommit,
    suite: {
      id: suite.id,
      format: suite.format,
      formatVersion: suite.formatVersion,
      requiredScenarioFormatVersion: suite.requiredScenarioFormatVersion,
      requiredRunnerProtocolVersion: suite.requiredRunnerProtocolVersion,
      path: "verification/platform/v1/suites/platform-seed-v0.1.json",
    },
    seedSha256: seedDigest,
    scenarios: scenarioEntries,
  };

  const matrix = {
    format: "axiom-platform-case-requirement-capability-matrix-v1",
    formatVersion: 1,
    sourceCommit,
    suiteId: suite.id,
    rows: scenarios.map((scenario) => ({
      id: scenario.id,
      requirementStatus: scenario.requirementStatus,
      requirementIds: scenario.requirementIds,
      targets: scenario.targets.map((target) => ({
        platformFamily: target.platformFamily,
        policy: target.policy,
        requiredCapabilities: target.requiredCapabilities,
      })),
    })),
  };

  const fixtures = [];
  for (const id of fixtureIds(scenarios)) {
    const path = await fixturePath(id);
    const body = await readFile(path, "utf8");
    const fixture = JSON.parse(body);
    fixtures.push({
      id,
      format: fixture.format,
      path: path.replace(`${verificationRoot}/`, "verification/"),
      sha256: sha256(body),
      bytes: Buffer.byteLength(body),
    });
  }
  const fixtureReport = {
    format: "axiom-platform-fixture-integrity-report-v1",
    formatVersion: 1,
    sourceCommit,
    suiteId: suite.id,
    status: "PASS",
    fixtureCount: fixtures.length,
    fixtures,
  };

  const files = [];
  files.push(await writeJson(outputRoot, "case-requirement-capability-matrix.json", matrix));
  files.push(await writeJson(outputRoot, "fixture-integrity-report.json", fixtureReport));
  files.push(await writeJson(outputRoot, "scenario-validation-report.json", validationReport));
  files.push(await writeJson(outputRoot, "suite-manifest.json", manifest));
  return { files, seedSha256: seedDigest, scenarioCount: scenarios.length, fixtureCount: fixtures.length };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const outputRoot = process.argv[2] ?? resolve(verificationRoot, "evidence/g0/gt-g0-09");
  try {
    const result = await generatePlatformScenarioEvidence({ outputRoot, sourceCommit: process.env.GITHUB_SHA ?? "WORKTREE" });
    console.log(`platform scenario evidence: ${result.scenarioCount}/28 scenarios, ${result.fixtureCount} fixtures`);
    for (const file of result.files) console.log(`${file.name}: ${file.sha256}`);
    console.log(`platform_seed_sha256: ${result.seedSha256}`);
  } catch (error) {
    console.error(`platform scenario evidence failed: ${error.message}`);
    process.exitCode = 1;
  }
}
