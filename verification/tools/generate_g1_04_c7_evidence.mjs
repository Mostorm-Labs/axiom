import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { summarizeProviderDiff } from "../packages/semantic-conformance-cli/dist/provider-diff.js";
import { evaluateC7Gate } from "../packages/semantic-conformance-cli/dist/gate.js";

const PACKAGE_REF = "e4facb7ab786b994dd5c9bb4bc3e4d57fe95d18e";
const TASK_ANCHOR = "9b73be589ae070bc602b8989f83d89745a54774e";
const P36_SOURCE_REF = "492d2f914f078a6e4ac8b567e07f7ec813c10107";
const P36_MATERIALIZED_REF = "9b73be589ae070bc602b8989f83d89745a54774e";
const P20_REF = "notion:3cc4c57a-590c-81ae-ab73-d75501c47169";
const P30_REF = "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");

const SOURCE_PATHS = [
  "verification/packages/semantic-conformance-cli/src/provider-diff.ts",
  "verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs",
  "verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs",
  "verification/tools/generate_g1_04_c7_evidence.mjs",
];

const PATHS = {
  authoring: "verification/corpus/semantic/v1/g1-04-c/authoring",
  generated: "verification/corpus/semantic/v1/g1-04-c/generated",
  suite: "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
  compiler: "verification/fixture-author/compile_g1_04_c.py",
  gateSchema: "verification/schemas/semantic/g1-04-c-gate.schema.json",
  coreCorpus: `verification/evidence/gates/G1/${P36_SOURCE_REF}/GT-G1-04-C/C-CORE-CORPUS.json`,
  noMutation: `verification/evidence/gates/G1/${P36_SOURCE_REF}/GT-G1-04-C/C-NO-MUTATION.json`,
  planProjection: `verification/evidence/gates/G1/${P36_SOURCE_REF}/GT-G1-04-C/C-PLAN-PROJECTION.json`,
  goldenRepair: `verification/evidence/gates/G1/${P36_SOURCE_REF}/GT-G1-04-C/P36-VERIFICATION-GOLDEN-REPAIR.json`,
};

function git(args) {
  return execFileSync("git", args, { cwd: repoRoot, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  return JSON.parse(git(["show", `${ref}:${path}`]));
}

function requireCommit(ref, label) {
  if (!/^[0-9a-f]{40}$/.test(ref)) throw new Error(`${label} must be a full immutable SHA`);
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

function requireNoDelta(from, to, paths, label) {
  try {
    execFileSync("git", ["diff", "--quiet", from, to, "--", ...paths], { cwd: repoRoot, stdio: "ignore" });
  } catch {
    throw new Error(`${label} changed unexpectedly`);
  }
}

function parseArgs(argv) {
  if (argv.length !== 4 || argv[0] !== "--source-ref" || !argv[1] || argv[2] !== "--output-dir" || !argv[3]) {
    throw new Error("usage: generate_g1_04_c7_evidence.mjs --source-ref <sha> --output-dir <directory>");
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
    resolve(verificationRoot, "evidence/gates/G1", P36_SOURCE_REF),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) throw new Error(`forbidden evidence output path: ${output}`);
  return output;
}

function assertSourceScope(sourceRef) {
  const entries = git(["diff", "--name-status", PACKAGE_REF, sourceRef]).split("\n").filter(Boolean);
  const paths = entries.map((entry) => entry.split("\t"));
  if (paths.length !== SOURCE_PATHS.length || paths.some(([status, path]) => status !== "M" || !SOURCE_PATHS.includes(path))) {
    throw new Error(`C7 source scope mismatch: ${entries.join(", ")}`);
  }
}

function assertAcceptedRoots(sourceRef) {
  requireNoDelta(TASK_ANCHOR, sourceRef, [
    PATHS.authoring,
    PATHS.generated,
    PATHS.suite,
    PATHS.compiler,
    PATHS.gateSchema,
    `verification/evidence/gates/G1/${P36_SOURCE_REF}/GT-G1-04-C`,
    "runtime/semantic",
    "schema/axiom/v1/proto",
  ], "accepted authoring, fixture, schema, evidence, or production semantic roots");
}

function record(value, label) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

function assertP36Artifacts({ coreCorpus, noMutation, planProjection, goldenRepair }) {
  if (coreCorpus.sourceRef !== P36_SOURCE_REF || coreCorpus.format !== "axiom-gt-g1-04-c-core-corpus-v1" || coreCorpus.formatVersion !== 1 || coreCorpus.p36 !== true) throw new Error("P36 core-corpus identity is invalid");
  if (coreCorpus.productionSemanticDelta !== 0 || coreCorpus.acceptedExpectedAllAuthorityManual !== true || coreCorpus.expectedTruthWrites !== 0 || coreCorpus.providerOutputUsedAsExpected !== false) throw new Error("P36 core-corpus trust facts are invalid");
  if (coreCorpus.selectedCaseCount !== 90 || coreCorpus.selectedExpectedCount !== 90 || coreCorpus.selectedObservationCount !== 180 || coreCorpus.operationFamilyCount !== 15) throw new Error("P36 core-corpus inventory is invalid");
  if (!["missingMandatoryFamilies", "wrongFamilyCaseIds", "unselectedCaseIds", "duplicateSuiteCaseIds"].every((field) => Array.isArray(coreCorpus[field]) && coreCorpus[field].length === 0)) throw new Error("P36 core-corpus mandatory selection is invalid");
  if (coreCorpus.acceptedOpenPolicyCount !== 0 || coreCorpus.resultStatusCounts?.PASS !== 90 || coreCorpus.resultStatusCounts?.FAIL !== 0 || coreCorpus.resultStatusCounts?.OBSERVATION_ONLY !== 0) throw new Error("P36 core-corpus result inventory is invalid");

  if (noMutation.format !== "axiom-gt-g1-04-c-p36-no-mutation-v1" || noMutation.formatVersion !== 1 || noMutation.stage !== "P36" || noMutation.sourceRef !== P36_SOURCE_REF) throw new Error("P36 no-mutation identity is invalid");
  if (noMutation.acceptedCases !== 90 || noMutation.observationCount !== 180 || noMutation.referenceObservations !== 90 || noMutation.indexedObservations !== 90 || noMutation.beforeAfterEqual !== "180/180" || noMutation.unexpectedHarnessErrors !== 0 || noMutation.observerMutationCalls !== 0 || noMutation.expectedTruthReads !== 0 || noMutation.semanticCodecCalls !== 0) throw new Error("P36 no-mutation facts are invalid");

  if (goldenRepair.format !== "axiom-gt-g1-04-c-p36-verification-golden-repair-v1" || goldenRepair.formatVersion !== 1 || goldenRepair.stage !== "P36" || goldenRepair.sourceRef !== P36_SOURCE_REF) throw new Error("P36 golden-repair identity is invalid");
  if (goldenRepair.productionSemanticDelta !== 0 || goldenRepair.authorityManualExpected !== true || goldenRepair.expectedTruthWrites !== 0 || goldenRepair.providerOutputUsedAsExpected !== false || goldenRepair.providerDivergence !== 0 || goldenRepair.manualGolden?.pass !== 90 || goldenRepair.manualGolden?.fail !== 0 || goldenRepair.manualGolden?.observationOnly !== 0 || goldenRepair.corpus?.cases !== 90 || goldenRepair.corpus?.observations !== 180 || goldenRepair.corpus?.reference !== 90 || goldenRepair.corpus?.indexed !== 90 || goldenRepair.corpus?.noMutation !== 180 || goldenRepair.corpus?.unexpectedHarnessErrors !== 0) throw new Error("P36 golden-repair facts are invalid");
  if (planProjection.sourceRef !== P36_SOURCE_REF || planProjection.factsOnly !== true || planProjection.acceptedCases !== 90 || planProjection.observationCount !== 180 || planProjection.expectedTruthReads !== 0 || planProjection.semanticCodecCalls !== 0 || planProjection.unexpectedHarnessErrors !== 0) throw new Error("P36 plan-projection facts are invalid");
}

function listFiles(root) {
  const files = [];
  function visit(current) {
    for (const name of readdirSync(current).sort()) {
      const path = resolve(current, name);
      const info = statSync(path);
      if (info.isDirectory()) visit(path);
      else if (info.isFile()) files.push(relative(root, path));
      else throw new Error(`unsupported filesystem entry: ${path}`);
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

function compileFixturesTwice() {
  const authoringRoot = resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/authoring");
  const generatedRoot = resolve(verificationRoot, "corpus/semantic/v1/g1-04-c/generated");
  const beforeAuthoring = treeDigest(authoringRoot);
  const beforeGenerated = treeDigest(generatedRoot);
  const temporaryRoot = mkdtempSync(resolve(tmpdir(), "g1-04-c7-fixtures-"));
  const first = resolve(temporaryRoot, "first");
  const second = resolve(temporaryRoot, "second");
  for (const output of [first, second]) execFileSync("python3", [resolve(verificationRoot, "fixture-author/compile_g1_04_c.py"), "--output", output], { cwd: repoRoot, stdio: "pipe" });
  const firstFiles = listFiles(first);
  const secondFiles = listFiles(second);
  if (JSON.stringify(firstFiles) !== JSON.stringify(secondFiles)) throw new Error("fixture regeneration file inventory differs");
  for (const path of firstFiles) if (!readFileSync(resolve(first, path)).equals(readFileSync(resolve(second, path)))) throw new Error(`fixture regeneration differs: ${path}`);
  const manifest = JSON.parse(readFileSync(resolve(first, "manifest.json"), "utf8"));
  if (manifest.caseCount !== 90 || manifest.blockingCaseCount !== 90 || !Array.isArray(manifest.entries) || manifest.entries.length !== 90) throw new Error("fixture regeneration does not produce the accepted 90-case inventory");
  const afterAuthoring = treeDigest(authoringRoot);
  const afterGenerated = treeDigest(generatedRoot);
  if (beforeAuthoring !== afterAuthoring || beforeGenerated !== afterGenerated) throw new Error("fixture compiler wrote to accepted authority or generated roots");
  return {
    status: "PASS",
    caseCount: 90,
    byteIdentical: true,
    fileCount: firstFiles.length,
    authoringRootWrites: 0,
    generatedRootWrites: 0,
    treeSha256: treeDigest(first),
  };
}

function condition(id, status, evidenceRefs) {
  return { id, status, evidenceRefs };
}

function assertGateSchema(gate, sourceRef, gateSummary) {
  const keys = Object.keys(gate).sort();
  const expected = ["authorityRefs", "format", "formatVersion", "gateId", "provenance", "resultRefs", "sourceRef", "status"];
  if (JSON.stringify(keys) !== JSON.stringify(expected)) throw new Error("C-GATE schema has unexpected fields");
  if (gate.format !== "axiom-g1-04-c-gate-v1" || gate.formatVersion !== 1 || gate.provenance !== "GATE_EVIDENCE" || gate.gateId !== "GT-G1-04-C" || (gate.status !== "PASS" && gate.status !== "FAIL") || gate.sourceRef !== sourceRef) throw new Error("C-GATE schema values are invalid");
  if (gate.status !== gateSummary.status) throw new Error("C-GATE status does not match the computed gate summary");
  for (const field of ["resultRefs", "authorityRefs"]) if (!Array.isArray(gate[field]) || gate[field].some((value) => typeof value !== "string" || value.length === 0) || new Set(gate[field]).size !== gate[field].length) throw new Error(`C-GATE ${field} is invalid`);
  if (gate.resultRefs.length === 0) throw new Error("C-GATE requires result refs");
}

export function generateEvidence(sourceRef) {
  requireCommit(PACKAGE_REF, "C7 package ref");
  requireCommit(TASK_ANCHOR, "C7 task anchor");
  requireCommit(P36_SOURCE_REF, "P36 source ref");
  requireCommit(P36_MATERIALIZED_REF, "P36 materialized ref");
  requireCommit(sourceRef, "source ref");
  requireAncestor(TASK_ANCHOR, sourceRef, "task anchor");
  requireAncestor(PACKAGE_REF, sourceRef, "C7 package ref");
  assertSourceScope(sourceRef);
  assertAcceptedRoots(sourceRef);

  const p36 = {
    coreCorpus: gitJson(P36_MATERIALIZED_REF, PATHS.coreCorpus),
    noMutation: gitJson(P36_MATERIALIZED_REF, PATHS.noMutation),
    planProjection: gitJson(P36_MATERIALIZED_REF, PATHS.planProjection),
    goldenRepair: gitJson(P36_MATERIALIZED_REF, PATHS.goldenRepair),
  };
  assertP36Artifacts(p36);
  const providerDiff = summarizeProviderDiff({ coreCorpusEvidence: p36.coreCorpus, noMutationEvidence: p36.noMutation });
  if (providerDiff.status !== "PASS" || providerDiff.providerAgreement !== "90/90" || providerDiff.divergenceCount !== 0 || providerDiff.goldenPassCount !== 90 || providerDiff.goldenFailCount !== 0 || providerDiff.observationOnlyCount !== 0 || providerDiff.goldenMismatchCaseIds.length !== 0 || providerDiff.manualGoldenCorrectness !== "PASS") throw new Error("P36 provider differential facts are invalid");
  const fixtureReproducibility = compileFixturesTwice();
  const coreCorpusRef = `git:${P36_MATERIALIZED_REF}:${PATHS.coreCorpus}`;
  const noMutationRef = `git:${P36_MATERIALIZED_REF}:${PATHS.noMutation}`;
  const planProjectionRef = `git:${P36_MATERIALIZED_REF}:${PATHS.planProjection}`;
  const goldenRepairRef = `git:${P36_MATERIALIZED_REF}:${PATHS.goldenRepair}`;
  const providerDiffRef = `g1-04-c://c7/${sourceRef}/C-PROVIDER-DIFF.json`;
  const gateSummary = evaluateC7Gate({
    conditions: [
      condition("authority-provenance", "PASS", [coreCorpusRef, goldenRepairRef]),
      condition("mandatory-corpus", "PASS", [coreCorpusRef]),
      condition("manual-golden-correctness", providerDiff.manualGoldenCorrectness, [coreCorpusRef, goldenRepairRef]),
      condition("no-mutation", "PASS", [noMutationRef]),
      condition("provider-differential", providerDiff.status, [providerDiffRef, noMutationRef]),
      condition("fixture-reproducibility", fixtureReproducibility.status, ["g1-04-c://c7/fixture-reproducibility"]),
      condition("open-reconciliation", "PASS", [coreCorpusRef]),
    ],
  });
  if (gateSummary.status !== "PASS" || gateSummary.failedConditions.length !== 0) throw new Error("C7 Gate does not satisfy all seven passing conditions");

  const envelope = {
    gate: "GT-G1-04-C",
    slice: "C7",
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    authority: { verification: P20_REF, implementationPlan: P30_REF },
    trustedDependencies: {
      p36SourceRef: P36_SOURCE_REF,
      p36MaterializedRef: P36_MATERIALIZED_REF,
      coreCorpusRef,
      noMutationRef,
      planProjectionRef,
      goldenRepairRef,
    },
    authorityManualExpected: true,
    expectedTruthWrites: 0,
    providerOutputUsedAsExpected: false,
    productionSemanticDelta: 0,
  };
  const differential = {
    ...envelope,
    format: "axiom-gt-g1-04-c7-provider-diff-v1",
    formatVersion: 1,
    ...providerDiff,
    fixtureReproducibility,
    gateSummary,
  };
  const gate = {
    format: "axiom-g1-04-c-gate-v1",
    formatVersion: 1,
    provenance: "GATE_EVIDENCE",
    gateId: "GT-G1-04-C",
    status: gateSummary.status,
    resultRefs: [providerDiffRef, coreCorpusRef, noMutationRef, planProjectionRef, goldenRepairRef],
    authorityRefs: [P20_REF, P30_REF],
    sourceRef,
  };
  assertGateSchema(gate, sourceRef, gateSummary);
  return { "C-PROVIDER-DIFF.json": differential, "C-GATE.json": gate };
}

function main() {
  try {
    const { sourceRef, outputDir } = parseArgs(process.argv.slice(2));
    const output = assertSafeOutputPath(outputDir);
    if (existsSync(output) && readdirSync(output).length !== 0) throw new Error(`evidence output directory must be empty: ${output}`);
    const evidence = generateEvidence(sourceRef);
    mkdirSync(output, { recursive: true });
    for (const [name, value] of Object.entries(evidence)) writeFileSync(resolve(output, name), `${JSON.stringify(value, null, 2)}\n`);
  } catch (error) {
    process.stderr.write(`${error instanceof Error ? error.message : String(error)}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
