import { readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { coordinateCase } from "../packages/semantic-conformance-cli/dist/coordinator.js";

const PACKAGE_REF = "6f19852e6589af60a37a1ea36b18b321e4a00543";
const TASK_ANCHOR = "855c114f36e4d4d4b9db9faaa28b96ae6d5249c6";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function parseArgs(argv) {
  const result = {};
  for (let i = 0; i < argv.length; i += 2) {
    const key = argv[i];
    const value = argv[i + 1];
    if (!["--source-ref", "--output"].includes(key) || !value) throw new Error("invalid arguments");
    result[key] = value;
  }
  if (!result["--source-ref"] || !result["--output"]) throw new Error("missing arguments");
  return result;
}

function base({ openPolicy = false } = {}) {
  const id = "SYNTHETIC";
  return {
    caseIntent: { format: "axiom-g1-04-c-case-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", id, operationFamily: "synthetic", authorityRuleRefs: ["C4-CONTRACT"], inputRef: "synthetic-input", expectedRef: "synthetic-expected", blocking: true },
    expected: { format: "axiom-g1-04-c-expected-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", caseId: id, authorityRuleRefs: ["C4-CONTRACT"], mutationExpected: false, disposition: "PLAN_READY", terminalPhase: "PREPARE", ...(openPolicy ? { openPolicy: true } : {}) },
    reference: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "reference", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: { stable: true }, afterProjection: { stable: true } },
    indexed: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "indexed", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: { stable: true }, afterProjection: { stable: true } },
    referenceRef: "synthetic-reference", indexedRef: "synthetic-indexed", openAuthorityDecision: "UNRESOLVED"
  };
}

function scenario(id, mutate, expectedStatus, diagnostics = []) {
  const input = mutate(base());
  const result = coordinateCase(input);
  const contractPass = result.status === expectedStatus && diagnostics.every((d) => result.diagnostics?.includes(d));
  return { id, expectedStatus, requiredDiagnostics: diagnostics, observedStatus: result.status, observedDiagnostics: result.diagnostics ?? [], contractPass };
}

function resultSchemaCompatible(result) {
  return result.format === "axiom-g1-04-c-result-v1"
    && result.formatVersion === 1
    && result.provenance === "CONFORMANCE_RESULT"
    && typeof result.caseId === "string"
    && result.caseId.length > 0
    && typeof result.status === "string"
    && result.status.length > 0
    && typeof result.expectedRef === "string"
    && result.expectedRef.length > 0
    && Array.isArray(result.observationRefs)
    && result.observationRefs.length >= 1
    && result.observationRefs.every((ref) => typeof ref === "string" && ref.length > 0)
    && new Set(result.observationRefs).size === result.observationRefs.length;
}

function observationRefBoundaryScenario(id, mutate, expectedStatus, diagnostics = [], expectedRefs = undefined) {
  const input = mutate(base());
  const result = coordinateCase(input);
  const refsMatch = expectedRefs === undefined || JSON.stringify(result.observationRefs) === JSON.stringify(expectedRefs);
  const contractPass = result.status === expectedStatus
    && diagnostics.every((d) => result.diagnostics?.includes(d))
    && refsMatch
    && resultSchemaCompatible(result);
  return { id, expectedStatus, requiredDiagnostics: diagnostics, expectedObservationRefs: expectedRefs, observedStatus: result.status, observedDiagnostics: result.diagnostics ?? [], observedObservationRefs: result.observationRefs, resultSchemaCompatible: resultSchemaCompatible(result), contractPass };
}

function scenarios() {
  return [
    scenario("C4-T01", x => x, "PASS"),
    scenario("C4-T02", x => { x.reference.observedDisposition = "REJECTED"; x.indexed.observedDisposition = "REJECTED"; return x; }, "FAIL", ["REFERENCE_GOLDEN_MISMATCH", "INDEXED_GOLDEN_MISMATCH"]),
    scenario("C4-T03", x => { x.indexed.observedDisposition = "REJECTED"; return x; }, "FAIL", ["INDEXED_GOLDEN_MISMATCH", "PROVIDER_DIVERGENCE"]),
    scenario("C4-T04", x => { x.reference.afterProjection = { stable: false }; return x; }, "FAIL", ["REFERENCE_MUTATION"]),
    scenario("C4-T05", x => { x.indexed.afterProjection = { stable: false }; return x; }, "FAIL", ["INDEXED_MUTATION"]),
    scenario("C4-T06", x => { x.expected.provenance = "IMPLEMENTATION_OBSERVATION"; return x; }, "FAIL", ["EXPECTED_PROVENANCE_INVALID"]),
    scenario("C4-T07", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_OPEN"; return x; }, "OBSERVATION_ONLY", ["OPEN_POLICY_OBSERVATION_ONLY"]),
    scenario("C4-T08", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_CLOSED"; return x; }, "FAIL", ["OPEN_POLICY_STALE_CLOSED"]),
    scenario("C4-T09", () => base({ openPolicy: true }), "FAIL", ["OPEN_AUTHORITY_UNRESOLVED"]),
    scenario("C4-T10", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_OPEN"; x.reference.afterProjection = { stable: false }; return x; }, "FAIL", ["REFERENCE_MUTATION"]),
  ];
}

function observationRefBoundaryScenarios() {
  return [
    observationRefBoundaryScenario("C4-R01-DISTINCT-OBSERVATION-REFS", x => x, "PASS", [], ["synthetic-reference", "synthetic-indexed"]),
    observationRefBoundaryScenario("C4-R02-DUPLICATE-OBSERVATION-REFS", x => { x.referenceRef = "same"; x.indexedRef = "same"; return x; }, "FAIL", ["PROVIDER_SET_INVALID"], ["same"]),
    observationRefBoundaryScenario("C4-R03-MISSING-REFERENCE-REF", x => { x.referenceRef = ""; return x; }, "FAIL", ["PROVIDER_SET_INVALID"], ["synthetic-indexed"]),
    observationRefBoundaryScenario("C4-R04-MISSING-INDEXED-REF", x => { x.indexedRef = ""; return x; }, "FAIL", ["PROVIDER_SET_INVALID"], ["synthetic-reference"]),
    observationRefBoundaryScenario("C4-R05-MISSING-ALL-OBSERVATION-REFS", x => { x.referenceRef = ""; x.indexedRef = ""; return x; }, "FAIL", ["PROVIDER_SET_INVALID"], ["INVALID_OBSERVATION_REF"]),
  ];
}

export function generateEvidence(sourceRef) {
  const expectedPath = resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/authoring/expected.json");
  const expected = JSON.parse(readFileSync(expectedPath, "utf8"));
  if (!Array.isArray(expected)) throw new Error("accepted expected inventory must be an array");
  const scenarioResults = scenarios();
  const observationRefBoundaryResults = observationRefBoundaryScenarios();
  return {
    format: "axiom-gt-g1-04-c-coordinator-contract-v1",
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    syntheticScenarioCount: scenarioResults.length,
    syntheticScenarioPassCount: scenarioResults.filter(x => x.contractPass).length,
    observationRefBoundaryScenarioCount: observationRefBoundaryResults.length,
    observationRefBoundaryScenarioPassCount: observationRefBoundaryResults.filter(x => x.contractPass).length,
    acceptedExpectedCount: expected.length,
    acceptedOpenPolicyCount: expected.filter(x => x.openPolicy === true).length,
    acceptedExpectedAllAuthorityManual: expected.every(x => x.provenance === "AUTHORITY_MANUAL"),
    expectedTruthWrites: 0,
    productionSemanticDependencies: 0,
    providerOutputUsedAsExpected: false,
    verification: scenarioResults,
    observationRefBoundaryVerification: observationRefBoundaryResults,
  };
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const evidence = generateEvidence(args["--source-ref"]);
  writeFileSync(args["--output"], `${JSON.stringify(evidence, null, 2)}\n`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
