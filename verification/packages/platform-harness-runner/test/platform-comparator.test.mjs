import assert from "node:assert/strict";
import test from "node:test";
import {
  classifyChanges,
  comparePlatformObservation,
  aggregatePrDecision,
  comparePlatformObservations,
} from "../dist/index.js";

const scenario = {
  format: "axiom-platform-scenario-v1",
  formatVersion: 1,
  id: "PLAT-TEST-001",
  requirementStatus: "SPEC_REQUIREMENT",
  targets: [{
    platformFamily: "WEB",
    policy: "REQUIRED",
    requiredCapabilities: ["platform.state.capture"],
  }],
  expected: {
    requiredEvents: ["CANVAS_CREATED", "HOST_ATTACHED"],
    forbiddenEvents: ["CANVAS_DESTROYED"],
    partialOrder: [["CANVAS_CREATED", "HOST_ATTACHED"]],
    stateAssertions: [],
  },
};

function observation(events, capabilities = ["platform.state.capture"]) {
  return {
    format: "axiom-platform-observation-v1",
    formatVersion: 1,
    scenarioId: scenario.id,
    profileId: "web-test",
    platformFamily: "WEB",
    capabilities,
    steps: events.map((event, index) => ({ sequence: index + 1, event })),
    semanticCheckpoints: [],
    stateCheckpoints: [],
    targetBindings: [],
    diagnostics: [],
  };
}

test("shared comparator owns PASS and emits no deferred-comparator observation", () => {
  const result = comparePlatformObservation(scenario, observation(["CANVAS_CREATED", "HOST_ATTACHED"]));
  assert.equal(result.result, "PASS");
  assert.equal(result.divergence, null);
  assert.deepEqual(result.openObservations, []);
});

test("first divergence is deterministic and follows declared required-event order", () => {
  const first = comparePlatformObservation(scenario, observation([]));
  const second = comparePlatformObservation(scenario, observation([]));
  assert.deepEqual(first, second);
  assert.deepEqual(first.divergence, {
    category: "REQUIRED_EVENT_MISSING",
    path: "expected.requiredEvents[0]",
    expected: "CANVAS_CREATED",
    actual: null,
  });
});

test("capability, forbidden event, and partial order failures stay distinguishable", () => {
  assert.equal(comparePlatformObservation(scenario, observation([], [])).divergence.category, "CAPABILITY_MISSING");
  assert.equal(comparePlatformObservation(scenario, observation(["CANVAS_CREATED", "HOST_ATTACHED", "CANVAS_DESTROYED"])).divergence.category, "FORBIDDEN_EVENT_OBSERVED");
  assert.equal(comparePlatformObservation(scenario, observation(["HOST_ATTACHED", "CANVAS_CREATED"])).divergence.category, "PARTIAL_ORDER_VIOLATION");
});

test("required events preserve multiplicity and state assertions are comparator-owned", () => {
  const repeated = {
    ...scenario,
    expected: { ...scenario.expected, requiredEvents: ["HOST_ATTACHED", "HOST_ATTACHED"] },
  };
  assert.equal(comparePlatformObservation(repeated, observation(["HOST_ATTACHED"])).divergence.path, "expected.requiredEvents[1]");
  const withState = {
    ...scenario,
    expected: { ...scenario.expected, stateAssertions: [{ selector: "canvas.state", operator: "EQUALS", value: "CREATED" }] },
  };
  const observed = observation(["CANVAS_CREATED", "HOST_ATTACHED"]);
  observed.stateCheckpoints = [{ selector: "canvas.state", value: "CREATED" }];
  assert.equal(comparePlatformObservation(withState, observed).result, "PASS");
  observed.stateCheckpoints[0].value = "DESTROYED";
  assert.equal(comparePlatformObservation(withState, observed).divergence.category, "STATE_ASSERTION_FAILED");
});

test("cross-platform comparison reports a stable semantic checkpoint divergence", () => {
  const web = observation(["CANVAS_CREATED", "HOST_ATTACHED"]);
  web.semanticCheckpoints = [{ id: "after", digest: "aaa" }];
  const android = { ...structuredClone(web), profileId: "android-test", platformFamily: "ANDROID", semanticCheckpoints: [{ id: "after", digest: "bbb" }] };
  const multiScenario = { ...scenario, targets: [...scenario.targets, { platformFamily: "ANDROID", policy: "REQUIRED", requiredCapabilities: ["platform.state.capture"] }] };
  const result = comparePlatformObservations(multiScenario, [web, android]);
  assert.equal(result.result, "FAIL_CROSS_PLATFORM_DIVERGENCE");
  assert.equal(result.divergence.path, "semanticCheckpoints.after.digest");
});

test("OPEN divergence is an observation, not a correctness PASS", () => {
  const openScenario = { ...scenario, requirementStatus: "OPEN" };
  const result = comparePlatformObservation(openScenario, observation([]));
  assert.equal(result.result, "OBSERVED_DIVERGENCE_OPEN");
  assert.equal(result.openObservations.length, 1);
});

test("change classifier is conservative and provider neutral", () => {
  assert.deepEqual(classifyChanges(["verification/schemas/platform/platform-result.schema.json"]).selectedPlatforms,
    ["web", "windows", "android", "apple"]);
  assert.deepEqual(classifyChanges(["verification/packages/platform-harness-android/src/index.ts"]).selectedPlatforms,
    ["android"]);
  const unknown = classifyChanges(["unclassified/new-boundary.xyz"]);
  assert.deepEqual(unknown.selectedPlatforms, ["web", "windows", "android", "apple"]);
  assert.equal(unknown.selectionReason, "SAFE_ALL_UNKNOWN_PATH");
  assert.equal(classifyChanges([]).selectionReason, "EMPTY_CHANGESET_SAFE_ALL");
});

const passRecord = (layer, attempt = 1) => ({
  format: "axiom-pr-layer-record-v1",
  formatVersion: 1,
  layer,
  subject: layer,
  attempt,
  status: "PASS",
  evidenceSha256: "a".repeat(64),
  diagnostics: [],
});

test("PR aggregation attributes deliberate failures to their own layer", () => {
  for (const layer of ["schema", "protocol", "semantic", "platform"]) {
    const records = ["schema", "protocol", "semantic", "platform"].map((name) => passRecord(name));
    records.find((record) => record.layer === layer).status = layer === "schema" ? "INVALID_EVIDENCE" : "FAIL";
    const decision = aggregatePrDecision(records);
    assert.equal(decision.failedLayer, layer);
    assert.equal(decision.decision, layer === "schema" ? "INVALID_EVIDENCE" : "FAIL");
  }
});

test("PR aggregation preserves retries and refuses missing prerequisites", () => {
  const records = [passRecord("schema"), passRecord("protocol"), passRecord("semantic"), passRecord("platform")];
  records.push({ ...passRecord("semantic", 2), status: "PASS_WITH_OBSERVATIONS" });
  const decision = aggregatePrDecision(records);
  assert.equal(decision.decision, "PASS_WITH_OBSERVATIONS");
  assert.equal(decision.attempts.filter((record) => record.layer === "semantic").length, 2);
  assert.equal(aggregatePrDecision(records.filter((record) => record.layer !== "protocol")).decision, "INVALID_EVIDENCE");
});

test("PR aggregation evaluates every selected platform subject", () => {
  const records = [passRecord("schema"), passRecord("protocol"), passRecord("semantic"),
    { ...passRecord("platform"), subject: "web" },
    { ...passRecord("platform"), subject: "android", status: "FAIL" }];
  const decision = aggregatePrDecision(records);
  assert.equal(decision.decision, "FAIL");
  assert.equal(decision.failedLayer, "platform");
});
