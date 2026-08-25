export type RequirementStatus = "SPEC_REQUIREMENT" | "FREEZE_CANDIDATE" | "BENCHMARK_TARGET" | "EXPERIMENTAL_TARGET" | "OPEN";

export type PlatformTarget = {
  platformFamily: string;
  policy: "REQUIRED" | "REQUIRED_WHEN_CAPABLE";
  requiredCapabilities: string[];
};

export type PlatformScenario = {
  id: string;
  requirementStatus: RequirementStatus;
  targets: PlatformTarget[];
  expected: {
    requiredEvents?: string[];
    forbiddenEvents?: string[];
    partialOrder?: Array<[string, string]>;
    stateAssertions?: Array<{ selector: string; operator: string; value: unknown }>;
  };
};

export type PlatformObservation = {
  scenarioId: string;
  profileId: string;
  platformFamily: string;
  capabilities: string[];
  steps: Array<Record<string, unknown>>;
  semanticCheckpoints?: Array<Record<string, unknown>>;
  stateCheckpoints?: Array<Record<string, unknown>>;
  targetBindings?: Array<Record<string, unknown>>;
  diagnostics?: unknown[];
};

export type Divergence = {
  category: string;
  path: string;
  expected: unknown;
  actual: unknown;
};

export type PlatformResult = {
  format: "axiom-platform-conformance-result-v1";
  formatVersion: 1;
  scenarioId: string;
  requirementStatus: RequirementStatus;
  result: "PASS" | "FAIL_SCENARIO_EXPECTATION" | "FAIL_CROSS_PLATFORM_DIVERGENCE" | "FAIL_CAPABILITY_MISSING" | "OBSERVED_DIVERGENCE_OPEN" | "HARNESS_ERROR";
  participants: Array<{ profileId: string }>;
  checks: Array<{ kind: string; status: "PASS" | "FAIL" }>;
  openObservations: unknown[];
  divergence: Divergence | null;
  diagnostics: unknown[];
};

function observedEvents(observation: PlatformObservation): string[] {
  return observation.steps.flatMap((step) => {
    if (typeof step.event === "string") return [step.event];
    if (Array.isArray(step.events)) return step.events.filter((event): event is string => typeof event === "string");
    return [];
  });
}

function stateFacts(observation: PlatformObservation): Map<string, unknown> {
  const facts = new Map<string, unknown>();
  for (const checkpoint of observation.stateCheckpoints ?? []) {
    if (typeof checkpoint.selector === "string" && Object.hasOwn(checkpoint, "value")) {
      facts.set(checkpoint.selector, checkpoint.value);
    }
    const state = checkpoint.state;
    if (state && typeof state === "object" && !Array.isArray(state)) {
      for (const [selector, value] of Object.entries(state)) facts.set(selector, value);
    }
  }
  for (const binding of observation.targetBindings ?? []) {
    if (Object.hasOwn(binding, "canonicalOwner")) facts.set("target.canonical.owner", binding.canonicalOwner);
    if (Object.hasOwn(binding, "previewOwner")) facts.set("target.preview.owner", binding.previewOwner);
    if (Object.hasOwn(binding, "distinct")) facts.set("target.canonical.preview", binding.distinct);
  }
  return facts;
}

function assertionMatches(operator: string, expected: unknown, actual: unknown): boolean {
  if (["EQUALS", "EQUALS_FIXTURE", "EQUALS_CANONICAL_FIXTURE"].includes(operator)) return Object.is(actual, expected);
  if (["SUCCEEDED", "NON_REENTRANT", "ADVANCED", "STRICTLY_INCREASES", "DISTINCT"].includes(operator)) {
    return actual === true || (operator === "SUCCEEDED" && actual === "SUCCEEDED");
  }
  return false;
}

function failureResult(
  scenario: PlatformScenario,
  observation: PlatformObservation,
  divergence: Divergence,
): PlatformResult {
  const isOpen = scenario.requirementStatus === "OPEN" ||
    scenario.requirementStatus === "EXPERIMENTAL_TARGET" ||
    scenario.requirementStatus === "BENCHMARK_TARGET";
  return {
    format: "axiom-platform-conformance-result-v1",
    formatVersion: 1,
    scenarioId: scenario.id,
    requirementStatus: scenario.requirementStatus,
    result: isOpen
      ? "OBSERVED_DIVERGENCE_OPEN"
      : divergence.category === "CAPABILITY_MISSING"
        ? "FAIL_CAPABILITY_MISSING"
        : "FAIL_SCENARIO_EXPECTATION",
    participants: [{ profileId: observation.profileId }],
    checks: [{ kind: divergence.category, status: "FAIL" }],
    openObservations: isOpen ? [{ kind: divergence.category, divergence }] : [],
    divergence,
    diagnostics: observation.diagnostics ?? [],
  };
}

export function comparePlatformObservation(
  scenario: PlatformScenario,
  observation: PlatformObservation,
): PlatformResult {
  const target = scenario.targets.find((candidate) => candidate.platformFamily === observation.platformFamily);
  if (scenario.id !== observation.scenarioId || !target) {
    return failureResult(scenario, observation, {
      category: "INPUT_IDENTITY_MISMATCH",
      path: scenario.id !== observation.scenarioId ? "observation.scenarioId" : "observation.platformFamily",
      expected: scenario.id !== observation.scenarioId ? scenario.id : scenario.targets.map(({ platformFamily }) => platformFamily),
      actual: scenario.id !== observation.scenarioId ? observation.scenarioId : observation.platformFamily,
    });
  }

  for (let index = 0; index < target.requiredCapabilities.length; index += 1) {
    const capability = target.requiredCapabilities[index];
    if (!observation.capabilities.includes(capability)) {
      return failureResult(scenario, observation, {
        category: "CAPABILITY_MISSING",
        path: `targets.${observation.platformFamily}.requiredCapabilities[${index}]`,
        expected: capability,
        actual: null,
      });
    }
  }

  const events = observedEvents(observation);
  const requiredEvents = scenario.expected.requiredEvents ?? [];
  const requiredSeen = new Map<string, number>();
  for (let index = 0; index < requiredEvents.length; index += 1) {
    const event = requiredEvents[index];
    const occurrence = requiredSeen.get(event) ?? 0;
    requiredSeen.set(event, occurrence + 1);
    if (events.filter((candidate) => candidate === event).length <= occurrence) {
      return failureResult(scenario, observation, {
        category: "REQUIRED_EVENT_MISSING",
        path: `expected.requiredEvents[${index}]`,
        expected: event,
        actual: null,
      });
    }
  }

  const forbiddenEvents = scenario.expected.forbiddenEvents ?? [];
  for (let index = 0; index < forbiddenEvents.length; index += 1) {
    const event = forbiddenEvents[index];
    if (events.includes(event)) {
      return failureResult(scenario, observation, {
        category: "FORBIDDEN_EVENT_OBSERVED",
        path: `expected.forbiddenEvents[${index}]`,
        expected: null,
        actual: event,
      });
    }
  }

  const partialOrder = scenario.expected.partialOrder ?? [];
  for (let index = 0; index < partialOrder.length; index += 1) {
    const [before, after] = partialOrder[index];
    if (events.indexOf(before) >= events.lastIndexOf(after)) {
      return failureResult(scenario, observation, {
        category: "PARTIAL_ORDER_VIOLATION",
        path: `expected.partialOrder[${index}]`,
        expected: [before, after],
        actual: events,
      });
    }
  }

  const facts = stateFacts(observation);
  const stateAssertions = scenario.expected.stateAssertions ?? [];
  for (let index = 0; index < stateAssertions.length; index += 1) {
    const assertion = stateAssertions[index];
    const actual = facts.get(assertion.selector);
    if (!assertionMatches(assertion.operator, assertion.value, actual)) {
      return failureResult(scenario, observation, {
        category: "STATE_ASSERTION_FAILED",
        path: `expected.stateAssertions[${index}]`,
        expected: assertion,
        actual: actual ?? null,
      });
    }
  }

  return {
    format: "axiom-platform-conformance-result-v1",
    formatVersion: 1,
    scenarioId: scenario.id,
    requirementStatus: scenario.requirementStatus,
    result: "PASS",
    participants: [{ profileId: observation.profileId }],
    checks: [{ kind: "SCENARIO_EXPECTATION", status: "PASS" }],
    openObservations: [],
    divergence: null,
    diagnostics: observation.diagnostics ?? [],
  };
}

function canonical(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (value && typeof value === "object") {
    return `{${Object.entries(value).sort(([left], [right]) => left.localeCompare(right))
      .map(([key, child]) => `${JSON.stringify(key)}:${canonical(child)}`).join(",")}}`;
  }
  return JSON.stringify(value);
}

export function comparePlatformObservations(
  scenario: PlatformScenario,
  observations: PlatformObservation[],
): PlatformResult {
  const ordered = [...observations].sort((left, right) => left.profileId.localeCompare(right.profileId));
  for (const observation of ordered) {
    const individual = comparePlatformObservation(scenario, observation);
    if (individual.result !== "PASS") return individual;
  }
  const reference = ordered[0];
  if (!reference) throw new Error("at least one platform observation is required");
  const referenceById = new Map((reference.semanticCheckpoints ?? []).map((checkpoint) => [String(checkpoint.id), checkpoint]));
  for (let participantIndex = 1; participantIndex < ordered.length; participantIndex += 1) {
    const participant = ordered[participantIndex];
    const checkpoints = new Map((participant.semanticCheckpoints ?? []).map((checkpoint) => [String(checkpoint.id), checkpoint]));
    for (const id of [...referenceById.keys()].sort()) {
      const expected = referenceById.get(id);
      const actual = checkpoints.get(id);
      if (canonical(expected) !== canonical(actual)) {
        const keys = [...new Set([...Object.keys(expected ?? {}), ...Object.keys(actual ?? {})])].sort();
        const key = keys.find((candidate) => canonical(expected?.[candidate]) !== canonical(actual?.[candidate])) ?? "value";
        return {
          format: "axiom-platform-conformance-result-v1", formatVersion: 1,
          scenarioId: scenario.id, requirementStatus: scenario.requirementStatus,
          result: "FAIL_CROSS_PLATFORM_DIVERGENCE",
          participants: ordered.map(({ profileId }) => ({ profileId })),
          checks: [{ kind: "CROSS_PLATFORM_SEMANTIC_CHECKPOINT", status: "FAIL" }],
          openObservations: [],
          divergence: { category: "SEMANTIC_CHECKPOINT_DIVERGENCE", path: `semanticCheckpoints.${id}.${key}`, expected: expected?.[key] ?? null, actual: actual?.[key] ?? null },
          diagnostics: [],
        };
      }
    }
  }
  return { ...comparePlatformObservation(scenario, reference), participants: ordered.map(({ profileId }) => ({ profileId })) };
}
