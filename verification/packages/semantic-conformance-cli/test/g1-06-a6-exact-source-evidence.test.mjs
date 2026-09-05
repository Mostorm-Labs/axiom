import assert from "node:assert/strict";
import { join } from "node:path";
import test from "node:test";

import {
  A5_MATERIALIZED_REF,
  A5_PACKAGE_REF,
  A5_SOURCE_REF,
  BRANCH_REF,
  EXPECTED_PROTOBUF_OFF_SKIP_IDS,
  INHERITED_V01_PACKAGE_REF,
  INHERITED_V01_SOURCE_PATHS,
  INHERITED_V01_SOURCE_REF,
  ORIGINAL_A6_ANCHOR,
  PACKAGE_REF,
  PRODUCTION_REPAIR_PATH,
  REQUIRED_EVIDENCE_FILES,
  TASK_ANCHOR,
  WORKFLOW_NAME,
  WORKFLOW_PATH,
  assertSafeOutputDirectory,
  validateA5Provenance,
  validateAncestry,
  validateCiIdentity,
  validateEvidenceInventory,
  validateFacts,
  validateOracleContract,
  validateInheritedV01SourceDelta,
  validateInheritedV01Identity,
  validateActualStartingRevision,
  validateProductionRepairPatch,
  validateSemanticAccounting,
  validateSourceDelta,
  validateSourceRef,
} from "../../../tools/generate_g1_06_a6_evidence.mjs";

const SOURCE_REF = "a".repeat(40);
const SOURCE_PARENT = "b".repeat(40);
const RUN_ID = "123456789";

function ci(overrides = {}) {
  return {
    runId: RUN_ID,
    runAttempt: "1",
    event: "push",
    ref: BRANCH_REF,
    headSha: SOURCE_REF,
    checkoutSha: SOURCE_REF,
    workflowName: WORKFLOW_NAME,
    workflowPath: WORKFLOW_PATH,
    workflowRef: `${BRANCH_REF}/${WORKFLOW_PATH}@${SOURCE_REF}`,
    hostedRunUrl: `https://github.com/Mostorm-Labs/axiom/actions/runs/${RUN_ID}`,
    artifactName: `gt-g1-06-a6-${SOURCE_REF}`,
    ...overrides,
  };
}

function suite(total, passed, skipped = 0, skipIds = []) {
  return { status: "PASS", total, executed: passed, passed, skipped, failed: 0, skipIds };
}

function facts(overrides = {}) {
  return {
    format: "axiom-gt-g1-06-a6-facts-v1",
    taskId: "GT-G1-06-A6",
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    actualStartingRevision: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    sourceCommitParent: SOURCE_PARENT,
    repository: "Mostorm-Labs/axiom",
    inheritedV01: {
      packageRef: INHERITED_V01_PACKAGE_REF,
      originalAnchor: ORIGINAL_A6_ANCHOR,
      sourceRef: INHERITED_V01_SOURCE_REF,
      sourceDelta: INHERITED_V01_SOURCE_PATHS,
      status: "INHERITED_VALID",
    },
    productionRepair: "SEMANTIC_NO_OP_WARNING_REPAIR",
    ci: ci(),
    a5AcceptedPredecessor: {
      status: "ACCEPTED_FOR_DOWNSTREAM",
      packageRef: A5_PACKAGE_REF,
      sourceRef: A5_SOURCE_REF,
      materializedRef: A5_MATERIALIZED_REF,
      evidenceFiles: REQUIRED_EVIDENCE_FILES,
    },
    focused: {
      protobufOn: {
        integration: suite(7, 7),
        operationConformance: suite(11, 11),
        g1_06: suite(62, 62),
        g1_05: suite(80, 80),
      },
      protobufOff: {
        integration: suite(7, 7),
        operationConformance: suite(11, 11),
        g1_06: suite(62, 62),
        g1_05: suite(80, 80),
      },
    },
    fullSemantic: suite(469, 469),
    fullSemanticOff: suite(469, 446, 23, EXPECTED_PROTOBUF_OFF_SKIP_IDS),
    correctnessContract: { primary: "canonical_semantic_projection", digestRole: "secondary_diagnostic_only", digestOnlyPassAssertions: 0 },
    requiredEvidenceFiles: REQUIRED_EVIDENCE_FILES,
    evidenceContractTest: "PASS",
    gitDiffCheck: "PASS",
    cleanCheckoutReproduction: "PASS",
    ...overrides,
  };
}

test("accepts a complete exact-source A6 facts envelope", () => {
  assert.doesNotThrow(() => validateFacts(facts(), {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    actualStartingRevision: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    sourceCommitParent: SOURCE_PARENT,
  }));
});

test("rejects malformed_or_foreign_source_ref", () => {
  assert.throws(() => validateSourceRef("not-a-sha"), /source_ref|SHA/i);
  assert.throws(() => validateFacts(facts({ sourceRef: "c".repeat(40) }), {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    actualStartingRevision: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    sourceCommitParent: SOURCE_PARENT,
  }), /immutable identity|foreign/i);
});

test("rejects old v0.1 package or retry-anchor substitution", () => {
  assert.throws(() => validateFacts(facts({ packageRef: INHERITED_V01_PACKAGE_REF }), {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    actualStartingRevision: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    sourceCommitParent: SOURCE_PARENT,
  }), /immutable identity|foreign/i);
  assert.throws(() => validateFacts(facts({ taskAnchor: ORIGINAL_A6_ANCHOR }), {
    packageRef: PACKAGE_REF,
    taskAnchor: TASK_ANCHOR,
    actualStartingRevision: TASK_ANCHOR,
    sourceRef: SOURCE_REF,
    sourceCommitParent: SOURCE_PARENT,
  }), /immutable identity|foreign/i);
});

test("rejects task_anchor_ancestry_failure", () => {
  assert.throws(() => validateAncestry({
    packageMaterializationRef: TASK_ANCHOR,
    taskAnchor: "not-a-sha",
    sourceRef: TASK_ANCHOR,
    sourceCommitParent: "1".repeat(40),
  }), /SHA|resolvable/i);
});

test("accepts an inherited v0.1 three-file source segment", () => {
  assert.doesNotThrow(() => validateInheritedV01SourceDelta([
    ["A", ".github/workflows/g1-06-exact-source.yml"],
    ["A", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["A", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
  ]));
});

test("rejects inherited segment identity or ancestry delta", () => {
  assert.throws(() => validateInheritedV01SourceDelta([
    ["A", ".github/workflows/g1-06-exact-source.yml"],
    ["A", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
  ]), /inherited|delta/i);
});

test("rejects inherited segment identity substitution and nonempty retry-start delta", () => {
  assert.throws(() => validateInheritedV01Identity({
    packageRef: PACKAGE_REF,
    originalAnchor: ORIGINAL_A6_ANCHOR,
    sourceRef: INHERITED_V01_SOURCE_REF,
    sourceDelta: INHERITED_V01_SOURCE_PATHS,
    status: "INHERITED_VALID",
  }), /inherited|identity/i);
  assert.throws(() => validateActualStartingRevision(ORIGINAL_A6_ANCHOR), /starting|retry anchor/i);
});

test("accepts the four-file v0.2 retry source delta", () => {
  assert.doesNotThrow(() => validateSourceDelta([
    ["M", ".github/workflows/g1-06-exact-source.yml"],
    ["M", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
    ["M", PRODUCTION_REPAIR_PATH],
  ]));
});

test("rejects extra_source_path", () => {
  assert.throws(() => validateSourceDelta([
    ["A", ".github/workflows/g1-06-exact-source.yml"],
    ["A", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["A", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
    ["A", "runtime/semantic/forbidden.cpp"],
  ]), /count|unauthorized/i);
});

test("rejects missing_source_path", () => {
  assert.throws(() => validateSourceDelta([
    ["M", ".github/workflows/g1-06-exact-source.yml"],
    ["M", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
  ]), /count|four/i);
});

test("rejects fifth retry path and wrong change status", () => {
  assert.throws(() => validateSourceDelta([
    ["M", ".github/workflows/g1-06-exact-source.yml"],
    ["M", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
    ["M", PRODUCTION_REPAIR_PATH],
    ["M", "runtime/semantic/src/forbidden.cpp"],
  ]), /count|unauthorized/i);
  assert.throws(() => validateSourceDelta([
    ["A", ".github/workflows/g1-06-exact-source.yml"],
    ["M", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs"],
    ["M", PRODUCTION_REPAIR_PATH],
  ]), /unauthorized|status/i);
});

const productionPatch = `@@ -87 +87,4 @@
-    if(source.placement.parent_id)setId(*source.placement.parent_id,destination.mutable_placement()->mutable_parent_id());destination.mutable_placement()->mutable_order_key()->set_value(std::string(reinterpret_cast<const char*>(source.placement.order_key.bytes().data()),source.placement.order_key.bytes().size()));
+    if(source.placement.parent_id) {
+        setId(*source.placement.parent_id,destination.mutable_placement()->mutable_parent_id());
+    }
+    destination.mutable_placement()->mutable_order_key()->set_value(std::string(reinterpret_cast<const char*>(source.placement.order_key.bytes().data()),source.placement.order_key.bytes().size()));`;

test("accepts the semantic no-op warning repair boundary", () => {
  assert.doesNotThrow(() => validateProductionRepairPatch(productionPatch));
});

test("rejects production patch outside the authorized statement boundary", () => {
  assert.throws(() => validateProductionRepairPatch(`${productionPatch}\n+    destination.set_kind_version(99);`), /statement|unexpected|authorized/i);
});

test("rejects conditional order-key write or mapping mutation", () => {
  assert.throws(() => validateProductionRepairPatch(productionPatch.replace("destination.mutable_placement()->mutable_order_key()->set_value", "if(source.placement.parent_id)destination.mutable_placement()->mutable_order_key()->set_value")), /conditional|order key|statement/i);
  assert.throws(() => validateProductionRepairPatch(productionPatch.replace("mutable_parent_id", "mutable_wrong_id")), /identifier|mapping|statement/i);
});

test("rejects CI_head_checkout_source_mismatch", () => {
  assert.throws(() => validateCiIdentity(ci({ checkoutSha: "c".repeat(40) }), SOURCE_REF), /head|checkout|match/i);
});

test("rejects missing_protobuf_on", () => {
  const value = facts();
  delete value.fullSemantic;
  assert.throws(() => validateSemanticAccounting(value), /full semantic CTest/i);
});

test("rejects missing_protobuf_off", () => {
  const value = facts();
  delete value.fullSemanticOff;
  assert.throws(() => validateSemanticAccounting(value), /protobuf-off/i);
});

test("rejects unexpected_protobuf_off_failure", () => {
  const value = facts();
  value.fullSemanticOff = { ...value.fullSemanticOff, failed: 1, executed: 447, passed: 446 };
  assert.throws(() => validateSemanticAccounting(value), /unexpected failures|executed/i);
});

test("rejects unaccounted_skip_drift", () => {
  const value = facts();
  value.fullSemanticOff = { ...value.fullSemanticOff, skipIds: [25] };
  assert.throws(() => validateSemanticAccounting(value), /skip accounting|skipIds/i);
});

test("rejects missing_required_evidence_file", () => {
  assert.throws(() => validateEvidenceInventory(REQUIRED_EVIDENCE_FILES.slice(0, -1)), /inventory|incomplete/i);
});

test("rejects digest_only_correctness_claim", () => {
  assert.throws(() => validateOracleContract({ primary: "digest", digestRole: "primary", digestOnlyPassAssertions: 1 }), /canonical|digest-only/i);
});

test("rejects forbidden_gate_evidence_output_path", () => {
  assert.throws(() => assertSafeOutputDirectory(join(process.cwd(), "verification/evidence/gates/G1/foreign")), /evidence\/gates|directly/i);
});

test("rejects historical_A5_evidence_mutation", () => {
  assert.throws(() => validateSourceDelta([
    ["A", ".github/workflows/g1-06-exact-source.yml"],
    ["A", "verification/tools/generate_g1_06_a6_evidence.mjs"],
    ["M", "verification/evidence/gates/G1/a2f0b21a3ff18f441eaa7d3d7702698eca6b5edc/GT-G1-06/G1-06-PLAN.json"],
  ]), /unauthorized|exactly four/i);
});

test("rejects unsupported_A5_provenance_substitution", () => {
  assert.throws(() => validateA5Provenance({
    status: "ACCEPTED_FOR_DOWNSTREAM",
    packageRef: "notion://foreign",
    sourceRef: A5_SOURCE_REF,
    materializedRef: A5_MATERIALIZED_REF,
    evidenceFiles: REQUIRED_EVIDENCE_FILES,
  }), /provenance|substitution/i);
});
