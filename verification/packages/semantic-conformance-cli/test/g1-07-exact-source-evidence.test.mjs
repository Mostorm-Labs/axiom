import assert from "node:assert/strict";
import test from "node:test";
import {
  EXECUTION_REF, PACKAGE_REF, REQUIRED_EVIDENCE_FILES, REPAIR_BASE, TASK_ANCHOR, WORKFLOW_NAME, WORKFLOW_PATH,
  assertSafeOutputDirectory, validateEvidenceInventory, validateFacts, validateRepositoryIdentity,
} from "../../../tools/generate_g1_07_evidence.mjs";

const SHA = "a".repeat(40);
const PARENT = "b".repeat(40);
const facts = (overrides = {}) => ({
  format: "axiom-gt-g1-07-facts-v1", taskId: "GT-G1-07", packageRef: PACKAGE_REF,
  repository: "Mostorm-Labs/axiom", taskAnchor: TASK_ANCHOR, actualStartingRevision: REPAIR_BASE,
  sourceRef: SHA, sourceCommitParent: PARENT,
  implementationSteps: { completed: 16, total: 16 }, acceptanceStatus: "PASS", cleanCheckout: "PASS",
  sourceDelta: { paths: [
    "runtime/semantic/tools/g1_07_replay_inspector.hpp", "runtime/semantic/tools/g1_07_replay_inspector.cpp",
    "runtime/semantic/tools/g1_07_replay_corpus.hpp", "runtime/semantic/tools/g1_07_replay_corpus.cpp",
    "runtime/semantic/tools/g1_07_replay_inspector_main.cpp", "runtime/semantic/tests/g1_07_replay_inspector_test.cpp",
    "runtime/semantic/tests/g1_07_replay_inspector_cli_test.cpp", WORKFLOW_PATH,
    "runtime/semantic/tools/CMakeLists.txt", "runtime/semantic/tests/CMakeLists.txt",
    "runtime/semantic/include/canvas/semantic/codec.hpp",
    "verification/tools/generate_g1_07_evidence.mjs",
    "verification/packages/semantic-conformance-cli/test/g1-07-exact-source-evidence.test.mjs",
  ]},
  semantic: { projectionPrimary: true, digestSecondaryOnly: true, digestOnlyPassAssertions: 0 },
  cliProcess: {
    processLevel: true, allCommandsCovered: true, exit2Covered: true, exit3Covered: true,
    commands: [
      { command: "run", scenario: "valid", provider: "reference", configuration: "protobuf-on", exitCode: 0, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "step", scenario: "boundary-error", provider: "reference", configuration: "protobuf-on", exitCode: 2, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "seek", scenario: "valid", provider: "reference", configuration: "protobuf-on", exitCode: 0, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "object", scenario: "valid", provider: "reference", configuration: "protobuf-on", exitCode: 0, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "projection", scenario: "valid", provider: "reference", configuration: "protobuf-on", exitCode: 0, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "run", scenario: "argument-error", provider: "reference", configuration: "protobuf-on", exitCode: 2, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
      { command: "run", scenario: "semantic-error", provider: "reference", configuration: "protobuf-on", exitCode: 3, stdoutSha256: "a".repeat(64), stderrSha256: "b".repeat(64), stdoutDocumentCount: 1, stdoutEndsWithNewline: true, stdoutSchemaValid: true, deterministicRepeat: true, independentOracle: true, status: "PASS" },
    ],
  },
  ci: { workflowName: WORKFLOW_NAME, workflowPath: WORKFLOW_PATH, sourceRef: SHA, checkoutSha: SHA, event: "push", ref: `refs/heads/${EXECUTION_REF}`, runId: "123", runAttempt: "1", hostedRunUrl: "https://github.com/Mostorm-Labs/axiom/actions/runs/123", artifactName: `gt-g1-07-${SHA}` },
  observations: { step: "PASS", seek: "PASS", observation: "PASS", determinism: "PASS", negative: "PASS" },
  regressionTests: {
    protobufOn: { total: 485, passed: 485, skipped: 0, failed: 0, skipIds: [] },
    protobufOff: { total: 485, passed: 462, skipped: 23, failed: 0, skipIds: [...Array.from({ length: 12 }, (_, i) => i + 25), ...Array.from({ length: 11 }, (_, i) => i + 53)] },
    focused: { protobufOn: { total: 16, passed: 16, skipped: 0, failed: 0 }, protobufOff: { total: 16, passed: 16, skipped: 0, failed: 0 } },
    components: {
      g1_06: { protobufOn: { total: 62, passed: 62, skipped: 0, failed: 0 }, protobufOff: { total: 62, passed: 39, skipped: 23, failed: 0 } },
      g1_05: { protobufOn: { total: 64, passed: 64, skipped: 0, failed: 0 }, protobufOff: { total: 64, passed: 64, skipped: 0, failed: 0 } },
    },
    fullSemantic: "protobuf-on and protobuf-off exact-source suites",
  },
  replay: {
    operationCount: 15,
    orderedOperationFamilies: ["InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms", "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent"],
    singleReplayCoordinatorInvocation: true,
    checkpoints: [0, 5, 10, 15],
    omittedValidExternalSuffixClaimed: false,
  },
  stepMatrix: { applied: 15, alreadyApplied: 0, rejected: 0, commitBlocked: 0, firstFailureIndex: null, firstFailureOperationId: null },
  seek: { positions: [0, 1, 5, 10, 15], projectionParity: "5/5", digestParity: "5/5" },
  observationsDetail: { objectFound: true, objectMissing: true, readOnly: true, lifecycle: "READY", generation: 15, commitOrdinal: 15 },
  negativeMatrix: { malformed: "PASS", invalidPosition: "PASS", rejectedSuffix: "PASS", commitBlocked: "PASS", snapshotBootstrapFailure: "PASS" },
  acceptanceMatrix: [
    { id: "single-step-canonical-engine", test: "AppliedStepExposesCanonicalCommitChangeSetAndIdentityNamespaces", evidence: "G1-07-STEP.json" },
    { id: "whole-route-canonical-replay", test: "AllOperationFamilyCorpusReplaysThroughSingleCanonicalRoute", evidence: "G1-07-TRACE.json" },
    { id: "seek-exact-prefix", test: "SeekCoversBoundaryAndLongCorpusPositionsAcrossProviders", evidence: "G1-07-SEEK.json" },
    { id: "object-inspection-read-only", test: "ObjectFoundAndMissingQueriesAreReadOnlyAndProviderEquivalent", evidence: "G1-07-OBSERVATION.json" },
    { id: "projection-primary", test: "ProvidersHaveProjectionParity", evidence: "G1-07-PROJECTION.json" },
    { id: "deterministic-cli", test: "EveryCommandUsesOneDeterministicSchema", evidence: "G1-07-DETERMINISM.json" },
    { id: "negative-boundaries", test: "TraceParserFailsClosedForStructuralAndMalformedInputs", evidence: "G1-07-NEGATIVE.json" },
  ],
  ...overrides,
});

test("accepts complete exact-source facts", () => assert.doesNotThrow(() => validateFacts(facts(), { sourceRef: SHA })));
test("rejects package, repository, and source substitutions", () => {
  assert.throws(() => validateFacts(facts({ packageRef: "notion://foreign" }), { sourceRef: SHA }));
  assert.throws(() => validateRepositoryIdentity("https://github.com/foreign/repo"));
  assert.throws(() => validateFacts(facts({ sourceRef: "c".repeat(40) }), { sourceRef: SHA }));
});
test("rejects incomplete steps, oracle drift, and forbidden source paths", () => {
  assert.throws(() => validateFacts(facts({ implementationSteps: { completed: 15, total: 16 } }), { sourceRef: SHA }));
  assert.throws(() => validateFacts(facts({ semantic: { projectionPrimary: false, digestSecondaryOnly: true, digestOnlyPassAssertions: 1 } }), { sourceRef: SHA }));
  assert.throws(() => validateFacts(facts({ sourceDelta: { paths: ["runtime/semantic/src/replay.cpp"] } }), { sourceRef: SHA }));
});
test("requires complete replay, step, seek, observation, and negative matrices", () => {
  assert.throws(() => validateFacts(facts({ replay: undefined })), /replay/i);
  assert.throws(() => validateFacts(facts({ stepMatrix: { applied: 15 } })), /step/i);
  assert.throws(() => validateFacts(facts({ seek: { positions: [0] } })), /seek/i);
  assert.throws(() => validateFacts(facts({ observationsDetail: { readOnly: false } })), /observation/i);
  assert.throws(() => validateFacts(facts({ negativeMatrix: { malformed: "PASS" } })), /negative/i);
});
test("rejects incomplete verification accounting, CI provenance, or acceptance traceability", () => {
  assert.throws(() => validateFacts(facts({ regressionTests: { protobufOn: { total: 485, passed: 484, skipped: 0, failed: 1, skipIds: [] } } })), /test|protobuf/i);
  assert.throws(() => validateFacts(facts({ ci: { ...facts().ci, checkoutSha: PARENT } })), /CI/i);
  assert.throws(() => validateFacts(facts({ acceptanceMatrix: [] })), /acceptance|traceability/i);
});
test("declares the exact twelve evidence inventory names", () => {
  assert.equal(REQUIRED_EVIDENCE_FILES.length, 12);
  assert.equal(EXECUTION_REF, "codex/gt-g1-07-rebind-v2");
  assert.equal(new Set(REQUIRED_EVIDENCE_FILES).size, 12);
});
test("rejects output escape", () => assert.throws(() => assertSafeOutputDirectory("../../outside"), /escapes|relative|staging/i));
