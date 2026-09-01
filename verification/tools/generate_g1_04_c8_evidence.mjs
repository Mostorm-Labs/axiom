import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { isDeepStrictEqual } from "node:util";

import { compareProviders } from "../packages/semantic-conformance-cli/dist/compare.js";
import { runCoreCorpus } from "../packages/semantic-conformance-cli/dist/core-corpus.js";
import { evaluateC7Gate } from "../packages/semantic-conformance-cli/dist/gate.js";

export const PACKAGE_REF = "5ef414e3dc5b8f4cc42327543b3b1b978ff9a0cc";
export const TASK_ANCHOR = "34c8db4f247849c5850e16226b0e556f57497053";
export const BRANCH_REF = "refs/heads/codex/gt-g1-04-c8-exact-source-ci";
export const REQUIRED_EVIDENCE_FILES = [
  "C-CORE-CORPUS.json",
  "C-IDEMPOTENCY.json",
  "C-NO-MUTATION.json",
  "C-PLAN-PROJECTION.json",
  "C-OPEN-RECONCILIATION.json",
  "C-PROVIDER-DIFF.json",
  "C-GATE.json",
  "C-CI-RUN.json",
];

const P20_REF = "notion:3cc4c57a-590c-81ae-ab73-d75501c47169";
const P30_REF = "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b";
const C6_SOURCE_REF = "2bd2a2fa6502163d471995147daa683cefd7cf8f";
const C7_SOURCE_REF = "4abd5a472c84457cfecd763957e68a6dc06c18d3";
const P36_SOURCE_REF = "492d2f914f078a6e4ac8b567e07f7ec813c10107";
const P36_MATERIALIZED_REF = "9b73be589ae070bc602b8989f83d89745a54774e";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");
const SOURCE_PATHS = new Set([
  ".github/workflows/g1-04-c-exact-source.yml",
  "verification/tools/generate_g1_04_c8_evidence.mjs",
  "verification/packages/semantic-conformance-cli/test/c8-exact-source-evidence.test.mjs",
  "verification/schemas/semantic/g1-04-c-ci-run.schema.json",
]);
const PATHS = {
  cases: "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
  expected: "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
  suite: "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
  generated: "verification/corpus/semantic/v1/g1-04-c/generated",
  manifest: "verification/corpus/semantic/v1/g1-04-c/generated/manifest.json",
};

function fail(message) {
  throw new Error(message);
}

function isSha(value) {
  return typeof value === "string" && /^[0-9a-f]{40}$/.test(value);
}

function asRecord(value, label) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) fail(`${label} must be an object`);
  return value;
}

function asString(value, label) {
  if (typeof value !== "string" || value.length === 0) fail(`${label} must be a non-empty string`);
  return value;
}

function asZero(value, label) {
  if (value !== 0) fail(`${label} must be zero`);
  return value;
}

function git(args) {
  return execFileSync("git", args, { cwd: repoRoot, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  return JSON.parse(git(["show", `${ref}:${path}`]));
}

function requireCommit(ref, label) {
  if (!isSha(ref)) fail(`${label} must be a full immutable SHA`);
  try {
    return git(["rev-parse", "--verify", `${ref}^{commit}`]);
  } catch {
    fail(`${label} is not a resolvable commit: ${ref}`);
  }
}

function requireAncestor(ancestor, descendant, label) {
  try {
    execFileSync("git", ["merge-base", "--is-ancestor", ancestor, descendant], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    fail(`${label} is not an ancestor: ${ancestor} -> ${descendant}`);
  }
}

function requireNoDelta(from, to, paths, label) {
  try {
    execFileSync("git", ["diff", "--quiet", from, to, "--", ...paths], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    fail(`${label} changed unexpectedly`);
  }
}

function listFiles(root) {
  const files = [];
  function visit(current) {
    for (const name of readdirSync(current).sort()) {
      const path = resolve(current, name);
      const info = statSync(path);
      if (info.isDirectory()) visit(path);
      else if (info.isFile()) files.push(relative(root, path));
      else fail(`unsupported fixture entry: ${path}`);
    }
  }
  visit(root);
  return files.sort();
}

function treeDigest(root) {
  const digest = createHash("sha256");
  for (const path of listFiles(root)) {
    digest.update(path);
    digest.update("\0");
    digest.update(readFileSync(resolve(root, path)));
    digest.update("\0");
  }
  return digest.digest("hex");
}

function assertSourceScope(sourceRef) {
  const entries = git(["diff", "--name-status", PACKAGE_REF, sourceRef]).split("\n").filter(Boolean).map((entry) => entry.split("\t"));
  if (entries.length !== SOURCE_PATHS.size || entries.some(([status, path]) => status !== "A" || !SOURCE_PATHS.has(path))) {
    fail(`C8 source scope mismatch: ${entries.map((entry) => entry.join(":")) .join(", ")}`);
  }
}

function assertProtectedRoots(sourceRef) {
  requireNoDelta(TASK_ANCHOR, sourceRef, [
    "runtime/semantic/include",
    "runtime/semantic/src",
    "runtime/semantic/tools",
    "runtime/semantic/tests",
    "schema/axiom/v1",
    "verification/corpus/semantic",
    "verification/fixture-author",
    "verification/tools/g1_04_c_contract.mjs",
  ], "production semantic, schema, C1/C2, or fixture-author roots");
  requireNoDelta(PACKAGE_REF, sourceRef, ["docs"], "architecture/docs");
}

function assertSourceLineage(sourceRef) {
  requireCommit(PACKAGE_REF, "C8 package ref");
  requireCommit(TASK_ANCHOR, "C8 task anchor");
  requireCommit(sourceRef, "source ref");
  requireAncestor(PACKAGE_REF, sourceRef, "C8 package ref");
  requireAncestor(TASK_ANCHOR, sourceRef, "task anchor");
  assertSourceScope(sourceRef);
  assertProtectedRoots(sourceRef);
}

function assertFreshObservation(observation) {
  const value = asRecord(observation, "native observation");
  if (value.format !== "axiom-gt-g1-04-c-plan-projection-v2" || value.formatVersion !== 2 || value.factsOnly !== true) fail("native observation has an invalid facts-only envelope");
  if (value.acceptedCases !== 90 || value.observationCount !== 180 || value.noMutationObservations !== 180 || value.unexpectedHarnessErrors !== 0) fail("native observation inventory is not 90/180/180 with zero harness errors");
  if (!Array.isArray(value.providers) || value.providers.length !== 2 || value.providers[0] !== "reference" || value.providers[1] !== "indexed") fail("native observation provider inventory is invalid");
  if (value.expectedTruthReads !== 0 || value.semanticCodecCalls !== 0) fail("native observation must not read expected truth or SemanticCodec");
  if (!Array.isArray(value.observationRecords) || value.observationRecords.length !== 180) fail("native observation records are incomplete");
  for (const [index, raw] of value.observationRecords.entries()) {
    const record = asRecord(raw, `native observation record ${index}`);
    if (record.format !== "axiom-gt-g1-04-c-observation-v1" || record.formatVersion !== 1 || record.provenance !== "IMPLEMENTATION_OBSERVATION") fail(`native observation record ${index} trust envelope is invalid`);
    if (record.provider !== "reference" && record.provider !== "indexed") fail(`native observation record ${index} provider is invalid`);
    asString(record.caseId, `native observation record ${index}.caseId`);
    if (!isDeepStrictEqual(record.beforeProjection, record.afterProjection)) fail(`native observation record ${index} mutated state`);
  }
  const negativeZero = asRecord(asRecord(value.decodedInputAudit, "decoded input audit").negativeZero, "negative zero audit");
  if (negativeZero.caseId !== "C1-TRANSFORM-NEGATIVE-ZERO" || negativeZero.requiredBits !== "8000000000000000") fail("native observation negative-zero audit is invalid");
  const byProvider = asRecord(negativeZero.byProvider, "negative zero provider audit");
  for (const provider of ["reference", "indexed"]) {
    if (!Array.isArray(byProvider[provider]) || !byProvider[provider].includes("8000000000000000")) fail(`native observation lost negative-zero bits for ${provider}`);
  }
  return value;
}

function compileFixturesTwice() {
  const authoringRoot = resolve(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring");
  const generatedRoot = resolve(repoRoot, PATHS.generated);
  const beforeAuthoring = treeDigest(authoringRoot);
  const beforeGenerated = treeDigest(generatedRoot);
  const temporaryRoot = mkdtempSync(resolve(tmpdir(), "g1-04-c8-fixtures-"));
  try {
    const first = resolve(temporaryRoot, "first");
    const second = resolve(temporaryRoot, "second");
    const compiler = resolve(repoRoot, "verification/fixture-author/compile_g1_04_c.py");
    for (const output of [first, second]) {
      execFileSync("python3", [compiler, "--root", repoRoot, "--output", output], { cwd: repoRoot, stdio: "pipe" });
    }
    const firstFiles = listFiles(first);
    const secondFiles = listFiles(second);
    const trackedFiles = listFiles(generatedRoot);
    if (JSON.stringify(firstFiles) !== JSON.stringify(secondFiles) || JSON.stringify(firstFiles) !== JSON.stringify(trackedFiles)) fail("fixture regeneration file inventory differs");
    for (const path of firstFiles) {
      const firstBytes = readFileSync(resolve(first, path));
      if (!firstBytes.equals(readFileSync(resolve(second, path)))) fail(`fixture double regeneration differs: ${path}`);
      if (!firstBytes.equals(readFileSync(resolve(generatedRoot, path)))) fail(`fixture regeneration differs from accepted corpus: ${path}`);
    }
    const manifest = JSON.parse(readFileSync(resolve(first, "manifest.json"), "utf8"));
    if (manifest.caseCount !== 90 || manifest.blockingCaseCount !== 90 || !Array.isArray(manifest.entries) || manifest.entries.length !== 90) fail("fixture regeneration is not the accepted 90-case corpus");
    if (treeDigest(authoringRoot) !== beforeAuthoring || treeDigest(generatedRoot) !== beforeGenerated) fail("fixture compiler wrote to accepted authority roots");
    return { status: "PASS", caseCount: 90, blockingCaseCount: 90, byteIdentical: true, trackedCorpusIdentical: true, fileCount: firstFiles.length, authoringRootWrites: 0, generatedRootWrites: 0, treeSha256: treeDigest(first) };
  } finally {
    rmSync(temporaryRoot, { recursive: true, force: true });
  }
}

function parseArgs(argv) {
  const flags = ["package-ref", "task-anchor", "source-ref", "observation", "output-dir", "ci-run-id", "ci-run-attempt", "ci-event", "ci-ref", "ci-head-sha", "checkout-sha", "workflow-ref"];
  if (argv.length !== flags.length * 2) fail("C8 evidence CLI requires the complete immutable identity surface");
  const result = {};
  for (let index = 0; index < flags.length; index += 1) {
    const flag = `--${flags[index]}`;
    if (argv[index * 2] !== flag || !argv[index * 2 + 1]) fail(`missing required C8 evidence argument: ${flag}`);
    result[flags[index].replace(/-([a-z])/g, (_, letter) => letter.toUpperCase())] = argv[index * 2 + 1];
  }
  return result;
}

function assertSafeOutputDirectory(outputDir) {
  const output = resolve(repoRoot, outputDir);
  const forbidden = [
    resolve(repoRoot, "verification/corpus/semantic/v1/g1-04-c/authoring"),
    resolve(repoRoot, PATHS.generated),
    resolve(repoRoot, "verification/schemas"),
    resolve(repoRoot, "verification/fixture-author"),
    resolve(repoRoot, "runtime"),
    resolve(repoRoot, "verification/evidence/gates"),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) fail(`forbidden C8 evidence output directory: ${output}`);
  return output;
}

export function assertMaterializationPath(root, name) {
  if (typeof root !== "string" || root.length === 0 || root.includes("\0")) fail("materialization root is invalid");
  if (typeof name !== "string" || name.includes("/") || name.includes("\\") || name === "." || name === ".." || !REQUIRED_EVIDENCE_FILES.includes(name)) fail("materialization path must name exactly one required evidence file");
  return `${root}/${name}`;
}

export function validateCiRunRecord(value, expected) {
  const record = asRecord(value, "C8 CI-run record");
  if (record.format !== "axiom-g1-04-c-ci-run-v1" || record.formatVersion !== 1 || record.gate !== "GT-G1-04-C" || record.slice !== "C8") fail("C8 CI-run format is invalid");
  if (record.packageRef !== expected.packageRef) fail("C8 CI-run packageRef is foreign");
  const anchor = asRecord(record.taskAnchor, "C8 CI-run taskAnchor");
  if (anchor.revision !== expected.taskAnchor || anchor.relation !== "ancestor") fail("C8 CI-run taskAnchor is foreign");
  if (!isSha(record.sourceRef) || record.sourceRef !== expected.sourceRef) fail("C8 CI-run sourceRef is foreign or invalid");
  if (record.event !== "push") fail("C8 CI-run event must be push");
  if (record.ref !== expected.branchRef) fail("C8 CI-run ref is not the authoritative branch");
  if (record.headSha !== record.sourceRef || record.checkoutSha !== record.sourceRef) fail("C8 CI-run source identity does not equal headSha and checkoutSha");
  if (!isSha(record.headSha) || !isSha(record.checkoutSha)) fail("C8 CI-run SHA is invalid");
  if (record.repository !== "Mostorm-Labs/axiom") fail("C8 CI-run repository is invalid");
  if (record.workflowName !== "GT-G1-04-C Exact Source" || record.workflowPath !== ".github/workflows/g1-04-c-exact-source.yml" || !asString(record.workflowRef, "C8 CI-run workflowRef").includes(record.workflowPath)) fail("C8 CI-run workflow identity is invalid");
  if (!/^[1-9][0-9]*$/.test(asString(record.runId, "C8 CI-run runId")) || !/^[1-9][0-9]*$/.test(asString(record.runAttempt, "C8 CI-run runAttempt"))) fail("C8 CI-run hosted run id is invalid");
  if (!/^https:\/\/github\.com\/Mostorm-Labs\/axiom\/actions\/runs\/[1-9][0-9]*$/.test(asString(record.hostedRunUrl, "C8 CI-run hostedRunUrl"))) fail("C8 CI-run must identify a hosted GitHub run");
  if (record.artifactName !== `gt-g1-04-c8-${record.sourceRef}`) fail("C8 CI-run artifactName is invalid");
  if (!Array.isArray(record.requiredEvidenceFiles) || JSON.stringify(record.requiredEvidenceFiles) !== JSON.stringify(REQUIRED_EVIDENCE_FILES)) fail("C8 CI-run required evidence inventory is invalid");
  const verificationResults = asRecord(record.verificationResults, "C8 CI-run verificationResults");
  for (const key of ["semanticCli", "fixtureReproducibility", "fullCTest", "nativeObservation", "independentEvaluation"]) if (verificationResults[key] !== "PASS") fail(`C8 CI-run verification result is not PASS: ${key}`);
  asZero(record.expectedTruthWrites, "expectedTruthWrites");
  if (record.providerOutputUsedAsExpected !== false) fail("providerOutputUsedAsExpected must be false");
  asZero(record.productionSemanticDelta, "productionSemanticDelta");
  asZero(record.authorityDelta, "authorityDelta");
  if (record.verdict !== "PASS") fail("C8 CI-run verdict must be PASS");
  return record;
}

function envelope(sourceRef) {
  return {
    gate: "GT-G1-04-C",
    slice: "C8",
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    authority: { verification: P20_REF, implementationPlan: P30_REF },
    trustedDependencies: {
      c6SourceRef: C6_SOURCE_REF,
      c6EvidenceRef: `git:${TASK_ANCHOR}:verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C`,
      c7SourceRef: C7_SOURCE_REF,
      c7EvidenceRef: `git:${TASK_ANCHOR}:verification/evidence/gates/G1/${C7_SOURCE_REF}/GT-G1-04-C`,
      p36SourceRef: P36_SOURCE_REF,
      p36MaterializedRef: P36_MATERIALIZED_REF,
    },
    authorityManualExpected: true,
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
    authorityDelta: 0,
  };
}

function groupByCase(observations) {
  const grouped = new Map();
  for (const observation of observations) {
    let providers = grouped.get(observation.caseId);
    if (!providers) {
      providers = new Map();
      grouped.set(observation.caseId, providers);
    }
    if (providers.has(observation.provider)) fail(`duplicate fresh provider observation: ${observation.caseId}/${observation.provider}`);
    providers.set(observation.provider, observation);
  }
  if (grouped.size !== 90) fail("fresh observations do not cover exactly 90 cases");
  for (const [caseId, providers] of grouped) if (providers.size !== 2 || !providers.has("reference") || !providers.has("indexed")) fail(`fresh observations lack Reference/Indexed pair: ${caseId}`);
  return grouped;
}

function assertResults(run) {
  if (run.selectedCaseCount !== 90 || run.selectedExpectedCount !== 90 || run.selectedObservationCount !== 180 || run.operationFamilyCount !== 15) fail("independent evaluator inventory is not 90/90/180 across 15 operations");
  if (run.results.some((result) => result.status !== "PASS")) fail("fresh provider observations disagree with manually authored expected truth");
  for (const key of ["missingMandatoryFamilies", "wrongFamilyCaseIds", "unselectedCaseIds", "duplicateSuiteCaseIds"]) if (run.coverage[key].length !== 0) fail(`independent evaluator coverage is invalid: ${key}`);
}

export function generateEvidence(input) {
  const { packageRef, taskAnchor, sourceRef, observation: observationPath, outputDir: _outputDir, ciRunId, ciRunAttempt, ciEvent, ciRef, ciHeadSha, checkoutSha, workflowRef } = input;
  if (packageRef !== PACKAGE_REF) fail("C8 packageRef must equal the superseding package ref");
  if (taskAnchor !== TASK_ANCHOR) fail("C8 taskAnchor must equal the frozen task anchor");
  if (!isSha(sourceRef) || !isSha(ciHeadSha) || !isSha(checkoutSha)) fail("C8 source and CI identities must be full immutable SHAs");
  if (ciEvent !== "push" || ciRef !== BRANCH_REF) fail("C8 CI identity is not an authoritative push on the exact branch");
  if (sourceRef !== ciHeadSha || sourceRef !== checkoutSha) fail("C8 sourceRef, CI head SHA, and checkout SHA must match exactly");
  assertSourceLineage(sourceRef);
  const inheritedC6 = {
    idempotency: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-IDEMPOTENCY.json`),
    noMutation: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-NO-MUTATION.json`),
    planProjection: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-PLAN-PROJECTION.json`),
    openReconciliation: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-OPEN-RECONCILIATION.json`),
  };
  if (inheritedC6.idempotency.status !== "PASS" || inheritedC6.idempotency.cases?.length !== 3 || inheritedC6.noMutation.status !== "PASS" || inheritedC6.noMutation.acceptedCases !== 90 || inheritedC6.noMutation.observationCount !== 180 || inheritedC6.noMutation.beforeAfterEqual !== "180/180" || inheritedC6.planProjection.status !== "PASS" || inheritedC6.planProjection.factsOnly !== true || inheritedC6.openReconciliation.status !== "PASS") fail("accepted C6 evidence lineage is invalid");
  const inheritedC7 = {
    providerDiff: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C7_SOURCE_REF}/GT-G1-04-C/C-PROVIDER-DIFF.json`),
    gate: gitJson(TASK_ANCHOR, `verification/evidence/gates/G1/${C7_SOURCE_REF}/GT-G1-04-C/C-GATE.json`),
  };
  if (inheritedC7.providerDiff.status !== "PASS" || inheritedC7.providerDiff.providerAgreement !== "90/90" || inheritedC7.providerDiff.divergenceCount !== 0 || inheritedC7.gate.status !== "PASS") fail("accepted C7 evidence lineage is invalid");
  const observation = assertFreshObservation(JSON.parse(readFileSync(observationPath, "utf8")));
  const cases = gitJson(sourceRef, PATHS.cases);
  const expected = gitJson(sourceRef, PATHS.expected);
  const suite = gitJson(sourceRef, PATHS.suite);
  const manifest = gitJson(sourceRef, PATHS.manifest);
  if (manifest.caseCount !== 90 || manifest.blockingCaseCount !== 90 || !Array.isArray(manifest.entries) || manifest.entries.length !== 90) fail("accepted generated manifest is not the 90-case blocking corpus");
  const run = runCoreCorpus({ cases, expected, suite, c3Evidence: observation, c3SourceRef: sourceRef });
  assertResults(run);
  const fixtureReproducibility = compileFixturesTwice();
  const observations = groupByCase(observation.observationRecords);
  const expectedByCase = new Map(expected.map((record) => [record.caseId, record]));
  const divergenceCaseIds = [];
  for (const [caseId, providers] of observations) {
    const expectation = expectedByCase.get(caseId);
    if (!expectation) fail(`fresh observation has no manual expected record: ${caseId}`);
    if (compareProviders(expectation, providers.get("reference"), providers.get("indexed")).length !== 0) divergenceCaseIds.push(caseId);
  }
  if (divergenceCaseIds.length !== 0) fail(`fresh providers diverge: ${divergenceCaseIds.join(", ")}`);
  const resultStatusCounts = Object.fromEntries(["PASS", "FAIL", "OBSERVATION_ONLY"].map((status) => [status, run.results.filter((result) => result.status === status).length]));
  const sourceEnvelope = envelope(sourceRef);
  const evidenceRoot = `g1-04-c://c8/${sourceRef}`;
  const coreCorpus = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-core-corpus-v1",
    formatVersion: 1,
    suiteId: run.suiteId,
    selectedCaseCount: run.selectedCaseCount,
    selectedExpectedCount: run.selectedExpectedCount,
    selectedObservationCount: run.selectedObservationCount,
    operationFamilyCount: run.operationFamilyCount,
    missingMandatoryFamilies: run.coverage.missingMandatoryFamilies,
    wrongFamilyCaseIds: run.coverage.wrongFamilyCaseIds,
    unselectedCaseIds: run.coverage.unselectedCaseIds,
    duplicateSuiteCaseIds: run.coverage.duplicateSuiteCaseIds,
    acceptedOpenPolicyCount: expected.filter((record) => record.openPolicy === true).length,
    acceptedExpectedAllAuthorityManual: expected.every((record) => record.provenance === "AUTHORITY_MANUAL"),
    resultStatusCounts,
    results: run.results,
  };
  const idempotencyIds = ["C1-IDEMPOTENT-EQUIVALENT", "C1-RESTORE-OPID-BEFORE-EXISTENCE", "C1-ID-COLLISION"];
  const idempotency = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-idempotency-v1",
    formatVersion: 1,
    status: "PASS",
    orderingProof: "fresh Reference and Indexed observations terminate idempotency vectors at IDEMPOTENCY before stateful validation",
    cases: idempotencyIds.map((caseId) => {
      const expectation = expectedByCase.get(caseId);
      const providers = observations.get(caseId);
      if (!expectation || !providers || expectation.terminalPhase !== "IDEMPOTENCY") fail(`fresh idempotency proof is incomplete: ${caseId}`);
      return { caseId, disposition: expectation.disposition, terminalPhase: expectation.terminalPhase, reference: "PASS", indexed: "PASS" };
    }),
  };
  const noMutation = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-no-mutation-v1",
    formatVersion: 1,
    status: "PASS",
    acceptedCases: observation.acceptedCases,
    observationCount: observation.observationCount,
    referenceObservations: 90,
    indexedObservations: 90,
    beforeAfterEqual: "180/180",
    unexpectedHarnessErrors: observation.unexpectedHarnessErrors,
    observerMutationCalls: 0,
    expectedTruthReads: observation.expectedTruthReads,
    semanticCodecCalls: observation.semanticCodecCalls,
  };
  const planProjection = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-plan-projection-v1",
    formatVersion: 1,
    status: "PASS",
    factsOnly: true,
    negativeZero: observation.decodedInputAudit.negativeZero,
    cases: expected.filter((record) => record.logicalPlanProjection !== undefined).map((record) => ({ caseId: record.caseId, expected: record.logicalPlanProjection, reference: "PASS", indexed: "PASS" })),
  };
  const openReconciliation = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-open-reconciliation-v1",
    formatVersion: 1,
    status: "PASS",
    acceptedOpenPolicyCount: 0,
    observationOnlyCount: resultStatusCounts.OBSERVATION_ONLY,
    closedCaseCount: 90,
    currentAuthorityOpenCases: [],
  };
  const providerDiff = {
    ...sourceEnvelope,
    format: "axiom-gt-g1-04-c8-provider-diff-v1",
    formatVersion: 1,
    status: "PASS",
    caseCount: 90,
    providerPairCount: 90,
    providerAgreement: "90/90",
    divergenceCount: 0,
    divergenceCaseIds,
    goldenPassCount: resultStatusCounts.PASS,
    goldenFailCount: resultStatusCounts.FAIL,
    observationOnlyCount: resultStatusCounts.OBSERVATION_ONLY,
    goldenMismatchCaseIds: [],
    manualGoldenCorrectness: "PASS",
    fixtureReproducibility,
  };
  const gateSummary = evaluateC7Gate({
    conditions: [
      { id: "authority-provenance", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-CORE-CORPUS.json`] },
      { id: "mandatory-corpus", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-CORE-CORPUS.json`] },
      { id: "manual-golden-correctness", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-CORE-CORPUS.json`] },
      { id: "no-mutation", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-NO-MUTATION.json`] },
      { id: "provider-differential", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-PROVIDER-DIFF.json`] },
      { id: "fixture-reproducibility", status: "PASS", evidenceRefs: [`${evidenceRoot}/fixture-reproducibility`] },
      { id: "open-reconciliation", status: "PASS", evidenceRefs: [`${evidenceRoot}/C-OPEN-RECONCILIATION.json`] },
    ],
  });
  if (gateSummary.status !== "PASS") fail("fresh C8 gate is not PASS");
  const gate = {
    format: "axiom-g1-04-c-gate-v1",
    formatVersion: 1,
    provenance: "GATE_EVIDENCE",
    gateId: "GT-G1-04-C",
    status: "PASS",
    resultRefs: [`${evidenceRoot}/C-CORE-CORPUS.json`, `${evidenceRoot}/C-IDEMPOTENCY.json`, `${evidenceRoot}/C-NO-MUTATION.json`, `${evidenceRoot}/C-PLAN-PROJECTION.json`, `${evidenceRoot}/C-OPEN-RECONCILIATION.json`, `${evidenceRoot}/C-PROVIDER-DIFF.json`],
    authorityRefs: [P20_REF, P30_REF],
    sourceRef,
  };
  const ciRun = validateCiRunRecord({
    format: "axiom-g1-04-c-ci-run-v1",
    formatVersion: 1,
    gate: "GT-G1-04-C",
    slice: "C8",
    packageRef,
    taskAnchor: { revision: taskAnchor, relation: "ancestor" },
    sourceRef,
    repository: "Mostorm-Labs/axiom",
    workflowName: "GT-G1-04-C Exact Source",
    workflowPath: ".github/workflows/g1-04-c-exact-source.yml",
    workflowRef,
    event: ciEvent,
    ref: ciRef,
    runId: ciRunId,
    runAttempt: ciRunAttempt,
    hostedRunUrl: `https://github.com/Mostorm-Labs/axiom/actions/runs/${ciRunId}`,
    headSha: ciHeadSha,
    checkoutSha,
    artifactName: `gt-g1-04-c8-${sourceRef}`,
    requiredEvidenceFiles: REQUIRED_EVIDENCE_FILES,
    verificationResults: { semanticCli: "PASS", fixtureReproducibility: "PASS", fullCTest: "PASS", nativeObservation: "PASS", independentEvaluation: "PASS" },
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
    authorityDelta: 0,
    verdict: "PASS",
  }, { packageRef: PACKAGE_REF, taskAnchor: TASK_ANCHOR, sourceRef, branchRef: BRANCH_REF });
  return {
    "C-CORE-CORPUS.json": coreCorpus,
    "C-IDEMPOTENCY.json": idempotency,
    "C-NO-MUTATION.json": noMutation,
    "C-PLAN-PROJECTION.json": planProjection,
    "C-OPEN-RECONCILIATION.json": openReconciliation,
    "C-PROVIDER-DIFF.json": { ...providerDiff, gateSummary },
    "C-GATE.json": gate,
    "C-CI-RUN.json": ciRun,
  };
}

function main() {
  try {
    const args = parseArgs(process.argv.slice(2));
    const output = assertSafeOutputDirectory(args.outputDir);
    if (existsSync(output) && readdirSync(output).length !== 0) fail(`C8 evidence output directory must be empty: ${output}`);
    const evidence = generateEvidence(args);
    mkdirSync(output, { recursive: true });
    if (JSON.stringify(Object.keys(evidence)) !== JSON.stringify(REQUIRED_EVIDENCE_FILES)) fail("C8 generator did not produce exactly the required evidence files");
    for (const [name, value] of Object.entries(evidence)) writeFileSync(resolve(output, name), `${JSON.stringify(value, null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
