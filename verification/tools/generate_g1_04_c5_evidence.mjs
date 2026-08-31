import { execFileSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { runCoreCorpus } from "../packages/semantic-conformance-cli/dist/core-corpus.js";

const PACKAGE_REF = "12832ab5df9d8638ad1712b182d402d2a0e4d311";
const TASK_ANCHOR = "34584c185d8db84034faeb9c3607b92e495ca8f2";
const C3_SOURCE_REF = "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a";
const C3_MATERIALIZED_REF = "855c114f36e4d4d4b9db9faaa28b96ae6d5249c6";
const C4_SOURCE_REF = "06b50a781a7b36d6c19f920711ccd416e455aa0b";
const C4_MATERIALIZED_REF = "34584c185d8db84034faeb9c3607b92e495ca8f2";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");

const PATHS = {
  cases: "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json",
  expected: "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json",
  suite: "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
  generatedManifest: "verification/corpus/semantic/v1/g1-04-c/generated/manifest.json",
  c3Evidence: "verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json",
  c4Evidence: "verification/evidence/gates/G1/06b50a781a7b36d6c19f920711ccd416e455aa0b/GT-G1-04-C/C-COORDINATOR-CONTRACT.json",
};

const C5_SOURCE_PATHS = new Set([
  "verification/packages/semantic-conformance-cli/src/core-corpus.ts",
  "verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs",
  "verification/packages/semantic-conformance-cli/test/core-corpus-evidence.test.mjs",
  "verification/tools/generate_g1_04_c5_evidence.mjs",
]);

function git(args) {
  return execFileSync("git", args, { cwd: repoRoot, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  return JSON.parse(git(["show", `${ref}:${path}`]));
}

function verifyCommit(ref, label) {
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
  if (argv.length !== 4 || argv[0] !== "--source-ref" || argv[2] !== "--output" || !argv[1] || !argv[3]) {
    throw new Error("usage: generate_g1_04_c5_evidence.mjs --source-ref <sha> --output <path>");
  }
  return { sourceRef: argv[1], output: argv[3] };
}

function assertSafeOutputPath(outputPath) {
  const output = resolve(repoRoot, outputPath);
  const forbidden = [
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/authoring"),
    resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/generated"),
    resolve(verificationRoot, "schemas"),
    resolve(repoRoot, "runtime"),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) {
    throw new Error(`forbidden evidence output path: ${output}`);
  }
  return output;
}

function assertSourceScope(sourceRef) {
  const changed = git(["diff", "--name-only", PACKAGE_REF, sourceRef]).split("\n").filter(Boolean);
  if (changed.length !== C5_SOURCE_PATHS.size || changed.some((path) => !C5_SOURCE_PATHS.has(path))) {
    throw new Error(`C5 source scope mismatch: ${changed.join(", ")}`);
  }
}

function assertAcceptedC1Blobs(sourceRef) {
  for (const path of [PATHS.cases, PATHS.expected, PATHS.suite]) {
    const anchorBlob = git(["rev-parse", `--verify`, `${TASK_ANCHOR}:${path}`]);
    const sourceBlob = git(["rev-parse", `--verify`, `${sourceRef}:${path}`]);
    if (anchorBlob !== sourceBlob) throw new Error(`accepted C1 blob changed: ${path}`);
  }
}

function assertAcceptedC2Manifest(manifest) {
  if (manifest.caseCount !== 90 || manifest.blockingCaseCount !== 90 || !Array.isArray(manifest.entries) || manifest.entries.length !== 90) {
    throw new Error("accepted C2 generated manifest is not the 90-case inventory");
  }
}

function assertAcceptedC3Evidence(evidence) {
  if (evidence.format !== "axiom-gt-g1-04-c-plan-projection-v2" || evidence.formatVersion !== 2 || evidence.factsOnly !== true) {
    throw new Error("accepted C3 evidence trust envelope is invalid");
  }
  if (evidence.acceptedCases !== 90 || evidence.observationCount !== 180 || evidence.noMutationObservations !== 180 || !Array.isArray(evidence.observationRecords) || evidence.observationRecords.length !== 180) {
    throw new Error("accepted C3 evidence inventory is not 90 cases / 180 observations / 180 no-mutation observations");
  }
}

function assertAcceptedC4Evidence(evidence) {
  if (evidence.format !== "axiom-gt-g1-04-c-coordinator-contract-v1" || evidence.sourceRef !== C4_SOURCE_REF) {
    throw new Error("accepted C4 evidence identity is invalid");
  }
  if (evidence.syntheticScenarioCount !== 10 || evidence.syntheticScenarioPassCount !== 10 || evidence.observationRefBoundaryScenarioCount !== 5 || evidence.observationRefBoundaryScenarioPassCount !== 5 || evidence.acceptedExpectedCount !== 90 || evidence.acceptedOpenPolicyCount !== 0 || evidence.expectedTruthWrites !== 0 || evidence.productionSemanticDependencies !== 0 || evidence.providerOutputUsedAsExpected !== false) {
    throw new Error("accepted C4 evidence facts are incomplete");
  }
}

export function generateEvidence(sourceRef) {
  verifyCommit(sourceRef, "source ref");
  requireAncestor(TASK_ANCHOR, sourceRef, "task anchor");
  requireAncestor(PACKAGE_REF, sourceRef, "C5 package ref");
  assertSourceScope(sourceRef);
  assertAcceptedC1Blobs(sourceRef);
  requireCleanDiff(C3_SOURCE_REF, sourceRef, ["runtime/semantic/include/canvas/semantic", "runtime/semantic/src"], "production semantic source");

  const cases = gitJson(sourceRef, PATHS.cases);
  const expected = gitJson(sourceRef, PATHS.expected);
  const suite = gitJson(sourceRef, PATHS.suite);
  const generatedManifest = gitJson(sourceRef, PATHS.generatedManifest);
  const c3Evidence = gitJson(C3_MATERIALIZED_REF, PATHS.c3Evidence);
  const c4Evidence = gitJson(C4_MATERIALIZED_REF, PATHS.c4Evidence);
  assertAcceptedC2Manifest(generatedManifest);
  assertAcceptedC3Evidence(c3Evidence);
  assertAcceptedC4Evidence(c4Evidence);

  const run = runCoreCorpus({
    cases,
    expected,
    suite,
    c3Evidence,
    c3SourceRef: C3_SOURCE_REF,
  });
  const statusCounts = Object.fromEntries(["PASS", "FAIL", "OBSERVATION_ONLY"].map((status) => [status, run.results.filter((result) => result.status === status).length]));
  return {
    format: "axiom-gt-g1-04-c-core-corpus-v1",
    formatVersion: 1,
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    authority: {
      verification: "notion:3cc4c57a-590c-81ae-ab73-d75501c47169",
      implementationPlan: "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b",
    },
    acceptedBasis: {
      c3SourceRef: C3_SOURCE_REF,
      c3MaterializedRef: C3_MATERIALIZED_REF,
      c4SourceRef: C4_SOURCE_REF,
      c4MaterializedRef: C4_MATERIALIZED_REF,
    },
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
    productionSemanticDeltaFromC3: 0,
    resultStatusCounts: statusCounts,
    results: run.results,
  };
}

function main() {
  try {
    const { sourceRef, output } = parseArgs(process.argv.slice(2));
    const outputPath = assertSafeOutputPath(output);
    mkdirSync(dirname(outputPath), { recursive: true });
    writeFileSync(outputPath, `${JSON.stringify(generateEvidence(sourceRef), null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
