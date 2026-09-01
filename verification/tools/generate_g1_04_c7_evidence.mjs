import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { summarizeProviderDiff } from "../packages/semantic-conformance-cli/dist/provider-diff.js";
import { evaluateC7Gate } from "../packages/semantic-conformance-cli/dist/gate.js";

const PACKAGE_REF = "abe3bcb2b86d8f68b8b36d431c5a4e6e92a7593e";
const TASK_ANCHOR = "0213e1d910f43fde14a23577f0d9265b7521869d";
const C5_SOURCE_REF = "906327beb9a268c339accd6d3ca6a7038e54ad68";
const C5_MATERIALIZED_REF = "2a601ab35a2294cb38d713ab001bbac7deaa9cf7";
const C6_SOURCE_REF = "2bd2a2fa6502163d471995147daa683cefd7cf8f";
const C6_MATERIALIZED_REF = TASK_ANCHOR;
const CORRECTED_C3_SOURCE_REF = C5_SOURCE_REF;
const CORRECTED_C3_MATERIALIZED_REF = C5_MATERIALIZED_REF;
const P20_REF = "notion:3cc4c57a-590c-81ae-ab73-d75501c47169";
const P30_REF = "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b";
const verificationRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const repoRoot = resolve(verificationRoot, "..");

const SOURCE_PATHS = [
  "verification/packages/semantic-conformance-cli/src/provider-diff.ts",
  "verification/packages/semantic-conformance-cli/src/gate.ts",
  "verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs",
  "verification/packages/semantic-conformance-cli/test/gate.test.mjs",
  "verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs",
  "verification/tools/generate_g1_04_c7_evidence.mjs",
];

const PATHS = {
  authoring: "verification/corpus/semantic/v1/g1-04-c/authoring",
  generated: "verification/corpus/semantic/v1/g1-04-c/generated",
  suite: "verification/corpus/semantic/v1/g1-04-c/suites/core.json",
  compiler: "verification/fixture-author/compile_g1_04_c.py",
  gateSchema: "verification/schemas/semantic/g1-04-c-gate.schema.json",
  c5CoreCorpus: `verification/evidence/gates/G1/${C5_SOURCE_REF}/GT-G1-04-C/C-CORE-CORPUS.json`,
  c6Idempotency: `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-IDEMPOTENCY.json`,
  c6NoMutation: `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-NO-MUTATION.json`,
  c6PlanProjection: `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-PLAN-PROJECTION.json`,
  c6OpenReconciliation: `verification/evidence/gates/G1/${C6_SOURCE_REF}/GT-G1-04-C/C-OPEN-RECONCILIATION.json`,
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
    resolve(verificationRoot, "evidence/gates/G1", C5_SOURCE_REF),
    resolve(verificationRoot, "evidence/gates/G1", C6_SOURCE_REF),
  ];
  if (forbidden.some((root) => output === root || output.startsWith(`${root}/`))) throw new Error(`forbidden evidence output path: ${output}`);
  return output;
}

function assertSourceScope(sourceRef) {
  const entries = git(["diff", "--name-status", PACKAGE_REF, sourceRef]).split("\n").filter(Boolean);
  const paths = entries.map((entry) => entry.split("\t"));
  if (paths.length !== SOURCE_PATHS.length || paths.some(([status, path]) => status !== "A" || !SOURCE_PATHS.includes(path))) {
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
    "verification/evidence/gates/G1",
    "runtime/semantic",
    "schema/axiom/v1/proto",
  ], "accepted authoring, fixture, schema, evidence, or production semantic roots");
}

function record(value, label) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

function ensureC6Envelope(value, format, label) {
  const evidence = record(value, label);
  if (evidence.format !== format || evidence.formatVersion !== 1 || evidence.status !== "PASS" || evidence.sourceRef !== C6_SOURCE_REF) throw new Error(`${label} identity or status is invalid`);
  if (evidence.authorityManualExpected !== true || evidence.expectedTruthWrites !== 0 || evidence.providerOutputUsedAsExpected !== false || evidence.productionSemanticDelta !== 0) {
    throw new Error(`${label} trust facts are invalid`);
  }
  return evidence;
}

function assertC6Evidence(c6) {
  const idempotency = ensureC6Envelope(c6.idempotency, "axiom-gt-g1-04-c6-idempotency-v1", "C6 idempotency evidence");
  const noMutation = ensureC6Envelope(c6.noMutation, "axiom-gt-g1-04-c6-no-mutation-v1", "C6 no-mutation evidence");
  const planProjection = ensureC6Envelope(c6.planProjection, "axiom-gt-g1-04-c6-plan-projection-v1", "C6 plan-projection evidence");
  const openReconciliation = ensureC6Envelope(c6.openReconciliation, "axiom-gt-g1-04-c6-open-reconciliation-v1", "C6 open-reconciliation evidence");
  if (noMutation.acceptedCases !== 90 || noMutation.observationCount !== 180 || noMutation.providerAgreement !== "90/90" || noMutation.beforeAfterEqual !== "180/180" || noMutation.observerMutationCalls !== 0) throw new Error("C6 no-mutation inventory is invalid");
  if (planProjection.factsOnly !== true || openReconciliation.unknownFuturePolicyKeysRemainUnknown !== true || typeof idempotency.orderingProof !== "string") throw new Error("C6 accepted aggregate evidence is incomplete");
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

function assertGateSchema(gate, sourceRef) {
  const keys = Object.keys(gate).sort();
  const expected = ["authorityRefs", "format", "formatVersion", "gateId", "provenance", "resultRefs", "sourceRef", "status"];
  if (JSON.stringify(keys) !== JSON.stringify(expected)) throw new Error("C-GATE schema has unexpected fields");
  if (gate.format !== "axiom-g1-04-c-gate-v1" || gate.formatVersion !== 1 || gate.provenance !== "GATE_EVIDENCE" || gate.gateId !== "GT-G1-04-C" || gate.status !== "FAIL" || gate.sourceRef !== sourceRef) throw new Error("C-GATE schema values are invalid");
  for (const field of ["resultRefs", "authorityRefs"]) if (!Array.isArray(gate[field]) || gate[field].some((value) => typeof value !== "string" || value.length === 0) || new Set(gate[field]).size !== gate[field].length) throw new Error(`C-GATE ${field} is invalid`);
  if (gate.resultRefs.length === 0) throw new Error("C-GATE requires result refs");
}

export function generateEvidence(sourceRef) {
  requireCommit(PACKAGE_REF, "C7 package ref");
  requireCommit(TASK_ANCHOR, "C7 task anchor");
  requireCommit(C5_MATERIALIZED_REF, "C5 materialized ref");
  requireCommit(C6_MATERIALIZED_REF, "C6 materialized ref");
  requireCommit(sourceRef, "source ref");
  requireAncestor(TASK_ANCHOR, sourceRef, "task anchor");
  requireAncestor(PACKAGE_REF, sourceRef, "C7 package ref");
  assertSourceScope(sourceRef);
  assertAcceptedRoots(sourceRef);

  const c5CoreCorpus = gitJson(C5_MATERIALIZED_REF, PATHS.c5CoreCorpus);
  const c6 = {
    idempotency: gitJson(C6_MATERIALIZED_REF, PATHS.c6Idempotency),
    noMutation: gitJson(C6_MATERIALIZED_REF, PATHS.c6NoMutation),
    planProjection: gitJson(C6_MATERIALIZED_REF, PATHS.c6PlanProjection),
    openReconciliation: gitJson(C6_MATERIALIZED_REF, PATHS.c6OpenReconciliation),
  };
  assertC6Evidence(c6);
  const providerDiff = summarizeProviderDiff({ coreCorpusEvidence: c5CoreCorpus, c6NoMutationEvidence: c6.noMutation });
  const fixtureReproducibility = compileFixturesTwice();
  const c5Ref = `git:${C5_MATERIALIZED_REF}:${PATHS.c5CoreCorpus}`;
  const c6NoMutationRef = `git:${C6_MATERIALIZED_REF}:${PATHS.c6NoMutation}`;
  const c6IdempotencyRef = `git:${C6_MATERIALIZED_REF}:${PATHS.c6Idempotency}`;
  const c6PlanProjectionRef = `git:${C6_MATERIALIZED_REF}:${PATHS.c6PlanProjection}`;
  const c6OpenRef = `git:${C6_MATERIALIZED_REF}:${PATHS.c6OpenReconciliation}`;
  const providerDiffRef = `g1-04-c://c7/${sourceRef}/C-PROVIDER-DIFF.json`;
  const gateSummary = evaluateC7Gate({
    conditions: [
      condition("authority-provenance", "PASS", [c5Ref, c6NoMutationRef]),
      condition("mandatory-corpus", "PASS", [c5Ref]),
      condition("manual-golden-correctness", providerDiff.manualGoldenCorrectness, [c5Ref]),
      condition("no-mutation", "PASS", [c6NoMutationRef]),
      condition("provider-differential", providerDiff.status, [providerDiffRef, c6NoMutationRef]),
      condition("fixture-reproducibility", fixtureReproducibility.status, ["g1-04-c://c7/fixture-reproducibility"]),
      condition("open-reconciliation", "PASS", [c6OpenRef]),
    ],
  });
  if (gateSummary.status !== "FAIL" || JSON.stringify(gateSummary.failedConditions) !== JSON.stringify(["manual-golden-correctness"])) throw new Error("C7 Gate does not preserve the required single manual-golden failure");

  const envelope = {
    gate: "GT-G1-04-C",
    slice: "C7",
    packageRef: PACKAGE_REF,
    sourceRef,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    authority: { verification: P20_REF, implementationPlan: P30_REF },
    trustedDependencies: {
      c5SourceRef: C5_SOURCE_REF,
      c5MaterializedRef: C5_MATERIALIZED_REF,
      c6SourceRef: C6_SOURCE_REF,
      c6MaterializedRef: C6_MATERIALIZED_REF,
      correctedC3SourceRef: CORRECTED_C3_SOURCE_REF,
      correctedC3MaterializedRef: CORRECTED_C3_MATERIALIZED_REF,
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
    resultRefs: [providerDiffRef, c5Ref, c6IdempotencyRef, c6NoMutationRef, c6PlanProjectionRef, c6OpenRef],
    authorityRefs: [P20_REF, P30_REF],
    sourceRef,
  };
  assertGateSchema(gate, sourceRef);
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
