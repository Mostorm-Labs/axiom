import assert from "node:assert/strict";
import { mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validatePlatformSeed } from "../tools/validate_platform_scenarios.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const suitePath = resolve(root, "platform/v1/suites/platform-seed-v0.1.json");

async function assertScenarioMutation(id, mutate, expectedError) {
  const tempRoot = await mkdtemp(resolve(tmpdir(), "axiom-platform-scenario-"));
  try {
    const source = resolve(root, `platform/v1/scenarios/${id}/scenario.json`);
    const scenario = JSON.parse(await readFile(source, "utf8"));
    mutate(scenario);
    const mutatedPath = resolve(tempRoot, `${id}.json`);
    await writeFile(mutatedPath, JSON.stringify(scenario));
    await assert.rejects(
      () => validatePlatformSeed({ scenarioFileOverrides: { [id]: mutatedPath } }),
      expectedError,
    );
  } finally {
    await rm(tempRoot, { recursive: true, force: true });
  }
}

test("platform-seed-v0.1 materializes exactly 28 stable scenarios", async () => {
  const suite = JSON.parse(await readFile(suitePath, "utf8"));
  assert.equal(suite.id, "platform-seed-v0.1");
  assert.equal(suite.format, "axiom-platform-suite-v1");
  assert.equal(suite.formatVersion, 1);
  assert.equal(suite.requiredScenarioFormatVersion, 1);
  assert.equal(suite.requiredRunnerProtocolVersion, 1);
  assert.equal(suite.scenarioRefs.length, 28);
  assert.equal(new Set(suite.scenarioRefs).size, 28);
  for (const ref of suite.scenarioRefs) {
    const scenario = JSON.parse(await readFile(resolve(root, ref.replace(/^verification\//, "")), "utf8"));
    assert.match(scenario.id, /^PLAT-[A-Z0-9-]+-001$/);
    assert.equal(scenario.format, "axiom-platform-scenario-v1");
    assert.equal(scenario.formatVersion, 1);
    assert.ok(scenario.requirementIds.length > 0);
    assert.ok(scenario.authorityRefs.length > 0);
    assert.ok(scenario.targets.length > 0);
    assert.ok(scenario.steps.length > 0);
    assert.ok(Object.keys(scenario.expected).length > 0);
    assert.ok(Object.keys(scenario.capture).length > 0);
  }
});

test("platform seed has one shared scenario file and no platform-specific expected copies", async () => {
  const scenariosRoot = resolve(root, "platform/v1/scenarios");
  const directories = await readdir(scenariosRoot, { withFileTypes: true });
  assert.equal(directories.filter((entry) => entry.isDirectory()).length, 28);
  const names = directories.flatMap((entry) => entry.isDirectory() ? [entry.name] : []);
  for (const name of names) {
    const files = await readdir(resolve(scenariosRoot, name));
    assert.deepEqual(files, ["scenario.json"]);
  }
});

test("platform seed encodes shared semantic fixtures and explicit lifecycle boundaries", async () => {
  const suite = JSON.parse(await readFile(suitePath, "utf8"));
  const scenarios = await Promise.all(suite.scenarioRefs.map(async (ref) =>
    JSON.parse(await readFile(resolve(root, ref.replace(/^verification\//, "")), "utf8"))));
  const byId = new Map(scenarios.map((scenario) => [scenario.id, scenario]));
  assert.equal(byId.get("PLAT-CANONICAL-REPLAY-001").canonicalFixtureRef, "REPLAY-MIXED-OPERATIONS-001");
  assert.equal(byId.get("PLAT-DESTROY-STALE-WORK-001").expected.destroyAfterPresentHold, true);
  assert.deepEqual(byId.get("PLAT-DESTROY-STALE-WORK-001").expected.forbiddenEvents, [
    "CANONICAL_FRAME_PRESENTED",
    "CALLBACK_DISPATCHED",
  ]);
  for (const id of [
    "PLAT-ARC-PREVIEW-FALLBACK-001",
    "PLAT-ARC-CANONICAL-HANDOFF-001",
    "PLAT-SURFACE-OWNERSHIP-001",
  ]) {
    assert.equal(byId.get(id).targets.some((target) => target.platformFamily === "WEB"), false);
    assert.equal(byId.get(id).expected.applicability.WEB, "NOT_APPLICABLE_BY_CONTRACT");
  }
  assert.deepEqual(byId.get("PLAT-ARC-CANONICAL-HANDOFF-001").expected.partialOrder, [
    ["PREVIEW_FRAME_PRESENTED", "CANONICAL_FRAME_PRESENTED"],
    ["CANONICAL_FRAME_PRESENTED", "CANONICAL_VISIBLE_ACKNOWLEDGED"],
    ["CANONICAL_VISIBLE_ACKNOWLEDGED", "PREVIEW_CLEAR_REQUESTED"],
    ["PREVIEW_CLEAR_REQUESTED", "PREVIEW_CLEARED"],
  ]);
});

test("platform seed passes schema and semantic validation", async () => {
  const result = await validatePlatformSeed();
  assert.equal(result.scenarios.length, 28);
  assert.match(result.digest, /^[0-9a-f]{64}$/);
});

test("duplicate scenario references are rejected", async () => {
  const tempRoot = await mkdtemp(resolve(tmpdir(), "axiom-platform-suite-"));
  try {
    const suite = JSON.parse(await readFile(suitePath, "utf8"));
    suite.scenarioRefs[1] = suite.scenarioRefs[0];
    const mutatedPath = resolve(tempRoot, "suite.json");
    await writeFile(mutatedPath, JSON.stringify(suite));
    await assert.rejects(() => validatePlatformSeed({ suiteFile: mutatedPath }), /duplicate (?:items|scenario refs)/);
  } finally {
    await rm(tempRoot, { recursive: true, force: true });
  }
});

test("missing scenario files are rejected", async () => {
  const tempRoot = await mkdtemp(resolve(tmpdir(), "axiom-platform-missing-"));
  try {
    await assert.rejects(
      () => validatePlatformSeed({ scenarioFileOverrides: { "PLAT-CREATE-CANVAS-001": resolve(tempRoot, "missing.json") } }),
      /scenario file does not exist/,
    );
  } finally {
    await rm(tempRoot, { recursive: true, force: true });
  }
});

test("empty SPEC oracle is rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => { scenario.expected = {}; }, /expected oracle is empty/);
});

test("unknown capabilities are rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.targets[0].requiredCapabilities.push("harness.unknown_capability");
  }, /unknown capability/);
});

test("capabilities unrelated to a scenario recipe are rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.targets[0].requiredCapabilities.push("device.loss.inject");
  }, /capability is not required by this scenario recipe/);
});

test("missing required capabilities are rejected", async () => {
  await assertScenarioMutation("PLAT-HOST-ATTACH-001", (scenario) => {
    scenario.targets[0].requiredCapabilities = ["platform.state.capture"];
  }, /required capability set mismatch/);
});

test("unknown or private authority references are rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.authorityRefs = ["https://app.notion.com/p/private-page"];
  }, /unknown authority reference/);
});

test("duplicate platform targets are rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.targets.push(structuredClone(scenario.targets[0]));
  }, /duplicate platform target/);
});

test("broken fixture references are rejected", async () => {
  await assertScenarioMutation("PLAT-CANONICAL-REPLAY-001", (scenario) => {
    scenario.canonicalFixtureRef = "REPLAY-MISSING-001";
  }, /fixture does not resolve/);
});

test("broken checkpoint references are rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.capture.stateCheckpoints[0].afterStepId = "missing-step";
  }, /checkpoint references unknown step/);
});

test("duplicate checkpoint IDs are rejected", async () => {
  await assertScenarioMutation("PLAT-CANONICAL-REPLAY-001", (scenario) => {
    scenario.capture.stateCheckpoints[0].id = scenario.capture.semanticCheckpoints[0].id;
  }, /duplicate checkpoint ID/);
});

test("partial-order references outside required events are rejected", async () => {
  await assertScenarioMutation("PLAT-HOST-ATTACH-001", (scenario) => {
    scenario.expected.partialOrder.push(["HOST_ATTACHED", "UNDECLARED_EVENT"]);
  }, /partial-order event is not declared required/);
});

test("broken generation source references are rejected", async () => {
  await assertScenarioMutation("PLAT-STALE-GENERATION-REJECT-001", (scenario) => {
    const fault = scenario.steps.find((step) => step.action.generationFromStepId);
    fault.action.generationFromStepId = "missing-step";
  }, /generation source step missing/);
});

test("hidden RUNNING preconditions are rejected", async () => {
  await assertScenarioMutation("PLAT-CANONICAL-REPLAY-001", (scenario) => {
    scenario.preconditions.canvasState = "RUNNING";
  }, /hidden running precondition/);
});

test("platform-private expected truth is rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.expected.windowsExpected = { status: "pass" };
  }, /platform-private expected truth/);
});

test("correctness sleep is rejected", async () => {
  await assertScenarioMutation("PLAT-CREATE-CANVAS-001", (scenario) => {
    scenario.steps[0].action.sleep = 1;
  }, /correctness sleep/);
});

test("Arc scenarios reject a Web target", async () => {
  await assertScenarioMutation("PLAT-ARC-PREVIEW-FALLBACK-001", (scenario) => {
    scenario.targets.push({ platformFamily: "WEB", policy: "REQUIRED", requiredCapabilities: [...scenario.targets[0].requiredCapabilities] });
  }, /Arc scenarios must not target Web/);
});
