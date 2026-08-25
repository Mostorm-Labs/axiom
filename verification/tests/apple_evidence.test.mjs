import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { dirname, resolve as resolvePath } from "node:path";
import { fileURLToPath } from "node:url";
import { generateAppleAdapterEvidence } from "../tools/generate_apple_adapter_evidence.mjs";
import { validateValue } from "../tools/validate_schemas.mjs";

const verificationRoot = resolvePath(dirname(fileURLToPath(import.meta.url)), "..");
const schema = async (name) => JSON.parse(await readFile(resolvePath(verificationRoot, `schemas/platform/${name}`), "utf8"));

test("Apple Evidence keeps iPhone and iPadOS reports independent and facts-only", async () => {
  const outputRoot = await mkdtemp(resolve(tmpdir(), "axiom-apple-evidence-"));
  try {
    const result = await generateAppleAdapterEvidence({ outputRoot, sourceCommit: "WORKTREE" });
    assert.equal(result.targetCount, 2);
    assert.equal(result.applicableCount, 28);
    const observationSchema = await schema("platform-observation.schema.json");
    const resultSchema = await schema("platform-result.schema.json");
    const profileSchema = await schema("platform-profile.schema.json");
    for (const [directory, deviceClass] of [["ios", "IPHONE"], ["ipados", "IPAD"]]) {
      const observations = await readdir(resolve(outputRoot, directory, "observations"));
      const results = await readdir(resolve(outputRoot, directory, "results"));
      assert.equal(observations.length, 28);
      assert.equal(results.length, 28);
      const profile = JSON.parse(await readFile(resolve(outputRoot, directory, "profile.json"), "utf8"));
      assert.equal(profile.realization.deviceClass, deviceClass);
      validateValue(profileSchema, profile);
      for (const name of observations) {
        const value = JSON.parse(await readFile(resolve(outputRoot, directory, "observations", name), "utf8"));
        validateValue(observationSchema, value);
        assert.equal(Object.hasOwn(value, "expected"), false);
      }
      for (const name of results) validateValue(resultSchema, JSON.parse(await readFile(resolve(outputRoot, directory, "results", name), "utf8")));
    }
    const manifest = JSON.parse(await readFile(resolve(outputRoot, "manifest.json"), "utf8"));
    assert.deepEqual(manifest.targets, ["ios", "ipados"]);
    for (const entry of manifest.files) assert.equal(createHash("sha256").update(await readFile(resolve(outputRoot, entry.path))).digest("hex"), entry.sha256);
  } finally {
    await rm(outputRoot, { recursive: true, force: true });
  }
});

test("Apple Evidence attaches an iPhone physical report without claiming iPadOS", async () => {
  const outputRoot = await mkdtemp(resolve(tmpdir(), "axiom-apple-physical-evidence-"));
  const reportPath = resolve(outputRoot, "iphone-physical-input.json");
  try {
    await writeFile(reportPath, `${JSON.stringify({
      platform: "ios",
      backend: "ganesh-metal",
      digest: "47826449b895ac4f4a57b4f386379775",
      lifecycle: 100,
      smoke_seconds: 60,
      max_frame_ms: 4.80675,
      memory_analysis: { passed: true },
      physical_environment: { reality: "physical", platform: "iOS" },
      visual: { matching_ratio: 0.9998270833333334, maximum_channel_delta: 184 },
    }, null, 2)}\n`, "utf8");
    await generateAppleAdapterEvidence({
      outputRoot,
      sourceCommit: "WORKTREE",
      physicalExecution: true,
      iosPhysicalReportPath: reportPath,
    });
    const iosSummary = JSON.parse(await readFile(resolve(outputRoot, "ios/summary.json"), "utf8"));
    const ipadosSummary = JSON.parse(await readFile(resolve(outputRoot, "ipados/summary.json"), "utf8"));
    assert.equal(iosSummary.physicalExecution, true);
    assert.equal(iosSummary.executionKind, "physical-device");
    assert.equal(ipadosSummary.physicalExecution, false);
    assert.equal(ipadosSummary.executionKind, "logical-reference");
    assert.equal(ipadosSummary.physicalStatus, "PENDING_NO_DEVICE");
    const attached = JSON.parse(await readFile(resolve(outputRoot, "ios/ios-physical-report.json"), "utf8"));
    assert.equal(attached.platform, "ios");
    await assert.rejects(readFile(resolve(outputRoot, "ipados/ipados-physical-report.json"), "utf8"));
  } finally {
    await rm(outputRoot, { recursive: true, force: true });
  }
});

test("Apple Evidence attaches independent iPhone and iPadOS physical reports", async () => {
  const outputRoot = await mkdtemp(resolve(tmpdir(), "axiom-apple-both-physical-evidence-"));
  const iosReportPath = resolve(outputRoot, "iphone-physical-input.json");
  const ipadosReportPath = resolve(outputRoot, "ipad-physical-input.json");
  const report = (platform, systemPlatform, frame) => ({
    platform,
    backend: "ganesh-metal",
    digest: "47826449b895ac4f4a57b4f386379775",
    lifecycle: 100,
    smoke_seconds: 60,
    max_frame_ms: frame,
    memory_analysis: { passed: true },
    physical_environment: { reality: "physical", platform: systemPlatform },
    visual: { matching_ratio: 0.9998270833333334, maximum_channel_delta: 184 },
  });
  try {
    await writeFile(iosReportPath, `${JSON.stringify(report("ios", "iOS", 4.8), null, 2)}\n`, "utf8");
    await writeFile(ipadosReportPath, `${JSON.stringify(report("ipados", "iOS", 5.3), null, 2)}\n`, "utf8");
    await generateAppleAdapterEvidence({
      outputRoot,
      sourceCommit: "WORKTREE",
      physicalExecution: true,
      iosPhysicalReportPath: iosReportPath,
      ipadosPhysicalReportPath: ipadosReportPath,
    });
    const iosSummary = JSON.parse(await readFile(resolve(outputRoot, "ios/summary.json"), "utf8"));
    const ipadosSummary = JSON.parse(await readFile(resolve(outputRoot, "ipados/summary.json"), "utf8"));
    assert.equal(iosSummary.physicalStatus, "PASS");
    assert.equal(ipadosSummary.physicalStatus, "PASS");
    assert.equal(iosSummary.physicalEvidence, "ios/ios-physical-report.json");
    assert.equal(ipadosSummary.physicalEvidence, "ipados/ipados-physical-report.json");
    assert.equal(JSON.parse(await readFile(resolve(outputRoot, "ios/ios-physical-report.json"), "utf8")).platform, "ios");
    assert.equal(JSON.parse(await readFile(resolve(outputRoot, "ipados/ipados-physical-report.json"), "utf8")).platform, "ipados");
  } finally {
    await rm(outputRoot, { recursive: true, force: true });
  }
});
