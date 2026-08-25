import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const cli = new URL("../dist/main.js", import.meta.url);
const run = (args, options = {}) => spawnSync(process.execPath, [cli.pathname, ...args], { encoding: "utf8", ...options });
const protocolArgs = (output, boundaries = ["in-process"]) => ["protocol", "--suite", "protocol-seed-v0.1", ...boundaries.flatMap((boundary) => ["--boundary", boundary]), "--output", output];

test("help, unknown, reserved and forbidden update commands have stable exit codes", () => {
  assert.equal(run(["--help"]).status, 0); assert.equal(run(["unknown"]).status, 2); assert.equal(run(["list"]).status, 30);
  assert.equal(run(["protocol", "--bless"]).status, 2); assert.equal(run(["protocol", "--update-golden"]).status, 2);
});
test("validate succeeds", () => assert.equal(run(["validate"]).status, 0));
test("validate and protocol classify invalid schema or corpus", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-cli-invalid-corpus-")); await mkdir(join(root, "tools"), { recursive: true });
  await writeFile(join(root, "tools", "validate_schemas.mjs"), "process.exitCode = 1;\n"); const env = { ...process.env, AXIOM_VERIFICATION_ROOT: root };
  assert.equal(run(["validate"], { env }).status, 10); assert.equal(run(protocolArgs(join(root, "evidence")), { env }).status, 10);
});
test("protocol creates trusted evidence without mutating corpus", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-cli-")); const result = run(protocolArgs(output, ["in-process", "serialized-loopback"]));
  assert.equal(result.status, 0, result.stdout + result.stderr); const summary = JSON.parse(await readFile(join(output, "summary.json"), "utf8"));
  assert.equal(summary.status, "PASS"); assert.equal(summary.resultCount, 112); assert.equal(summary.corpusHashBefore, summary.corpusHashAfter);
  assert.deepEqual((await readdir(join(output, "protocol-meta-results"))).sort(), ["IN_PROCESS", "OUT_OF_PROCESS"]);
});
test("protocol classifies runner mismatch with its stable exit code", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-cli-mismatch-")); const result = run(protocolArgs(output), { env: { ...process.env, AXIOM_PROTOCOL_MUTATION: "duplicate-completion" } });
  assert.equal(result.status, 21); assert.equal(JSON.parse(await readFile(join(output, "summary.json"), "utf8")).status, "RUNNER_EXPECTATION_MISMATCH");
});
test("protocol classifies missing evidence and does not accept a partial bundle", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-cli-evidence-")); const result = run(protocolArgs(output), { env: { ...process.env, AXIOM_PROTOCOL_EVIDENCE_MUTATION: "missing-integrity" } });
  assert.equal(result.status, 20); assert.equal(JSON.parse(await readFile(join(output, "summary.json"), "utf8")).status, "INVALID_EVIDENCE");
});
test("protocol refuses output inside the versioned corpus", () => {
  const corpus = resolve(fileURLToPath(new URL("../../../platform/protocol/v1", import.meta.url))); assert.equal(run(protocolArgs(corpus)).status, 2);
});

test("web profile reports the browser/WASM realization without Arc", () => {
  const result = run(["profile", "--adapter", "web"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const profile = JSON.parse(result.stdout);
  assert.equal(profile.platformFamily, "WEB");
  assert.equal(profile.realization.runtime, "WASM");
  assert.equal(profile.capabilities.includes("arc.preview"), false);
});

test("windows profile reports native D3D12 and Arc realization", () => {
  const result = run(["profile", "--adapter", "windows"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const profile = JSON.parse(result.stdout);
  assert.equal(profile.platformFamily, "WINDOWS");
  assert.equal(profile.realization.host, "WIN32_NATIVE");
  assert.equal(profile.realization.backend, "D3D12");
  assert.equal(profile.capabilities.includes("arc.preview"), true);
});

test("android profile reports Activity/View/JNI, GLES3 and Arc realization", () => {
  const result = run(["profile", "--adapter", "android"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const profile = JSON.parse(result.stdout);
  assert.equal(profile.platformFamily, "ANDROID");
  assert.equal(profile.realization.host, "ACTIVITY_VIEW_JNI");
  assert.equal(profile.realization.backend, "GLES3");
  assert.equal(profile.capabilities.includes("input.pointer_sample_batch"), true);
});

test("Apple profiles report separate iPhone and iPadOS RN/ObjC++ Metal realizations", () => {
  const ios = run(["profile", "--adapter", "ios"]);
  const ipados = run(["profile", "--adapter", "ipados"]);
  assert.equal(ios.status, 0, ios.stdout + ios.stderr);
  assert.equal(ipados.status, 0, ipados.stdout + ipados.stderr);
  const iphone = JSON.parse(ios.stdout);
  const ipad = JSON.parse(ipados.stdout);
  assert.equal(iphone.platformFamily, "APPLE");
  assert.equal(iphone.realization.deviceClass, "IPHONE");
  assert.equal(ipad.realization.deviceClass, "IPAD");
  assert.notEqual(iphone.profileId, ipad.profileId);
});

test("web run uses the shared seed and emits applicability plus observation facts", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-web-adapter-"));
  const result = run(["run", "--suite", "platform-seed-v0.1", "--adapter", "web", "--output", output]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const summary = JSON.parse(await readFile(join(output, "summary.json"), "utf8"));
  assert.equal(summary.adapter, "web");
  assert.equal(summary.applicableCount, 25);
  assert.equal(summary.notApplicableCount, 3);
  assert.equal(summary.resultCount, 28);
  assert.ok((await readdir(join(output, "observations"))).length === 25);
  await assert.rejects(() => readdir(join(output, "results")), /ENOENT/);
  const applicability = JSON.parse(await readFile(join(output, "applicability.json"), "utf8"));
  assert.deepEqual(applicability.notApplicable.map((entry) => entry.scenarioId).sort(), [
    "PLAT-ARC-CANONICAL-HANDOFF-001", "PLAT-ARC-PREVIEW-FALLBACK-001", "PLAT-SURFACE-OWNERSHIP-001",
  ]);
  const observation = JSON.parse(await readFile(join(output, "observations", "PLAT-CREATE-CANVAS-001.json"), "utf8"));
  assert.equal(observation.platformFamily, "WEB");
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
});

test("android run consumes all 28 shared scenarios and emits facts only", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-android-adapter-"));
  const result = run(["run", "--suite", "platform-seed-v0.1", "--adapter", "android", "--output", output]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const summary = JSON.parse(await readFile(join(output, "summary.json"), "utf8"));
  assert.equal(summary.adapter, "android");
  assert.equal(summary.applicableCount, 28);
  assert.equal(summary.notApplicableCount, 0);
  assert.equal(summary.resultCount, 28);
  assert.equal((await readdir(join(output, "observations"))).length, 28);
  const observation = JSON.parse(await readFile(join(output, "observations", "PLAT-INPUT-BATCH-NORMALIZED-001.json"), "utf8"));
  assert.equal(observation.platformFamily, "ANDROID");
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
});

for (const adapter of ["ios", "ipados"]) {
  test(`${adapter} run keeps 28 shared scenarios facts-only`, async () => {
    const output = await mkdtemp(join(tmpdir(), `axiom-${adapter}-adapter-`));
    const result = run(["run", "--suite", "platform-seed-v0.1", "--adapter", adapter, "--output", output]);
    assert.equal(result.status, 0, result.stdout + result.stderr);
    const summary = JSON.parse(await readFile(join(output, "summary.json"), "utf8"));
    assert.equal(summary.adapter, adapter);
    assert.equal(summary.applicableCount, 28);
    assert.equal((await readdir(join(output, "observations"))).length, 28);
    const observation = JSON.parse(await readFile(join(output, "observations", "PLAT-INPUT-BATCH-NORMALIZED-001.json"), "utf8"));
    assert.equal(observation.platformFamily, "APPLE");
    assert.equal(Object.hasOwn(observation, "expected"), false);
    assert.equal(Object.hasOwn(observation, "result"), false);
  });
}

test("compare is runner-owned and writes deterministic first-divergence results", async () => {
  const runOutput = await mkdtemp(join(tmpdir(), "axiom-compare-run-"));
  const compareOutput = await mkdtemp(join(tmpdir(), "axiom-compare-results-"));
  assert.equal(run(["run", "--suite", "platform-seed-v0.1", "--adapter", "web", "--output", runOutput]).status, 0);
  const compared = run(["compare", "--suite", "platform-seed-v0.1", "--observations", join(runOutput, "observations"), "--output", compareOutput]);
  assert.equal(compared.status, 21);
  const result = JSON.parse(await readFile(join(compareOutput, "PLAT-HOST-ATTACH-001.json"), "utf8"));
  assert.equal(result.divergence.category, "REQUIRED_EVENT_MISSING");
  assert.notEqual(result.openObservations?.[0]?.kind, "COMPARATOR_DEFERRED");
});

test("aggregate emits provider-neutral PR decision and rejects missing layers", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-pr-aggregate-"));
  const records = join(root, "records"); await mkdir(records);
  for (const layer of ["schema", "protocol", "semantic", "platform"]) {
    await writeFile(join(records, `${layer}.json`), JSON.stringify({
      format: "axiom-pr-layer-record-v1", formatVersion: 1, layer, subject: layer, attempt: 1,
      status: "PASS", evidenceSha256: "a".repeat(64), diagnostics: [],
    }));
  }
  const output = join(root, "decision.json");
  assert.equal(run(["aggregate", "--records", records, "--output", output]).status, 0);
  assert.equal(JSON.parse(await readFile(output, "utf8")).decision, "PASS");
  await rm(join(records, "protocol.json"));
  assert.equal(run(["aggregate", "--records", records, "--output", output]).status, 20);
});

test("classify produces a conservative provider-neutral run set", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-pr-classify-"));
  const changed = join(root, "changed.txt"); const output = join(root, "run-set.json");
  await writeFile(changed, "verification/packages/platform-harness-android/src/index.ts\n");
  assert.equal(run(["classify", "--changed-paths", changed, "--output", output]).status, 0);
  assert.deepEqual(JSON.parse(await readFile(output, "utf8")).selectedPlatforms, ["android"]);
});

test("full-run-set emits deterministic five-profile release identity", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-full-run-set-"));
  const output = join(root, "run-set.json");
  const args = ["full-run-set", "--cadence", "release", "--source-commit", "a".repeat(40), "--schema-sha256", "b".repeat(64), "--corpus-sha256", "c".repeat(64), "--runner-version", "0.1.0", "--runtime-version", "0.1.0", "--repeat", "1", "--seed", "17", "--output", output];
  assert.equal(run(args).status, 0);
  const value = JSON.parse(await readFile(output, "utf8"));
  assert.equal(value.authority, "G0_WIRING_ONLY");
  assert.deepEqual(value.requiredProfiles.map(({ profileKey }) => profileKey), ["web", "windows", "android", "ios", "ipados"]);
  assert.equal(value.requiredProfiles.every(({ requiredReality }) => requiredReality === "PHYSICAL"), true);
});

test("aggregate-full returns blocked authority when release records are hosted", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-full-aggregate-"));
  const runSetPath = join(root, "run-set.json");
  const recordsPath = join(root, "records");
  const indexPath = join(root, "index.json");
  const output = join(root, "decision.json");
  await mkdir(recordsPath);
  const sourceCommit = "a".repeat(40);
  const runSet = { format: "axiom-full-run-set-v1", formatVersion: 1, authority: "G0_WIRING_ONLY", cadence: "RELEASE", sourceCommit, schemaSha256: "b".repeat(64), corpusSha256: "c".repeat(64), runnerVersion: "0.1.0", runtimeVersion: "0.1.0", repeat: 1, seed: 17, requiredProfiles: [
    ["web", "WEB", "web-reference-v0-1"], ["windows", "WINDOWS", "windows-native-reference-v0-1"], ["android", "ANDROID", "android-instrumentation-reference-v0-1"], ["ios", "APPLE", "ios-rn-objcxx-reference-v0-1"], ["ipados", "APPLE", "ipados-rn-objcxx-reference-v0-1"],
  ].map(([profileKey, platformFamily, profileId]) => ({ profileKey, platformFamily, profileId, requiredReality: "PHYSICAL" })) };
  await writeFile(runSetPath, JSON.stringify(runSet));
  for (const subject of ["schema", "protocol", "semantic", "web", "windows", "android", "ios", "ipados"]) {
    const profile = { web: ["WEB", "web-reference-v0-1"], windows: ["WINDOWS", "windows-native-reference-v0-1"], android: ["ANDROID", "android-instrumentation-reference-v0-1"], ios: ["APPLE", "ios-rn-objcxx-reference-v0-1"], ipados: ["APPLE", "ipados-rn-objcxx-reference-v0-1"] }[subject];
    await writeFile(join(recordsPath, `${subject}.json`), JSON.stringify({ format: "axiom-platform-evidence-record-v1", formatVersion: 1, subject, category: ["schema", "protocol", "semantic"].includes(subject) ? "PREREQUISITE" : "PROFILE", platformFamily: profile?.[0] ?? null, profileId: profile?.[1] ?? null, sourceCommit, corpusSha256: "c".repeat(64), runnerVersion: "0.1.0", runtimeVersion: "0.1.0", reality: profile ? "HOSTED" : "NOT_APPLICABLE", status: "PASS", evidenceSha256: "d".repeat(64), pgStatuses: [], diagnostics: [], environment: {} }));
  }
  assert.equal(run(["aggregate-full", "--run-set", runSetPath, "--records", recordsPath, "--output", output]).status, 21);
  const decision = JSON.parse(await readFile(output, "utf8"));
  assert.equal(decision.decision, "BLOCKED_AUTHORITY");
  await writeFile(indexPath, JSON.stringify({}));
});

test("compare-full rejects different source revisions", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-full-compare-"));
  const left = join(root, "left.json"); const right = join(root, "right.json"); const output = join(root, "comparison.json");
  const decision = (sourceCommit) => ({ format: "axiom-platform-release-decision-v1", formatVersion: 1, authority: "G0_WIRING_ONLY", cadence: "NIGHTLY", decision: "PASS", failedSubject: null, blockingReasons: [], identity: { sourceCommit, schemaSha256: "b".repeat(64), corpusSha256: "c".repeat(64), runnerVersion: "0.1.0", runtimeVersion: "0.1.0", repeat: 1, seed: 1 }, pgStatuses: [], evidenceSubjects: [] });
  await writeFile(left, JSON.stringify(decision("a".repeat(40)))); await writeFile(right, JSON.stringify(decision("e".repeat(40))));
  assert.equal(run(["compare-full", "--left", left, "--right", right, "--output", output]).status, 20);
  assert.equal(JSON.parse(await readFile(output, "utf8")).status, "INVALID_EVIDENCE");
});

test("gate-report aggregates commit-bound G0 lineage and blocks physical promotion", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-g0-gate-report-"));
  const lineage = join(root, "lineage.json"); const hosted = join(root, "hosted.json"); const artifacts = join(root, "artifacts.json"); const output = join(root, "report.json");
  const hash = "b".repeat(64); const sourceCommit = "a".repeat(40); const evidenceBytes = Buffer.from("evidence");
  const evidenceHash = createHash("sha256").update(evidenceBytes).digest("hex");
  await mkdir(join(root, "evidence"));
  for (let i = 0; i < 16; i += 1) await writeFile(join(root, "evidence", `${i}.json`), evidenceBytes);
  const artifactBytes = Buffer.from("artifact"); const artifactHash = createHash("sha256").update(artifactBytes).digest("hex");
  await writeFile(join(root, "artifact.json"), artifactBytes);
  await writeFile(lineage, JSON.stringify({ corpus: { schemaSha256: hash, corpusSha256: hash, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" }, platforms: [{ subject: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", reality: "HOSTED", evidencePath: "evidence/0.json", environment: {} }], tasks: Array.from({ length: 16 }, (_, i) => ({ taskId: `GT-G0-${String(i).padStart(2, "0")}`, status: "Pass", evidencePath: `evidence/${i}.json`, evidenceSha256: evidenceHash })) }));
  await writeFile(hosted, JSON.stringify({ nightlyDecision: "PASS", releaseDecision: "BLOCKED_AUTHORITY", reproducibility: "PASS" }));
  await writeFile(artifacts, JSON.stringify([{ path: "artifact.json", bytes: artifactBytes.byteLength, sha256: artifactHash }]));
  const result = run(["gate-report", "--source-commit", sourceCommit, "--branch", "main", "--lineage", lineage, "--hosted", hosted, "--artifacts", artifacts, "--repository-root", root, "--output", output]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.equal(JSON.parse(await readFile(output, "utf8")).status, "BLOCKED");
});

test("gate-report rejects drifted evidence and artifacts", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-g0-gate-integrity-"));
  const lineage = join(root, "lineage.json"); const hosted = join(root, "hosted.json"); const artifacts = join(root, "artifacts.json"); const output = join(root, "report.json");
  const hash = "b".repeat(64); const sourceCommit = "a".repeat(40); const evidence = join(root, "evidence.json"); const artifact = join(root, "artifact.json");
  await writeFile(evidence, "actual-evidence"); await writeFile(artifact, "actual-artifact");
  const taskLineage = Array.from({ length: 16 }, (_, i) => ({ taskId: `GT-G0-${String(i).padStart(2, "0")}`, status: "Pass", evidencePath: "evidence.json", evidenceSha256: hash }));
  await writeFile(lineage, JSON.stringify({ corpus: { schemaSha256: hash, corpusSha256: hash, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" }, platforms: [{ subject: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", reality: "HOSTED", evidencePath: "evidence.json", environment: {} }], tasks: taskLineage }));
  await writeFile(hosted, JSON.stringify({ nightlyDecision: "PASS", releaseDecision: "BLOCKED_AUTHORITY", reproducibility: "PASS" }));
  await writeFile(artifacts, JSON.stringify([{ path: "artifact.json", bytes: 14, sha256: hash }]));
  const result = run(["gate-report", "--source-commit", "a".repeat(40), "--branch", "main", "--lineage", lineage, "--hosted", hosted, "--artifacts", artifacts, "--repository-root", root, "--output", output]);
  assert.equal(result.status, 20);
  assert.match(result.stderr, /evidence hash mismatch/);

  const validEvidenceHash = createHash("sha256").update("actual-evidence").digest("hex");
  taskLineage.forEach((task) => { task.evidenceSha256 = validEvidenceHash; });
  await writeFile(lineage, JSON.stringify({ corpus: { schemaSha256: hash, corpusSha256: hash, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" }, platforms: [{ subject: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", reality: "HOSTED", evidencePath: "evidence.json", environment: {} }], tasks: taskLineage }));
  await writeFile(artifacts, JSON.stringify([{ path: "artifact.json", bytes: 999, sha256: createHash("sha256").update("actual-artifact").digest("hex") }]));
  const bytes = run(["gate-report", "--source-commit", sourceCommit, "--branch", "main", "--lineage", lineage, "--hosted", hosted, "--artifacts", artifacts, "--repository-root", root, "--output", output]);
  assert.equal(bytes.status, 20);
  assert.match(bytes.stderr, /byte count mismatch/);
});

test("gate-report rejects missing files and path traversal", async () => {
  const root = await mkdtemp(join(tmpdir(), "axiom-g0-gate-paths-"));
  const lineage = join(root, "lineage.json"); const hosted = join(root, "hosted.json"); const artifacts = join(root, "artifacts.json"); const output = join(root, "report.json");
  const hash = "b".repeat(64);
  await writeFile(join(root, "evidence.json"), "platform-evidence");
  const base = { corpus: { schemaSha256: hash, corpusSha256: hash, runnerVersion: "0.1.0", runtimeVersion: "0.1.0" }, platforms: [{ subject: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", reality: "HOSTED", evidencePath: "evidence.json", environment: {} }], tasks: Array.from({ length: 16 }, (_, i) => ({ taskId: `GT-G0-${String(i).padStart(2, "0")}`, status: "Pass", evidencePath: "missing.json", evidenceSha256: hash })) };
  await writeFile(lineage, JSON.stringify(base)); await writeFile(hosted, JSON.stringify({ nightlyDecision: "PASS", releaseDecision: "BLOCKED_AUTHORITY", reproducibility: "PASS" })); await writeFile(artifacts, "[]");
  const args = ["gate-report", "--source-commit", "a".repeat(40), "--branch", "main", "--lineage", lineage, "--hosted", hosted, "--artifacts", artifacts, "--repository-root", root, "--output", output];
  const missing = run(args); assert.equal(missing.status, 20); assert.match(missing.stderr, /file is missing/);
  base.tasks[0].evidencePath = "../outside.json"; base.tasks[1].evidencePath = "evidence.json"; base.tasks[1].evidenceSha256 = createHash("sha256").update("platform-evidence").digest("hex"); await writeFile(lineage, JSON.stringify(base));
  const traversal = run(args); assert.equal(traversal.status, 20); assert.match(traversal.stderr, /unsafe relative path/);
});
