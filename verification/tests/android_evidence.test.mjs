import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdtemp, readFile, readdir, rm } from "node:fs/promises";
import { resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { generateAndroidAdapterEvidence } from "../tools/generate_android_adapter_evidence.mjs";
import { validateValue } from "../tools/validate_schemas.mjs";

const verificationRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const schema = async (name) => JSON.parse(await readFile(resolve(verificationRoot, `schemas/platform/${name}`), "utf8"));

test("Android adapter evidence contains 28 facts-only observations and hashed manifest", async () => {
  const outputRoot = await mkdtemp(resolve(tmpdir(), "axiom-android-evidence-"));
  try {
    const result = await generateAndroidAdapterEvidence({ outputRoot, sourceCommit: "WORKTREE" });
    assert.equal(result.applicableCount, 28);
    const observations = await readdir(resolve(outputRoot, "observations"));
    const results = await readdir(resolve(outputRoot, "results"));
    assert.equal(observations.length, 28);
    assert.equal(results.length, 28);
    const observation = JSON.parse(await readFile(resolve(outputRoot, "observations", observations[0]), "utf8"));
    assert.equal(observation.platformFamily, "ANDROID");
    assert.equal(Object.hasOwn(observation, "expected"), false);
    const resultFile = JSON.parse(await readFile(resolve(outputRoot, "results", results[0]), "utf8"));
    assert.equal(resultFile.result, "OBSERVED_AGREEMENT_OPEN");
    assert.notEqual(resultFile.result, "PASS");
    const observationSchema = await schema("platform-observation.schema.json");
    const resultSchema = await schema("platform-result.schema.json");
    const profileSchema = await schema("platform-profile.schema.json");
    validateValue(profileSchema, JSON.parse(await readFile(resolve(outputRoot, "profile.json"), "utf8")));
    for (const name of observations) validateValue(observationSchema, JSON.parse(await readFile(resolve(outputRoot, "observations", name), "utf8")));
    for (const name of results) validateValue(resultSchema, JSON.parse(await readFile(resolve(outputRoot, "results", name), "utf8")));
    const summary = JSON.parse(await readFile(resolve(outputRoot, "summary.json"), "utf8"));
    assert.equal(summary.physicalExecution, false);
    const manifest = JSON.parse(await readFile(resolve(outputRoot, "manifest.json"), "utf8"));
    assert.equal(manifest.sourceCommit, "WORKTREE");
    assert.ok(manifest.files.every((entry) => /^[0-9a-f]{64}$/.test(entry.sha256)));
    for (const entry of manifest.files) {
      const bytes = await readFile(resolve(outputRoot, entry.path));
      assert.equal(createHash("sha256").update(bytes).digest("hex"), entry.sha256);
    }
  } finally {
    await rm(outputRoot, { recursive: true, force: true });
  }
});
