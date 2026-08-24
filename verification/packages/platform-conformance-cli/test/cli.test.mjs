import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdir, mkdtemp, readFile, readdir, writeFile } from "node:fs/promises";
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
  const applicability = JSON.parse(await readFile(join(output, "applicability.json"), "utf8"));
  assert.deepEqual(applicability.notApplicable.map((entry) => entry.scenarioId).sort(), [
    "PLAT-ARC-CANONICAL-HANDOFF-001", "PLAT-ARC-PREVIEW-FALLBACK-001", "PLAT-SURFACE-OWNERSHIP-001",
  ]);
  const observation = JSON.parse(await readFile(join(output, "observations", "PLAT-CREATE-CANVAS-001.json"), "utf8"));
  assert.equal(observation.platformFamily, "WEB");
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
});
