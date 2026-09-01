import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { isDeepStrictEqual } from "node:util";
import { runCoreCorpus } from "../packages/semantic-conformance-cli/dist/core-corpus.js";

const EXECUTION_START_REF = "666082e114310503b019873fb9e12744c19bcc1e";
const BLOCKED_C7_SOURCE_REF = "1c4d9b3ef2118ab7e31f20b418f03b0655c19cd4";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");
const paths = {
  cases: "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
  expected: "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
  suite: "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
  manifest: "verification/corpus/semantic/v1/g1-04-c/generated/manifest.json",
};

const P36_SOURCE_PATHS = new Set([
  "runtime/semantic/tests/CMakeLists.txt",
  "runtime/semantic/tests/g1_04_c_fixture_decoder_test.cpp",
  "runtime/semantic/tools/g1_04_c_fixture_decoder.cpp",
  "runtime/semantic/tools/g1_04_c_observation_export.cpp",
  "verification/fixture-author/compile_g1_04_c.py",
  "verification/tests/g1_04_c_authoring_root.test.mjs",
  "verification/tests/test_g1_04_c_fixture_compiler.py",
  "verification/packages/semantic-conformance-cli/test/core-corpus-evidence.test.mjs",
  "verification/packages/semantic-conformance-cli/test/p36-golden-repair-evidence.test.mjs",
  "verification/tools/generate_g1_04_c_p36_golden_repair_evidence.mjs",
  paths.expected,
]);
const GENERATED_PREFIX = "verification/corpus/semantic/v1/g1-04-c/generated/";
const EXPECTED_TRANSITIONS = new Map([
  ["C1-PLACEMENT-ORDERKEY", { field: "terminalPhase", before: "STATEFUL_VALIDATE", after: "STATELESS_VALIDATE" }],
  ["C1-GEOMETRY-OVERFLOW", { field: "semanticErrorCategory", before: "INTEGER_OVERFLOW", after: "GEOMETRY_LIMIT_EXCEEDED" }],
  ["C1-ERASE-REMOVE-WHOLE-REJECT", { field: "terminalPhase", before: "STATELESS_VALIDATE", after: "STATEFUL_VALIDATE" }],
]);

function git(args) {
  return execFileSync("git", args, { cwd: repoRoot, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  return JSON.parse(git(["show", `${ref}:${path}`]));
}

function gitBlob(ref, path) {
  return execFileSync("git", ["show", `${ref}:${path}`], { cwd: repoRoot });
}

function assertCommit(ref, label) {
  if (!/^[0-9a-f]{40}$/.test(ref)) throw new Error(`${label} must be a full immutable SHA`);
  try {
    git(["rev-parse", "--verify", `${ref}^{commit}`]);
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

function requireNoDelta(from, to, guardedPaths, label) {
  try {
    execFileSync("git", ["diff", "--quiet", from, to, "--", ...guardedPaths], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    throw new Error(`${label} changed unexpectedly`);
  }
}

function parseArgs(argv) {
  if (argv.length !== 6 || argv[0] !== "--source-ref" || !argv[1] || argv[2] !== "--observations" || !argv[3] || argv[4] !== "--output-dir" || !argv[5]) {
    throw new Error("usage: generate_g1_04_c_p36_golden_repair_evidence.mjs --source-ref <sha> --observations <path> --output-dir <directory>");
  }
  return { sourceRef: argv[1], observationsPath: argv[3], outputDir: argv[5] };
}

function assertSafeOutputPath(outputDir) {
  const output = resolve(repoRoot, outputDir);
  const forbidden = [
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/authoring"),
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/generated"),
    resolve(verificationRoot, "schemas"),
    resolve(repoRoot, "runtime"),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) throw new Error(`forbidden evidence output path: ${output}`);
  return output;
}

function assertSourceScope(sourceRef) {
  const changed = git(["diff", "--name-only", EXECUTION_START_REF, sourceRef]).split("\n").filter(Boolean);
  if (changed.some((path) => !P36_SOURCE_PATHS.has(path) && !path.startsWith(GENERATED_PREFIX))) {
    throw new Error(`P36 source scope mismatch: ${changed.join(", ")}`);
  }
  return changed.sort();
}

function assertExpectedTransitions(sourceRef) {
  const base = new Map(gitJson(EXECUTION_START_REF, paths.expected).map((record) => [record.caseId, record]));
  const current = new Map(gitJson(sourceRef, paths.expected).map((record) => [record.caseId, record]));
  if (base.size !== 90 || current.size !== 90) throw new Error("expected corpus must contain exactly 90 records");
  const changed = [];
  for (const [caseId, before] of base) {
    const after = current.get(caseId);
    if (!after) throw new Error(`expected record removed: ${caseId}`);
    if (JSON.stringify(before) === JSON.stringify(after)) continue;
    const transition = EXPECTED_TRANSITIONS.get(caseId);
    if (!transition) throw new Error(`unapproved expected record change: ${caseId}`);
    const beforeCopy = structuredClone(before);
    const afterCopy = structuredClone(after);
    delete beforeCopy[transition.field];
    delete afterCopy[transition.field];
    if (JSON.stringify(beforeCopy) !== JSON.stringify(afterCopy) || before[transition.field] !== transition.before || after[transition.field] !== transition.after) {
      throw new Error(`expected record transition is not authority-approved: ${caseId}`);
    }
    changed.push({
      caseId,
      oldTuple: { disposition: before.disposition, terminalPhase: before.terminalPhase, semanticErrorCategory: before.semanticErrorCategory ?? null },
      newTuple: { disposition: after.disposition, terminalPhase: after.terminalPhase, semanticErrorCategory: after.semanticErrorCategory ?? null },
      authorityRefs: after.authorityRuleRefs,
      reasonProviderOutputWasNotUsed: "Current Authority terminal-phase and geometry accounting rules independently define this transition.",
    });
  }
  for (const record of current.values()) if (record.provenance !== "AUTHORITY_MANUAL") throw new Error(`expected provenance is not AUTHORITY_MANUAL: ${record.caseId}`);
  return changed.sort((left, right) => left.caseId.localeCompare(right.caseId));
}

function sha256(value) {
  return createHash("sha256").update(value).digest("hex");
}

function assertObservations(path) {
  const input = readFileSync(resolve(repoRoot, path));
  const evidence = JSON.parse(input);
  if (evidence.format !== "axiom-gt-g1-04-c-plan-projection-v2" || evidence.formatVersion !== 2 || evidence.factsOnly !== true) throw new Error("observation input trust envelope is invalid");
  if (evidence.acceptedCases !== 90 || evidence.observationCount !== 180 || evidence.noMutationObservations !== 180 || evidence.unexpectedHarnessErrors !== 0 || evidence.expectedTruthReads !== 0 || evidence.semanticCodecCalls !== 0 || !Array.isArray(evidence.observationRecords) || evidence.observationRecords.length !== 180) {
    throw new Error("observation input facts are incomplete");
  }
  const observed = new Set();
  let reference = 0;
  let indexed = 0;
  let unchanged = 0;
  for (const observation of evidence.observationRecords) {
    if (observation.format !== "axiom-g1-04-c-observation-v1" || observation.formatVersion !== 1 || observation.provenance !== "IMPLEMENTATION_OBSERVATION") throw new Error("observation record provenance is invalid");
    if (observation.provider !== "reference" && observation.provider !== "indexed") throw new Error(`unexpected provider: ${observation.provider}`);
    const key = `${observation.caseId}:${observation.provider}`;
    if (observed.has(key)) throw new Error(`duplicate observation: ${key}`);
    observed.add(key);
    if (observation.provider === "reference") ++reference;
    else ++indexed;
    if (isDeepStrictEqual(observation.beforeProjection, observation.afterProjection)) ++unchanged;
  }
  if (reference !== 90 || indexed !== 90 || unchanged !== 180) throw new Error("observation provider or no-mutation inventory is invalid");
  const negativeZero = evidence.decodedInputAudit?.negativeZero;
  if (negativeZero?.caseId !== "C1-TRANSFORM-NEGATIVE-ZERO" || negativeZero.requiredBits !== "8000000000000000") {
    throw new Error("negative-zero decoder-boundary audit is missing");
  }
  for (const provider of ["reference", "indexed"]) {
    if (!Array.isArray(negativeZero.byProvider?.[provider]) || !negativeZero.byProvider[provider].includes("8000000000000000")) {
      throw new Error(`negative-zero decoder-boundary audit is incomplete for ${provider}`);
    }
  }
  return { evidence, sha256: sha256(input), reference, indexed, unchanged, negativeZero };
}

function statusCounts(results) {
  return Object.fromEntries(["PASS", "FAIL", "OBSERVATION_ONLY"].map((status) => [status, results.filter((result) => result.status === status).length]));
}

function generateEvidence(sourceRef, observationsPath) {
  assertCommit(EXECUTION_START_REF, "execution start ref");
  assertCommit(BLOCKED_C7_SOURCE_REF, "blocked C7 source ref");
  assertCommit(sourceRef, "source ref");
  requireAncestor(EXECUTION_START_REF, sourceRef, "execution start ref");
  requireNoDelta(EXECUTION_START_REF, sourceRef, ["runtime/semantic/src", "runtime/semantic/include/canvas/semantic", "proto", "schema"], "production semantic, schema, or public ABI");
  const changedSourceFiles = assertSourceScope(sourceRef);
  const changedExpectedRecords = assertExpectedTransitions(sourceRef);
  const observation = assertObservations(observationsPath);
  const cases = gitJson(sourceRef, paths.cases);
  const expected = gitJson(sourceRef, paths.expected);
  const suite = gitJson(sourceRef, paths.suite);
  const manifestBlob = gitBlob(sourceRef, paths.manifest);
  const manifest = JSON.parse(manifestBlob);
  if (manifest.caseCount !== 90 || manifest.blockingCaseCount !== 90 || !Array.isArray(manifest.entries) || manifest.entries.length !== 90) throw new Error("generated manifest is not the accepted 90-case inventory");
  const run = runCoreCorpus({ cases, expected, suite, c3Evidence: observation.evidence, c3SourceRef: sourceRef });
  const counts = statusCounts(run.results);
  const providerDivergence = run.results.filter((result) => result.diagnostics?.includes("PROVIDER_DIVERGENCE")).length;
  const planProjection = {
    ...observation.evidence,
    sourceRef,
    executionStartRef: EXECUTION_START_REF,
    observationInputSha256: observation.sha256,
  };
  const noMutation = {
    format: "axiom-gt-g1-04-c-p36-no-mutation-v1",
    formatVersion: 1,
    stage: "P36",
    task: "GT-G1-04-C/manual-golden-correctness",
    sourceRef,
    acceptedCases: 90,
    observationCount: 180,
    referenceObservations: observation.reference,
    indexedObservations: observation.indexed,
    beforeAfterEqual: `${observation.unchanged}/180`,
    unexpectedHarnessErrors: 0,
    observerMutationCalls: 0,
    expectedTruthReads: 0,
    semanticCodecCalls: 0,
  };
  const coreCorpus = {
    format: "axiom-gt-g1-04-c-core-corpus-v1",
    formatVersion: 1,
    packageRef: "12832ab5df9d8638ad1712b182d402d2a0e4d311",
    sourceRef,
    taskAnchor: { revision: EXECUTION_START_REF, relation: "ancestor" },
    p36: true,
    acceptedBasis: { observationInputSha256: observation.sha256 },
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
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
    resultStatusCounts: counts,
    results: run.results,
  };
  const summary = {
    format: "axiom-gt-g1-04-c-p36-verification-golden-repair-v1",
    formatVersion: 1,
    stage: "P36",
    task: "GT-G1-04-C/manual-golden-correctness",
    executionStartRef: EXECUTION_START_REF,
    blockedC7SourceRef: BLOCKED_C7_SOURCE_REF,
    sourceRef,
    p35: { primary: "TEST_DEFECT", secondary: ["EVIDENCE_GAP"] },
    productionSemanticDelta: 0,
    authorityManualExpected: true,
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    changedSourceFiles,
    changedExpectedRecords,
    fixtureManifest: { path: paths.manifest, sha256: sha256(manifestBlob), caseCount: manifest.caseCount },
    negativeZeroDecoderBoundary: observation.negativeZero,
    corpus: {
      cases: 90,
      observations: 180,
      reference: observation.reference,
      indexed: observation.indexed,
      unexpectedHarnessErrors: 0,
      noMutation: observation.unchanged,
    },
    providerDivergence,
    manualGolden: { pass: counts.PASS, fail: counts.FAIL, observationOnly: counts.OBSERVATION_ONLY },
    historicalC7Supersession: {
      sourceRef: BLOCKED_C7_SOURCE_REF,
      materializedRef: EXECUTION_START_REF,
      historicalResult: "61 PASS / 29 FAIL remains immutable",
    },
  };
  return {
    "C-PLAN-PROJECTION.json": planProjection,
    "C-NO-MUTATION.json": noMutation,
    "C-CORE-CORPUS.json": coreCorpus,
    "P36-VERIFICATION-GOLDEN-REPAIR.json": summary,
  };
}

function main() {
  try {
    const { sourceRef, observationsPath, outputDir } = parseArgs(process.argv.slice(2));
    const output = assertSafeOutputPath(outputDir);
    const evidence = generateEvidence(sourceRef, observationsPath);
    mkdirSync(output, { recursive: true });
    for (const [name, value] of Object.entries(evidence)) writeFileSync(resolve(output, name), `${JSON.stringify(value, null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
