#!/usr/bin/env node

import { createHash } from "node:crypto";
import { readFile, readdir } from "node:fs/promises";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validateValue } from "./validate_schemas.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const suitePath = resolve(root, "platform/v1/suites/platform-seed-v0.1.json");
const scenarioRoot = resolve(root, "platform/v1/scenarios");
const fixtureRoot = resolve(root, "platform/v1/fixtures");

const expected = [
  ["PLAT-CREATE-CANVAS-001", "SPEC_REQUIREMENT", ["PC-I02", "VER-20"]],
  ["PLAT-HOST-ATTACH-001", "SPEC_REQUIREMENT", ["PC-I02"]],
  ["PLAT-DOCUMENT-ATTACH-001", "SPEC_REQUIREMENT", ["PC-I02"]],
  ["PLAT-CANONICAL-REPLAY-001", "SPEC_REQUIREMENT", ["PC-I01", "VER-07"]],
  ["PLAT-METRICS-RESIZE-001", "SPEC_REQUIREMENT", ["PC-I03"]],
  ["PLAT-METRICS-DPI-SCALE-001", "SPEC_REQUIREMENT", ["PC-I03"]],
  ["PLAT-METRICS-ORIENTATION-001", "SPEC_REQUIREMENT", ["PC-I03"]],
  ["PLAT-VISIBILITY-001", "SPEC_REQUIREMENT", ["PC-I03"]],
  ["PLAT-APP-BACKGROUND-001", "SPEC_REQUIREMENT", ["PC-I09"]],
  ["PLAT-APP-FOREGROUND-001", "SPEC_REQUIREMENT", ["PC-I09"]],
  ["PLAT-CANVAS-SUSPEND-001", "SPEC_REQUIREMENT", ["PC-I10"]],
  ["PLAT-CANVAS-RESUME-001", "SPEC_REQUIREMENT", ["PC-I10"]],
  ["PLAT-SURFACE-LOST-001", "SPEC_REQUIREMENT", ["PC-I05"]],
  ["PLAT-SURFACE-REBIND-001", "SPEC_REQUIREMENT", ["PC-I05", "PC-I06"]],
  ["PLAT-STALE-GENERATION-REJECT-001", "FREEZE_CANDIDATE", ["PC-I04"]],
  ["PLAT-DEVICE-LOST-001", "SPEC_REQUIREMENT", ["PC-I07", "VER-19"]],
  ["PLAT-DEVICE-RECOVER-001", "SPEC_REQUIREMENT", ["PC-I08", "VER-19"]],
  ["PLAT-HOST-DETACH-REATTACH-001", "SPEC_REQUIREMENT", ["PC-I02", "VER-20"]],
  ["PLAT-DATABRIDGE-NO-ECHO-001", "SPEC_REQUIREMENT", ["PC-I18"]],
  ["PLAT-CALLBACK-NONREENTRANT-001", "SPEC_REQUIREMENT", ["PC-I16", "PC-I18"]],
  ["PLAT-INPUT-BATCH-NORMALIZED-001", "SPEC_REQUIREMENT", ["PC-I17", "VER-07"]],
  ["PLAT-INPUT-HOTPATH-001", "SPEC_REQUIREMENT", ["PC-I14", "PC-I17"]],
  ["PLAT-ARC-PREVIEW-FALLBACK-001", "SPEC_REQUIREMENT", ["PC-I12"]],
  ["PLAT-ARC-CANONICAL-HANDOFF-001", "FREEZE_CANDIDATE", ["PC-I13"]],
  ["PLAT-SURFACE-OWNERSHIP-001", "SPEC_REQUIREMENT", ["PC-I11"]],
  ["PLAT-DESTROY-CANVAS-001", "SPEC_REQUIREMENT", ["VER-20"]],
  ["PLAT-DESTROY-STALE-WORK-001", "FREEZE_CANDIDATE", ["PC-I20"]],
  ["PLAT-RECOVERY-REPEATED-001", "SPEC_REQUIREMENT", ["PC-I05", "PC-I06", "PC-I07", "PC-I08"]],
];

const knownCapabilities = new Set([
  "semantic.projection.capture", "surface.generation", "metrics.generation",
  "surface.loss.inject", "device.loss.inject", "presentation.feedback",
  "bridge.public_facade", "bridge.data_bridge", "bridge.callback_trace",
  "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
  "fault.present_completion_hold", "platform.state.capture",
  "surface.ownership.capture", "fault.stale_generation.inject",
  "harness.completion_tokens", "harness.source_lease_registry",
  "harness.late_event_fence", "harness.source_attempt_trace",
]);

const knownAuthorityRefs = new Set([
  "SRC-NOTION-G0-IH09-CAPTURE-20260824",
  "SRC-NOTION-PLATFORM-SCENARIO-FORMAT-V01-CAPTURE-20260824",
  "SRC-NOTION-PLATFORM-SEED-V01-CAPTURE-20260824",
]);

const scenarioCapabilities = new Map([
  ["PLAT-CREATE-CANVAS-001", ["platform.state.capture"]],
  ["PLAT-HOST-ATTACH-001", ["platform.state.capture", "surface.generation"]],
  ["PLAT-DOCUMENT-ATTACH-001", ["platform.state.capture", "semantic.projection.capture"]],
  ["PLAT-CANONICAL-REPLAY-001", ["semantic.projection.capture"]],
  ["PLAT-METRICS-RESIZE-001", ["semantic.projection.capture", "metrics.generation", "platform.state.capture"]],
  ["PLAT-METRICS-DPI-SCALE-001", ["semantic.projection.capture", "metrics.generation", "platform.state.capture"]],
  ["PLAT-METRICS-ORIENTATION-001", ["semantic.projection.capture", "metrics.generation", "platform.state.capture"]],
  ["PLAT-VISIBILITY-001", ["semantic.projection.capture", "metrics.generation", "platform.state.capture"]],
  ["PLAT-APP-BACKGROUND-001", ["semantic.projection.capture", "platform.state.capture"]],
  ["PLAT-APP-FOREGROUND-001", ["semantic.projection.capture", "platform.state.capture", "presentation.feedback"]],
  ["PLAT-CANVAS-SUSPEND-001", ["semantic.projection.capture", "platform.state.capture"]],
  ["PLAT-CANVAS-RESUME-001", ["semantic.projection.capture", "platform.state.capture", "presentation.feedback"]],
  ["PLAT-SURFACE-LOST-001", ["semantic.projection.capture", "platform.state.capture", "surface.generation", "surface.loss.inject"]],
  ["PLAT-SURFACE-REBIND-001", ["semantic.projection.capture", "platform.state.capture", "surface.generation", "surface.loss.inject", "presentation.feedback"]],
  ["PLAT-STALE-GENERATION-REJECT-001", ["semantic.projection.capture", "platform.state.capture", "surface.generation", "fault.stale_generation.inject"]],
  ["PLAT-DEVICE-LOST-001", ["semantic.projection.capture", "platform.state.capture", "device.loss.inject"]],
  ["PLAT-DEVICE-RECOVER-001", ["semantic.projection.capture", "platform.state.capture", "device.loss.inject", "presentation.feedback"]],
  ["PLAT-HOST-DETACH-REATTACH-001", ["semantic.projection.capture", "platform.state.capture", "surface.generation", "presentation.feedback"]],
  ["PLAT-DATABRIDGE-NO-ECHO-001", ["semantic.projection.capture", "platform.state.capture", "bridge.data_bridge", "bridge.callback_trace"]],
  ["PLAT-CALLBACK-NONREENTRANT-001", ["semantic.projection.capture", "platform.state.capture", "bridge.data_bridge", "bridge.callback_trace", "input.pointer_sample_batch"]],
  ["PLAT-INPUT-BATCH-NORMALIZED-001", ["semantic.projection.capture", "platform.state.capture", "input.pointer_sample_batch"]],
  ["PLAT-INPUT-HOTPATH-001", ["semantic.projection.capture", "platform.state.capture", "input.pointer_sample_batch", "presentation.feedback", "fault.present_completion_hold", "bridge.data_bridge", "bridge.public_facade"]],
  ["PLAT-ARC-PREVIEW-FALLBACK-001", ["semantic.projection.capture", "platform.state.capture", "presentation.feedback", "arc.preview", "arc.preview.loss.inject", "surface.ownership.capture"]],
  ["PLAT-ARC-CANONICAL-HANDOFF-001", ["semantic.projection.capture", "platform.state.capture", "presentation.feedback", "arc.preview", "surface.ownership.capture"]],
  ["PLAT-SURFACE-OWNERSHIP-001", ["semantic.projection.capture", "platform.state.capture", "arc.preview", "surface.ownership.capture"]],
  ["PLAT-DESTROY-CANVAS-001", ["semantic.projection.capture", "platform.state.capture"]],
  ["PLAT-DESTROY-STALE-WORK-001", ["semantic.projection.capture", "platform.state.capture", "presentation.feedback", "fault.present_completion_hold", "harness.completion_tokens", "harness.source_lease_registry", "harness.late_event_fence", "harness.source_attempt_trace"]],
  ["PLAT-RECOVERY-REPEATED-001", ["semantic.projection.capture", "platform.state.capture", "surface.generation", "surface.loss.inject", "device.loss.inject", "presentation.feedback"]],
]);

export class PlatformScenarioError extends Error {}

function requireCondition(condition, message) {
  if (!condition) throw new PlatformScenarioError(message);
}

function collectStrings(value, results = []) {
  if (typeof value === "string") results.push(value);
  else if (Array.isArray(value)) value.forEach((entry) => collectStrings(entry, results));
  else if (value && typeof value === "object") Object.values(value).forEach((entry) => collectStrings(entry, results));
  return results;
}

function actionOperations(scenario) {
  return scenario.steps.map((step) => step.action.operation ?? step.action.type).filter(Boolean);
}

function validateRecipe(scenario) {
  const operations = actionOperations(scenario);
  const required = (...values) => values.every((value) => operations.includes(value));
  switch (scenario.id) {
    case "PLAT-CREATE-CANVAS-001":
      requireCondition(operations.length === 1 && operations[0] === "CREATE_CANVAS", `${scenario.id}: must only create canvas`);
      break;
    case "PLAT-HOST-ATTACH-001":
      requireCondition(operations.join(",") === "CREATE_CANVAS,ATTACH_HOST", `${scenario.id}: invalid isolated host recipe`);
      break;
    case "PLAT-DOCUMENT-ATTACH-001":
      requireCondition(!operations.includes("ATTACH_HOST") && required("RESTORE_CANONICAL_FIXTURE", "CREATE_CANVAS", "ATTACH_DOCUMENT"), `${scenario.id}: host must remain detached`);
      break;
    case "PLAT-APP-FOREGROUND-001":
      requireCondition(required("BACKGROUND", "FOREGROUND"), `${scenario.id}: background/foreground cycle missing`);
      break;
    case "PLAT-CANVAS-RESUME-001":
      requireCondition(required("SUSPEND_CANVAS", "RESUME_CANVAS"), `${scenario.id}: suspend/resume cycle missing`);
      break;
    case "PLAT-SURFACE-REBIND-001":
      requireCondition(required("SURFACE_LOST", "PROVIDE_SURFACE_REBIND"), `${scenario.id}: loss/rebind cycle missing`);
      break;
    case "PLAT-DEVICE-RECOVER-001":
      requireCondition(scenario.steps.some((step) => step.kind === "FAULT" && step.action.type === "DEVICE_LOST" && step.action.mode === "ACTIVATE") && scenario.steps.some((step) => step.kind === "FAULT" && step.action.type === "DEVICE_LOST" && step.action.mode === "CLEAR"), `${scenario.id}: device activate/clear missing`);
      break;
    case "PLAT-INPUT-HOTPATH-001":
      requireCondition(scenario.steps.some((step) => step.action.type === "PRESENT_COMPLETION_HELD" && step.action.mode === "ACTIVATE") && scenario.steps.some((step) => step.kind === "INPUT" && step.completion.mode === "DISPATCH_ONLY") && scenario.steps.some((step) => step.action.type === "PRESENT_COMPLETION_HELD" && step.action.mode === "CLEAR"), `${scenario.id}: deterministic hot-path hold recipe missing`);
      break;
    case "PLAT-DESTROY-STALE-WORK-001":
      requireCondition(scenario.steps.some((step) => step.action.operation === "ARM_LATE_EVENT_FENCE") && scenario.steps.some((step) => step.action.operation === "DRAIN_SOURCES"), `${scenario.id}: fence/drain recipe missing`);
      break;
  }
}

async function resolveFixture(ref) {
  const candidates = [
    resolve(fixtureRoot, "semantic", `${ref}.json`),
    resolve(fixtureRoot, "input", `${ref}.json`),
  ];
  for (const candidate of candidates) {
    try { await readFile(candidate, "utf8"); return candidate; } catch (error) { if (error.code !== "ENOENT") throw error; }
  }
  throw new PlatformScenarioError(`fixture does not resolve: ${ref}`);
}

export async function validatePlatformSeed({ suiteFile = suitePath, scenarioFileOverrides = {} } = {}) {
  const suiteSchema = JSON.parse(await readFile(resolve(root, "schemas/platform/platform-suite.schema.json"), "utf8"));
  const scenarioSchema = JSON.parse(await readFile(resolve(root, "schemas/platform/platform-scenario.schema.json"), "utf8"));
  const suite = JSON.parse(await readFile(suiteFile, "utf8"));
  validateValue(suiteSchema, suite);
  requireCondition(suite.id === "platform-seed-v0.1", "suite id mismatch");
  requireCondition(suite.requiredScenarioFormatVersion === 1, "suite scenario format version mismatch");
  requireCondition(suite.requiredRunnerProtocolVersion === 1, "suite runner protocol version mismatch");
  requireCondition(suite.scenarioRefs.length === 28, "suite must contain exactly 28 scenarios");
  requireCondition(new Set(suite.scenarioRefs).size === 28, "suite contains duplicate scenario refs");

  const directories = (await readdir(scenarioRoot, { withFileTypes: true })).filter((entry) => entry.isDirectory()).map((entry) => entry.name).sort();
  requireCondition(directories.length === 28, "scenario directory count must be exactly 28");
  const scenarios = [];
  for (let index = 0; index < expected.length; ++index) {
    const [id, status, requirementIds] = expected[index];
    const ref = suite.scenarioRefs[index];
    requireCondition(ref === `verification/platform/v1/scenarios/${id}/scenario.json`, `${id}: published order/path mismatch`);
    const scenarioPath = scenarioFileOverrides[id] ?? resolve(root, ref.replace(/^verification\//, ""));
    let scenario;
    try {
      scenario = JSON.parse(await readFile(scenarioPath, "utf8"));
    } catch (error) {
      if (error.code === "ENOENT") throw new PlatformScenarioError(`${id}: scenario file does not exist`);
      throw error;
    }
    validateValue(scenarioSchema, scenario);
    requireCondition(scenario.id === id, `${id}: identity mismatch`);
    requireCondition(scenario.requirementStatus === status, `${id}: requirement status mismatch`);
    requireCondition(JSON.stringify(scenario.requirementIds) === JSON.stringify(requirementIds), `${id}: requirement IDs mismatch`);
    for (const authorityRef of scenario.authorityRefs) requireCondition(knownAuthorityRefs.has(authorityRef), `${id}: unknown authority reference ${authorityRef}`);
    requireCondition(new Set(scenario.targets.map((target) => target.platformFamily)).size === scenario.targets.length, `${id}: duplicate platform target`);
    requireCondition(new Set(scenario.steps.map((step) => step.stepId)).size === scenario.steps.length, `${id}: duplicate step ID`);
    requireCondition(Object.keys(scenario.expected).length > 0, `${id}: expected oracle is empty`);
    requireCondition(scenario.preconditions.canvasState !== "RUNNING", `${id}: hidden running precondition is forbidden`);
    requireCondition(!scenario.targets.some((target) => new Set(target.requiredCapabilities).size !== target.requiredCapabilities.length), `${id}: duplicate capability`);
    for (const target of scenario.targets) for (const capability of target.requiredCapabilities) requireCondition(knownCapabilities.has(capability), `${id}: unknown capability ${capability}`);
    const expectedCapabilities = new Set(scenarioCapabilities.get(id));
    for (const target of scenario.targets) {
      for (const capability of target.requiredCapabilities) requireCondition(expectedCapabilities.has(capability), `${id}: capability is not required by this scenario recipe: ${capability}`);
      requireCondition(JSON.stringify([...target.requiredCapabilities].sort()) === JSON.stringify([...expectedCapabilities].sort()), `${id}: required capability set mismatch`);
    }
    const serialized = JSON.stringify(scenario);
    requireCondition(!/(?:web|windows|android|apple)[._-]?expected/i.test(serialized), `${id}: platform-private expected truth is forbidden`);
    requireCondition(!/\b(?:sleep|delayMs|waitMs)\b/i.test(serialized), `${id}: correctness sleep is forbidden`);
    if (id === "PLAT-ARC-PREVIEW-FALLBACK-001" || id === "PLAT-ARC-CANONICAL-HANDOFF-001") {
      requireCondition(!scenario.targets.some((target) => target.platformFamily === "WEB"), `${id}: Arc scenarios must not target Web`);
      requireCondition(scenario.expected.applicability?.WEB === "NOT_APPLICABLE_BY_CONTRACT", `${id}: Arc Web applicability must be explicit`);
    }
    if (id === "PLAT-ARC-CANONICAL-HANDOFF-001") {
      const expectedOrder = [
        ["PREVIEW_FRAME_PRESENTED", "CANONICAL_FRAME_PRESENTED"],
        ["CANONICAL_FRAME_PRESENTED", "CANONICAL_VISIBLE_ACKNOWLEDGED"],
        ["CANONICAL_VISIBLE_ACKNOWLEDGED", "PREVIEW_CLEAR_REQUESTED"],
        ["PREVIEW_CLEAR_REQUESTED", "PREVIEW_CLEARED"],
      ];
      requireCondition(JSON.stringify(scenario.expected.partialOrder) === JSON.stringify(expectedOrder), `${id}: canonical-visible preview-clear order mismatch`);
    }
    const strings = collectStrings(scenario);
    for (const refValue of strings.filter((value) => /^(?:REPLAY|OP|POINTER)-[A-Z0-9-]+-\d+$/.test(value))) await resolveFixture(refValue);
    const stepIds = new Set(scenario.steps.map((step) => step.stepId));
    const checkpointIds = [...(scenario.capture.semanticCheckpoints ?? []), ...(scenario.capture.stateCheckpoints ?? [])].map((checkpoint) => checkpoint.id);
    requireCondition(new Set(checkpointIds).size === checkpointIds.length, `${id}: duplicate checkpoint ID`);
    for (const checkpoint of [...(scenario.capture.semanticCheckpoints ?? []), ...(scenario.capture.stateCheckpoints ?? [])]) requireCondition(stepIds.has(checkpoint.afterStepId), `${id}: checkpoint references unknown step ${checkpoint.afterStepId}`);
    const requiredEvents = new Set(scenario.expected.requiredEvents ?? []);
    for (const relation of scenario.expected.partialOrder ?? []) for (const event of relation) requireCondition(requiredEvents.has(event), `${id}: partial-order event is not declared required: ${event}`);
    for (const stepItem of scenario.steps) if (stepItem.action.generationFromStepId) requireCondition(stepIds.has(stepItem.action.generationFromStepId), `${id}: generation source step missing`);
    validateRecipe(scenario);
    scenarios.push(scenario);
  }
  const canonical = JSON.stringify({ suite, scenarios });
  return { suite, scenarios, digest: createHash("sha256").update(canonical).digest("hex") };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const result = await validatePlatformSeed();
    console.log(`platform scenario validation: ${result.scenarios.length}/28 valid`);
    console.log(`platform_seed_sha256: ${result.digest}`);
  } catch (error) {
    console.error(`platform scenario validation failed: ${error.message}`);
    process.exitCode = 1;
  }
}
