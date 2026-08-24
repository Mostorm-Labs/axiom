#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, readdir, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "./validate_platform_scenarios.mjs";
import { WebReferenceAdapter, WEB_PROFILE } from "../packages/platform-harness-web/dist/index.js";

const verificationRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const outputRoot = resolve(verificationRoot, "evidence/g0/gt-g0-10");
const sourceCommit = process.env.AXIOM_EVIDENCE_SOURCE_COMMIT ?? "UNBOUND";

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const writeJson = async (path, value) => {
  await writeFile(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
};

await mkdir(join(outputRoot, "observations"), { recursive: true });
await mkdir(join(outputRoot, "results"), { recursive: true });
const corpus = await validatePlatformSeed({ suiteFile: join(verificationRoot, "platform/v1/suites/platform-seed-v0.1.json") });
const adapter = new WebReferenceAdapter();
const applicable = corpus.scenarios.filter((scenario) => scenario.targets.some((target) => target.platformFamily === "WEB"));
const notApplicable = corpus.scenarios.filter((scenario) => !scenario.targets.some((target) => target.platformFamily === "WEB"));
for (const scenario of applicable) {
  const observation = adapter.execute(scenario);
  await writeJson(join(outputRoot, "observations", `${scenario.id}.json`), observation);
  await writeJson(join(outputRoot, "results", `${scenario.id}.json`), {
    format: "axiom-platform-conformance-result-v1", formatVersion: 1, scenarioId: scenario.id,
    requirementStatus: scenario.requirementStatus, result: "OBSERVED_AGREEMENT_OPEN",
    participants: [{ profileId: WEB_PROFILE.profileId }], checks: [{ kind: "OBSERVATION_CAPTURED", status: "OBSERVED" }],
    openObservations: [{ kind: "COMPARATOR_DEFERRED", reason: "shared runner owns expected comparison" }], divergence: null, diagnostics: [],
  });
}
await writeJson(join(outputRoot, "profile.json"), WEB_PROFILE);
await writeJson(join(outputRoot, "applicability.json"), {
  format: "axiom-platform-web-applicability-v1", adapter: "web",
  notApplicable: notApplicable.map((scenario) => ({ scenarioId: scenario.id, reason: scenario.expected.applicability?.WEB ?? "NOT_APPLICABLE_BY_CONTRACT" })),
});
await writeJson(join(outputRoot, "summary.json"), {
  format: "axiom-platform-web-run-summary-v1", sourceCommit, implementationState: sourceCommit === "UNBOUND" ? "UNBOUND" : "COMMITTED",
  adapter: "web", suite: "platform-seed-v0.1", corpusDigest: corpus.digest,
  applicableCount: applicable.length, notApplicableCount: notApplicable.length, resultCount: corpus.scenarios.length,
  observationPolicy: "facts-only; comparison-owned-by-shared-runner",
});

const files = [];
async function collect(dir, relativeRoot = "") {
  for (const entry of (await readdir(dir, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
    const path = join(dir, entry.name);
    const relativePath = relativeRoot ? `${relativeRoot}/${entry.name}` : entry.name;
    if (entry.isDirectory()) await collect(path, relativePath);
    else if (entry.isFile() && entry.name !== "manifest.json") files.push({ path: relativePath, sha256: sha256(await readFile(path)) });
  }
}
await collect(outputRoot);
await writeJson(join(outputRoot, "manifest.json"), { format: "axiom-platform-web-evidence-manifest-v1", sourceCommit, files });
console.log(`web adapter evidence: ${applicable.length} applicable, ${notApplicable.length} N/A`);
