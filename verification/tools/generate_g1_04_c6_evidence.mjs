import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { runCrossCutting } from "../packages/semantic-conformance-cli/dist/cross-cutting.js";

const PACKAGE_REF = "8a43af6ebb9b88ecefb79141eb90b5680405a449";
const TASK_ANCHOR = "c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d";
const C3_SOURCE_REF = "906327beb9a268c339accd6d3ca6a7038e54ad68";
const C3_MATERIALIZED_REF = "2a601ab35a2294cb38d713ab001bbac7deaa9cf7";
const P36_R2_SOURCE_REF = "1763d57e7554ec690634326b998971f0decaae28";
const P36_FINAL_MATERIALIZED_REF = "c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d";

const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");

const PATHS = {
  cases: "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
  expected: "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
  coreCorpus: `verification/evidence/gates/G1/${C3_SOURCE_REF}/GT-G1-04-C/C-CORE-CORPUS.json`,
  noMutation: `verification/evidence/gates/G1/${C3_SOURCE_REF}/GT-G1-04-C/C-NO-MUTATION.json`,
  planProjection: `verification/evidence/gates/G1/${C3_SOURCE_REF}/GT-G1-04-C/C-PLAN-PROJECTION.json`,
  p36Repair: `verification/evidence/gates/G1/${C3_SOURCE_REF}/GT-G1-04-C/P36-C6-UPSTREAM-REPAIR.json`,
  p36Overflow: `verification/evidence/gates/G1/${P36_R2_SOURCE_REF}/GT-G1-04-C/P36-R2-OVERFLOW-LINEAGE.json`,
};

const C6_SOURCE_PATHS = new Set([
  "verification/packages/semantic-conformance-cli/src/cross-cutting.ts",
  "verification/packages/semantic-conformance-cli/test/cross-cutting.test.mjs",
  "verification/packages/semantic-conformance-cli/test/cross-cutting-evidence.test.mjs",
  "verification/tools/generate_g1_04_c6_evidence.mjs",
]);

function git(args) {
  return execFileSync("git", args, { cwd: repoRoot, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  return JSON.parse(git(["show", `${ref}:${path}`]));
}

function requireCommit(ref, label) {
  try {
    return git(["rev-parse", "--verify", `${ref}^{commit}`]);
  } catch {
    throw new Error(`${label} is not a resolvable commit: ${ref}`);
  }
}

function requireAncestor(ancestor, descendant, label) {
  try {
    execFileSync("git", ["merge-base", "--is-ancestor", ancestor, descendant], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    throw new Error(`${label} is not an ancestor: ${ancestor} -> ${descendant}`);
  }
}

function requireCleanDiff(from, to, paths, label) {
  try {
    execFileSync("git", ["diff", "--quiet", from, to, "--", ...paths], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    throw new Error(`${label} has an unexpected delta`);
  }
}

function parseArgs(argv) {
  if (argv.length !== 4 || argv[0] !== "--source-ref" || !argv[1] || argv[2] !== "--output-dir" || !argv[3]) {
    throw new Error("usage: generate_g1_04_c6_evidence.mjs --source-ref <sha> --output-dir <directory>");
  }
  return { sourceRef: argv[1], outputDir: argv[3] };
}

function assertSafeOutputPath(outputDir) {
  const output = resolve(repoRoot, outputDir);
  const forbidden = [
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/authoring"),
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/generated"),
    resolve(verificationRoot, "schemas"),
    resolve(verificationRoot, "fixture-author"),
    resolve(repoRoot, "runtime"),
    resolve(verificationRoot, "evidence/gates/G1", C3_SOURCE_REF),
    resolve(verificationRoot, "evidence/gates/G1", P36_R2_SOURCE_REF),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) {
    throw new Error(`forbidden evidence output path: ${output}`);
  }
  return output;
}

function assertSourceScope(sourceRef) {
  const changed = git(["diff", "--name-only", PACKAGE_REF, sourceRef]).split("\n").filter(Boolean);
  if (changed.length !== C6_SOURCE_PATHS.size || changed.some((path) => !C6_SOURCE_PATHS.has(path))) {
    throw new Error(`C6 source scope mismatch: ${changed.join(", ")}`);
  }
}

function assertAcceptedRoots(sourceRef) {
  const immutablePaths = [
    "verification/corpus/semantic/v1/g1-04-c",
    "verification/schemas/semantic",
    "verification/fixture-author",
    "verification/tools/g1_04_c_contract.mjs",
    "verification/tests/g1_04_c_open_reconciliation.test.mjs",
    "verification/tools/generate_g1_04_c5_evidence.mjs",
    `verification/evidence/gates/G1/${C3_SOURCE_REF}/GT-G1-04-C`,
    `verification/evidence/gates/G1/${P36_R2_SOURCE_REF}/GT-G1-04-C`,
    "schema/axiom/v1/proto",
    "runtime/semantic/include/canvas/semantic",
    "runtime/semantic/src",
  ];
  requireCleanDiff(TASK_ANCHOR, sourceRef, immutablePaths, "accepted C0-C5, P36, schema, and production roots");
}

function identityEnvelope(sourceRef) {
  return {
    gate: "GT-G1-04-C",
    slice: "C6",
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    authority: {
      verification: "notion:3cc4c57a-590c-81ae-ab73-d75501c47169",
      implementationPlan: "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b",
    },
    trustedDependencies: {
      p36FinalMaterializedRef: P36_FINAL_MATERIALIZED_REF,
      p36R1SourceRef: C3_SOURCE_REF,
      p36R1MaterializedRef: C3_MATERIALIZED_REF,
      p36R2SourceRef: P36_R2_SOURCE_REF,
      correctedC3SourceRef: C3_SOURCE_REF,
      correctedC3MaterializedRef: C3_MATERIALIZED_REF,
    },
    authorityManualExpected: true,
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
  };
}

function dispositionGroups(expected) {
  const groups = new Map();
  for (const record of expected) {
    const disposition = record.disposition;
    const ids = groups.get(disposition) ?? [];
    ids.push(record.caseId);
    groups.set(disposition, ids);
  }
  return [...groups.entries()].map(([disposition, caseIds]) => ({ disposition, caseIds, caseCount: caseIds.length, observationCount: caseIds.length * 2 }));
}

export function generateEvidence(sourceRef) {
  requireCommit(sourceRef, "source ref");
  requireCommit(PACKAGE_REF, "C6 package ref");
  requireCommit(TASK_ANCHOR, "C6 task anchor");
  requireAncestor(TASK_ANCHOR, sourceRef, "task anchor");
  requireAncestor(PACKAGE_REF, sourceRef, "C6 package ref");
  assertSourceScope(sourceRef);
  assertAcceptedRoots(sourceRef);

  const cases = gitJson(sourceRef, PATHS.cases);
  const expected = gitJson(sourceRef, PATHS.expected);
  const coreCorpusEvidence = gitJson(C3_MATERIALIZED_REF, PATHS.coreCorpus);
  const noMutationEvidence = gitJson(C3_MATERIALIZED_REF, PATHS.noMutation);
  const planProjectionEvidence = gitJson(C3_MATERIALIZED_REF, PATHS.planProjection);
  const p36RepairEvidence = gitJson(C3_MATERIALIZED_REF, PATHS.p36Repair);
  const p36OverflowLineageEvidence = gitJson(P36_FINAL_MATERIALIZED_REF, PATHS.p36Overflow);

  const run = runCrossCutting({ cases, expected, coreCorpusEvidence, noMutationEvidence, planProjectionEvidence, p36RepairEvidence, p36OverflowLineageEvidence });
  const envelope = identityEnvelope(sourceRef);
  const idempotency = {
    ...envelope,
    format: "axiom-gt-g1-04-c6-idempotency-v1",
    formatVersion: 1,
    status: run.idempotency.status,
    orderingProof: run.idempotency.orderingProof,
    cases: run.idempotency.cases,
  };
  const noMutation = {
    ...envelope,
    format: "axiom-gt-g1-04-c6-no-mutation-v1",
    formatVersion: 1,
    status: run.noMutation.status,
    acceptedCases: run.noMutation.acceptedCases,
    observationCount: run.noMutation.observationCount,
    referenceObservations: 90,
    indexedObservations: 90,
    providerAgreement: run.noMutation.providerAgreement,
    beforeAfterEqual: `${run.noMutation.beforeAfterEqual}/180`,
    observerMutationCalls: run.noMutation.observerMutationCalls,
    byDisposition: run.noMutation.byDisposition,
    dispositionGroups: dispositionGroups(expected),
  };
  const planProjection = {
    ...envelope,
    format: "axiom-gt-g1-04-c6-plan-projection-v1",
    formatVersion: 1,
    status: run.planProjection.status,
    factsOnly: run.planProjection.factsOnly,
    cases: run.planProjection.cases,
  };
  const openReconciliation = {
    ...envelope,
    format: "axiom-gt-g1-04-c6-open-reconciliation-v1",
    formatVersion: 1,
    status: run.openReconciliation.status,
    closedGroups: run.openReconciliation.closedGroups,
    geometry: run.openReconciliation.geometry,
    restore: {
      caseId: "C1-RESTORE-NO-TOMBSTONE",
      status: run.openReconciliation.restoreNoTombstone,
      hiddenTombstoneHistoryRequired: false,
      productionTombstoneApiAdded: false,
    },
    hierarchy: run.openReconciliation.hierarchy,
    unknownFuturePolicyKeysRemainUnknown: true,
  };
  return { "C-IDEMPOTENCY.json": idempotency, "C-NO-MUTATION.json": noMutation, "C-PLAN-PROJECTION.json": planProjection, "C-OPEN-RECONCILIATION.json": openReconciliation };
}

function main() {
  try {
    const { sourceRef, outputDir } = parseArgs(process.argv.slice(2));
    const output = assertSafeOutputPath(outputDir);
    const evidence = generateEvidence(sourceRef);
    mkdirSync(output, { recursive: true });
    for (const [name, value] of Object.entries(evidence)) writeFileSync(resolve(output, name), `${JSON.stringify(value, null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
