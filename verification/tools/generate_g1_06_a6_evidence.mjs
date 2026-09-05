import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const TASK_ID = "GT-G1-06-A6";
export const PACKAGE_REF = "notion://3d24c57a-590c-81df-ba72-c1b66fac724a/GT-G1-06-A6-P31-v0.2";
export const PACKAGE_PAGE = "3d24c57a-590c-81b4-a317-f24565aff1f4";
export const ORIGINAL_A6_ANCHOR = "3f01f599efee21d9adbd8c4cacdcf953995979f9";
export const INHERITED_V01_SOURCE_REF = "ee9a383039e619926c38cd91dc48475e25d38b0d";
export const PACKAGE_MATERIALIZATION_REF = INHERITED_V01_SOURCE_REF;
export const TASK_ANCHOR = PACKAGE_MATERIALIZATION_REF;
export const INHERITED_V01_PACKAGE_REF = "notion://3d24c57a-590c-813c-85b3-f330715b4120/GT-G1-06-A6-P31-v0.1";
export const BRANCH_REF = "refs/heads/codex/gt-g1-06-snapshot-replay";
export const WORKFLOW_PATH = ".github/workflows/g1-06-exact-source.yml";
export const WORKFLOW_NAME = "GT-G1-06 Exact Source";
export const EXECUTION_REF = "codex/gt-g1-06-snapshot-replay";
export const A5_SOURCE_REF = "a2f0b21a3ff18f441eaa7d3d7702698eca6b5edc";
export const A5_MATERIALIZED_REF = PACKAGE_MATERIALIZATION_REF;
export const A5_PACKAGE_REF = "notion://3d24c57a-590c-81b3-a01f-fd3c70dbb97c/GT-G1-06-A5-P31-v0.3";

export const AUTHORIZED_SOURCE_PATHS = [
  WORKFLOW_PATH,
  "verification/tools/generate_g1_06_a6_evidence.mjs",
  "verification/packages/semantic-conformance-cli/test/g1-06-a6-exact-source-evidence.test.mjs",
  "runtime/semantic/src/protobuf_object_mapping.cpp",
];

export const INHERITED_V01_SOURCE_PATHS = AUTHORIZED_SOURCE_PATHS.slice(0, 3);
export const PRODUCTION_REPAIR_PATH = "runtime/semantic/src/protobuf_object_mapping.cpp";

export const REQUIRED_EVIDENCE_FILES = [
  "G1-06-PLAN.json",
  "G1-06-SNAPSHOT.json",
  "G1-06-REPLAY.json",
  "G1-06-PROJECTION.json",
  "G1-06-DIGEST.json",
  "G1-06-CTEST.txt",
  "G1-06-DIFF.json",
];

export const EXPECTED_PROTOBUF_OFF_SKIP_IDS = [
  ...Array.from({ length: 12 }, (_, index) => index + 25),
  ...Array.from({ length: 11 }, (_, index) => index + 53),
];

const REPOSITORY = "Mostorm-Labs/axiom";
const REPOSITORY_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const EVIDENCE_ROOT = resolve(REPOSITORY_ROOT, "verification/evidence/gates");

function fail(message) {
  throw new Error(message);
}

function asRecord(value, label) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) fail(`${label} must be an object`);
  return value;
}

function asString(value, label) {
  if (typeof value !== "string" || value.length === 0) fail(`${label} must be a non-empty string`);
  return value;
}

export function isSha(value) {
  return typeof value === "string" && /^[0-9a-f]{40}$/.test(value);
}

export function validateSourceRef(value) {
  if (!isSha(value)) fail("source_ref must be a full immutable SHA");
  return value;
}

function git(args) {
  return execFileSync("git", args, { cwd: REPOSITORY_ROOT, encoding: "utf8" }).trim();
}

function gitJson(ref, path) {
  try {
    return JSON.parse(git(["show", `${ref}:${path}`]));
  } catch (error) {
    fail(`unable to read accepted evidence ${ref}:${path}: ${error.message}`);
  }
}

function requireCommit(ref, label) {
  validateSourceRef(ref);
  try {
    return git(["rev-parse", "--verify", `${ref}^{commit}`]);
  } catch {
    fail(`${label} is not a resolvable commit: ${ref}`);
  }
}

function requireAncestor(ancestor, descendant, label) {
  try {
    execFileSync("git", ["merge-base", "--is-ancestor", ancestor, descendant], { cwd: REPOSITORY_ROOT, stdio: "ignore" });
  } catch {
    fail(`${label} is not an ancestor: ${ancestor} -> ${descendant}`);
  }
}

export function validateAncestry({ packageMaterializationRef, taskAnchor, sourceRef, sourceCommitParent }) {
  requireCommit(packageMaterializationRef, "package materialization ref");
  requireCommit(taskAnchor, "task anchor");
  requireCommit(sourceRef, "source ref");
  requireCommit(sourceCommitParent, "source commit parent");
  if (packageMaterializationRef !== taskAnchor) fail("package materialization ref and task anchor differ");
  requireAncestor(taskAnchor, sourceRef, "task anchor");
  return true;
}

export function validateSourceDelta(entries) {
  if (!Array.isArray(entries)) fail("source delta must be an array");
  const normalized = entries.map((entry) => {
    if (!Array.isArray(entry) || entry.length !== 2) fail("source delta entry is malformed");
    return { status: entry[0], path: entry[1] };
  });
  if (normalized.length !== AUTHORIZED_SOURCE_PATHS.length) fail("source delta count is not exactly four");
  const paths = normalized.map((entry) => entry.path);
  if (new Set(paths).size !== AUTHORIZED_SOURCE_PATHS.length) fail("source delta contains duplicate paths");
  for (const entry of normalized) {
    if (entry.status !== "M" || !AUTHORIZED_SOURCE_PATHS.includes(entry.path)) fail(`unauthorized source delta: ${entry.status} ${entry.path}`);
  }
  return normalized;
}

export function validateInheritedV01SourceDelta(entries) {
  if (!Array.isArray(entries)) fail("inherited v0.1 source delta must be an array");
  const normalized = entries.map((entry) => {
    if (!Array.isArray(entry) || entry.length !== 2) fail("inherited v0.1 source delta entry is malformed");
    return { status: entry[0], path: entry[1] };
  });
  if (normalized.length !== INHERITED_V01_SOURCE_PATHS.length) fail("inherited v0.1 source delta count is not exactly three");
  const paths = normalized.map((entry) => entry.path);
  if (new Set(paths).size !== INHERITED_V01_SOURCE_PATHS.length) fail("inherited v0.1 source delta contains duplicate paths");
  for (const entry of normalized) {
    if (entry.status !== "A" || !INHERITED_V01_SOURCE_PATHS.includes(entry.path)) fail(`invalid inherited v0.1 source delta: ${entry.status} ${entry.path}`);
  }
  return normalized;
}

export function validateInheritedV01Identity(value) {
  const inherited = asRecord(value, "inherited A6 v0.1 segment");
  if (inherited.packageRef !== INHERITED_V01_PACKAGE_REF || inherited.originalAnchor !== ORIGINAL_A6_ANCHOR || inherited.sourceRef !== INHERITED_V01_SOURCE_REF || inherited.status !== "INHERITED_VALID") fail("inherited A6 v0.1 segment identity is invalid");
  if (JSON.stringify(inherited.sourceDelta) !== JSON.stringify(INHERITED_V01_SOURCE_PATHS)) fail("inherited A6 v0.1 source delta is invalid");
  return inherited;
}

export function validateActualStartingRevision(value) {
  if (value !== TASK_ANCHOR) fail("actual starting revision is not the accepted retry anchor");
  return value;
}

export function validateProductionRepairPatch(patch) {
  if (typeof patch !== "string" || patch.length === 0) fail("production repair patch is missing");
  const changedLines = patch.split(/\r?\n/).filter((line) => (line.startsWith("+") && !line.startsWith("+++")) || (line.startsWith("-") && !line.startsWith("---")));
  if (changedLines.length !== 5 || changedLines.filter((line) => line.startsWith("-")).length !== 1 || changedLines.filter((line) => line.startsWith("+")).length !== 4) fail("production repair patch has unexpected changed statements");
  const normalized = patch.replace(/\s+/g, " ").trim();
  const removed = "- if(source.placement.parent_id)setId(*source.placement.parent_id,destination.mutable_placement()->mutable_parent_id());destination.mutable_placement()->mutable_order_key()->set_value(std::string(reinterpret_cast<const char*>(source.placement.order_key.bytes().data()),source.placement.order_key.bytes().size()));";
  const added = "+ if(source.placement.parent_id) { + setId(*source.placement.parent_id,destination.mutable_placement()->mutable_parent_id()); + } + destination.mutable_placement()->mutable_order_key()->set_value(std::string(reinterpret_cast<const char*>(source.placement.order_key.bytes().data()),source.placement.order_key.bytes().size()));";
  if (!normalized.includes(removed) || !normalized.includes(added)) fail("production repair changes more than the authorized conditional-bracing statement");
  if (normalized.includes("if(source.placement.parent_id) { destination.mutable_placement()->mutable_order_key")) fail("order key write became conditional");
  if (!normalized.includes("setId(*source.placement.parent_id,destination.mutable_placement()->mutable_parent_id())")) fail("parent identifier mapping changed");
  if (!normalized.includes("destination.mutable_placement()->mutable_order_key()->set_value")) fail("order key field mapping changed");
  return true;
}

export function validateRepositoryIdentity(remoteUrl) {
  const normalized = asString(remoteUrl, "repository remote").replace(/\.git$/, "");
  if (normalized !== "https://github.com/Mostorm-Labs/axiom" && normalized !== "git@github.com:Mostorm-Labs/axiom") {
    fail(`repository identity is not ${REPOSITORY}: ${remoteUrl}`);
  }
  return REPOSITORY;
}

export function validateCiIdentity(ci, sourceRef) {
  const value = asRecord(ci, "CI identity");
  validateSourceRef(sourceRef);
  if (value.event !== "push") fail("CI event must be push");
  if (value.ref !== BRANCH_REF) fail("CI ref is not the authoritative execution branch");
  if (value.headSha !== sourceRef || value.checkoutSha !== sourceRef) fail("CI head, checkout, and source ref must match exactly");
  if (!isSha(value.headSha) || !isSha(value.checkoutSha)) fail("CI head or checkout SHA is invalid");
  if (!/^[1-9][0-9]*$/.test(asString(value.runId, "CI run id"))) fail("CI run id is invalid");
  if (!/^[1-9][0-9]*$/.test(asString(value.runAttempt, "CI run attempt"))) fail("CI run attempt is invalid");
  if (!/^https:\/\/github\.com\/Mostorm-Labs\/axiom\/actions\/runs\/[1-9][0-9]*$/.test(asString(value.hostedRunUrl, "CI hosted run URL"))) fail("CI hosted run URL is not reviewer-accessible");
  if (value.workflowName !== WORKFLOW_NAME || value.workflowPath !== WORKFLOW_PATH) fail("CI workflow identity is invalid");
  if (value.workflowRef !== `${BRANCH_REF}/${WORKFLOW_PATH}@${sourceRef}`) fail("CI workflow ref is not source-bound");
  if (value.artifactName !== `gt-g1-06-a6-${sourceRef}`) fail("CI artifact name is not source-bound");
  return value;
}

function validateCounts(suite, label, { protobufOff = false } = {}) {
  const value = asRecord(suite, label);
  for (const key of ["total", "executed", "passed", "skipped", "failed"]) {
    if (!Number.isInteger(value[key]) || value[key] < 0) fail(`${label}.${key} is invalid`);
  }
  if (value.total !== value.executed + value.skipped) fail(`${label} total does not equal executed plus skipped`);
  if (value.executed !== value.passed + value.failed) fail(`${label} executed does not equal passed plus failed`);
  if (value.failed !== 0) fail(`${label} has unexpected failures`);
  if (!Array.isArray(value.skipIds) || value.skipIds.some((id) => !Number.isInteger(id))) fail(`${label}.skipIds is invalid`);
  const skipIds = [...value.skipIds].sort((a, b) => a - b);
  if (JSON.stringify(skipIds) !== JSON.stringify(value.skipIds)) fail(`${label}.skipIds must be sorted`);
  if (protobufOff) {
    if (JSON.stringify(skipIds) !== JSON.stringify(EXPECTED_PROTOBUF_OFF_SKIP_IDS)) fail(`${label} skip accounting drifted from the accepted protobuf-off set`);
  } else if (skipIds.length !== 0 || value.skipped !== 0) {
    fail(`${label} unexpectedly skipped tests`);
  }
  return value;
}

export function validateSemanticAccounting(facts) {
  const value = asRecord(facts, "semantic verification facts");
  const protobufOn = validateCounts(value.fullSemantic, "full semantic CTest", { protobufOff: false });
  const protobufOff = validateCounts(value.fullSemanticOff, "protobuf-off full semantic CTest", { protobufOff: true });
  return { protobufOn, protobufOff };
}

export function validateFocusedResults(value) {
  const focused = asRecord(value, "focused semantic results");
  const required = ["integration", "operationConformance", "g1_06", "g1_05"];
  for (const configuration of ["protobufOn", "protobufOff"]) {
    const configured = asRecord(focused[configuration], `focused.${configuration}`);
    for (const key of required) {
      const result = asRecord(configured[key], `focused.${configuration}.${key}`);
      if (result.status !== "PASS" || result.failed !== 0) fail(`focused ${configuration}.${key} result is not PASS`);
      if (!Number.isInteger(result.total) || !Number.isInteger(result.passed) || !Number.isInteger(result.skipped)) fail(`focused ${configuration}.${key} counts are invalid`);
      if (result.total !== result.passed + result.skipped) fail(`focused ${configuration}.${key} counts do not reconcile`);
    }
  }
  return focused;
}

export function validateEvidenceInventory(files) {
  if (!Array.isArray(files) || JSON.stringify(files) !== JSON.stringify(REQUIRED_EVIDENCE_FILES)) fail("required evidence inventory is incomplete or reordered");
  return files;
}

export function validateA5Provenance(value) {
  const predecessor = asRecord(value, "A5 accepted predecessor provenance");
  if (predecessor.sourceRef !== A5_SOURCE_REF || predecessor.materializedRef !== A5_MATERIALIZED_REF || predecessor.packageRef !== A5_PACKAGE_REF) fail("A5 provenance substitution is unsupported");
  if (predecessor.status !== "ACCEPTED_FOR_DOWNSTREAM") fail("A5 predecessor is not accepted for downstream");
  validateEvidenceInventory(predecessor.evidenceFiles);
  return predecessor;
}

export function validateOracleContract(value) {
  const oracle = asRecord(value, "correctness contract");
  if (oracle.primary !== "canonical_semantic_projection") fail("canonical projection is not the primary oracle");
  if (oracle.digestRole !== "secondary_diagnostic_only") fail("digest role is not secondary-only");
  if (oracle.digestOnlyPassAssertions !== 0) fail("digest-only correctness claim is forbidden");
  return oracle;
}

export function assertSafeOutputDirectory(outputDir) {
  const output = resolve(outputDir);
  if (output === EVIDENCE_ROOT || output.startsWith(`${EVIDENCE_ROOT}/`)) fail("generator cannot write directly into verification/evidence/gates");
  if (output === REPOSITORY_ROOT || output.startsWith(`${REPOSITORY_ROOT}/verification/evidence/gates/`)) fail("generator output is not an external temporary directory");
  return output;
}

function loadAcceptedA5() {
  const root = `verification/evidence/gates/G1/${A5_SOURCE_REF}/GT-G1-06`;
  const files = ["G1-06-PLAN.json", "G1-06-SNAPSHOT.json", "G1-06-REPLAY.json", "G1-06-PROJECTION.json", "G1-06-DIGEST.json", "G1-06-CTEST.txt", "G1-06-DIFF.json"];
  const plan = gitJson(PACKAGE_MATERIALIZATION_REF, `${root}/G1-06-PLAN.json`);
  const diff = gitJson(PACKAGE_MATERIALIZATION_REF, `${root}/G1-06-DIFF.json`);
  if (plan.package_ref !== A5_PACKAGE_REF || plan.source_ref !== A5_SOURCE_REF) fail("accepted A5 plan provenance is invalid");
  if (diff.package_ref !== A5_PACKAGE_REF || diff.source_ref !== A5_SOURCE_REF) fail("accepted A5 diff provenance is invalid");
  try {
    git(["grep", "-n", "-F", "notion://3d24c57a-590c-8116-8d39-fae074ec4ca3/GT-G1-06-A5-P31-v0.3", PACKAGE_MATERIALIZATION_REF, "--", root]);
    fail("stale A5 package ref is present in accepted predecessor evidence");
  } catch (error) {
    if (error.status !== 1) throw error;
  }
  return {
    status: "ACCEPTED_FOR_DOWNSTREAM",
    packageRef: A5_PACKAGE_REF,
    sourceRef: A5_SOURCE_REF,
    materializedRef: A5_MATERIALIZED_REF,
    evidenceRoot: root,
    evidenceFiles: files,
  };
}

function gitSourceDelta(sourceRef) {
  const raw = git(["diff", "--name-status", TASK_ANCHOR, sourceRef]);
  return raw ? raw.split("\n").filter(Boolean).map((line) => line.split("\t")) : [];
}

function gitDiff(from, to, path) {
  return git(["diff", "--unified=0", from, to, "--", path]);
}

function validateInheritedV01Segment() {
  requireCommit(ORIGINAL_A6_ANCHOR, "original A6 anchor");
  requireCommit(INHERITED_V01_SOURCE_REF, "inherited v0.1 source ref");
  requireAncestor(ORIGINAL_A6_ANCHOR, INHERITED_V01_SOURCE_REF, "original A6 anchor");
  validateInheritedV01SourceDelta(git(["diff", "--name-status", ORIGINAL_A6_ANCHOR, INHERITED_V01_SOURCE_REF])
    .split("\n").filter(Boolean).map((line) => line.split("\t")));
  return {
    packageRef: INHERITED_V01_PACKAGE_REF,
    originalAnchor: ORIGINAL_A6_ANCHOR,
    sourceRef: INHERITED_V01_SOURCE_REF,
    sourceDelta: INHERITED_V01_SOURCE_PATHS.slice(),
    status: "INHERITED_VALID",
  };
}

function assertSourceLineage(sourceRef, actualStartingRevision) {
  const parent = git(["rev-parse", `${sourceRef}^`]);
  validateAncestry({ packageMaterializationRef: PACKAGE_MATERIALIZATION_REF, taskAnchor: TASK_ANCHOR, sourceRef, sourceCommitParent: parent });
  validateActualStartingRevision(actualStartingRevision);
  const inherited = validateInheritedV01Segment();
  validateSourceDelta(gitSourceDelta(sourceRef));
  validateProductionRepairPatch(gitDiff(actualStartingRevision, sourceRef, PRODUCTION_REPAIR_PATH));
  validateRepositoryIdentity(git(["remote", "get-url", "origin"]));
  return { parent, inherited };
}

function readFacts(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    fail(`unable to read facts file: ${error.message}`);
  }
}

function ciIdentityEqual(left, right) {
  return ["runId", "runAttempt", "event", "ref", "headSha", "checkoutSha", "workflowName", "workflowPath", "workflowRef", "hostedRunUrl", "artifactName"]
    .every((key) => left[key] === right[key]);
}

export function validateFacts(facts, expected) {
  const value = asRecord(facts, "A6 facts");
  if (value.format !== "axiom-gt-g1-06-a6-facts-v1" || value.taskId !== TASK_ID) fail("facts envelope is invalid");
  if (value.packageRef !== expected.packageRef || value.taskAnchor !== expected.taskAnchor || value.sourceRef !== expected.sourceRef) fail("facts immutable identity is foreign");
  if (value.actualStartingRevision !== expected.actualStartingRevision) fail("facts actual starting revision is foreign");
  if (value.repository !== REPOSITORY || value.sourceCommitParent !== expected.sourceCommitParent) fail("facts repository or source parent is invalid");
  validateInheritedV01Identity(value.inheritedV01);
  if (value.productionRepair !== "SEMANTIC_NO_OP_WARNING_REPAIR") fail("production repair classification is invalid");
  validateCiIdentity(value.ci, expected.sourceRef);
  validateA5Provenance(value.a5AcceptedPredecessor);
  validateFocusedResults(value.focused);
  const semantic = validateSemanticAccounting(value);
  validateOracleContract(value.correctnessContract);
  validateEvidenceInventory(value.requiredEvidenceFiles);
  if (value.evidenceContractTest !== "PASS") fail("A6 evidence-contract test did not pass");
  if (value.gitDiffCheck !== "PASS") fail("git diff --check did not pass");
  if (value.cleanCheckoutReproduction !== "PASS") fail("clean-checkout reproduction did not pass");
  return { ...value, semantic };
}

function commonEnvelope({ facts, sourceRef, sourceCommitParent, inherited, a5 }) {
  return {
    taskId: TASK_ID,
    stage: "P32",
    packageRef: PACKAGE_REF,
    packagePage: PACKAGE_PAGE,
    packageMaterializationRef: PACKAGE_MATERIALIZATION_REF,
    taskAnchor: { revision: TASK_ANCHOR, relation: "ancestor" },
    originalA6Anchor: ORIGINAL_A6_ANCHOR,
    inheritedV01: inherited,
    actualStartingRevision: facts.actualStartingRevision,
    sourceRef,
    sourceCommitParent,
    executionRef: EXECUTION_REF,
    repository: REPOSITORY,
    sourceDelta: AUTHORIZED_SOURCE_PATHS.slice(),
    a5AcceptedPredecessor: a5,
    ci: facts.ci,
  };
}

function evidenceDocument(name, common, facts, a5, inherited) {
  const { sourceRef } = common;
  if (name === "G1-06-PLAN.json") {
    return {
      format: "axiom-gt-g1-06-a6-plan-v1",
      ...common,
      authorizedSourceScope: AUTHORIZED_SOURCE_PATHS,
      semanticRuntimeDelta: "NONE",
      exactSourceWorkflow: { path: WORKFLOW_PATH, sourceBound: true, outputOutsideAcceptedEvidenceRoot: true, workflowArtifactOnly: false },
      correctnessContract: facts.correctnessContract,
      verification: { focused: facts.focused, protobufOn: facts.fullSemantic, protobufOff: facts.fullSemanticOff, evidenceContractTest: facts.evidenceContractTest, gitDiffCheck: facts.gitDiffCheck, cleanCheckoutReproduction: facts.cleanCheckoutReproduction },
      evidenceInventory: REQUIRED_EVIDENCE_FILES,
      status: "PASS",
    };
  }
  if (name === "G1-06-SNAPSHOT.json") {
    return { format: "axiom-gt-g1-06-a6-snapshot-v1", ...common, inheritedA5SnapshotEvidence: `${a5.materializedRef}:${a5.evidenceRoot}/G1-06-SNAPSHOT.json`, exactSourceVerification: facts.focused, status: "PASS" };
  }
  if (name === "G1-06-REPLAY.json") {
    return { format: "axiom-gt-g1-06-a6-replay-v1", ...common, inheritedA5ReplayEvidence: `${a5.materializedRef}:${a5.evidenceRoot}/G1-06-REPLAY.json`, authoritativeContinuation: inherited.replay?.authoritative_continuation ?? null, exactSourceVerification: facts.focused, status: "PASS" };
  }
  if (name === "G1-06-PROJECTION.json") {
    return { format: "axiom-gt-g1-06-a6-projection-v1", ...common, primaryOracle: "canonical_semantic_projection", inheritedA5ProjectionEvidence: `${a5.materializedRef}:${a5.evidenceRoot}/G1-06-PROJECTION.json`, projectionBackstop: "PASS", exactSourceVerification: facts.focused, status: "PASS" };
  }
  if (name === "G1-06-DIGEST.json") {
    return { format: "axiom-gt-g1-06-a6-digest-v1", ...common, role: "secondary verification-only diagnostic", primaryOracle: "canonical_semantic_projection", digestOnlyPassAssertions: 0, inheritedA5DigestEvidence: `${a5.materializedRef}:${a5.evidenceRoot}/G1-06-DIGEST.json`, exactSourceVerification: facts.focused, status: "PASS" };
  }
  if (name === "G1-06-DIFF.json") {
    return { format: "axiom-gt-g1-06-a6-diff-v1", ...common, sourceToMaterializedRelation: "EVIDENCE_ONLY_DESCENDANT", materialization: { requiredFiles: REQUIRED_EVIDENCE_FILES, directCiWriteToGateRoot: false, reviewerAccessible: true }, protectedRootsUnchanged: true, status: "PASS" };
  }
  return null;
}

function renderCtest(common, facts, a5) {
  const focusedLine = (configuration, key) => {
    const result = facts.focused[configuration][key];
    return `${configuration}_${key}=${result.total} total, ${result.passed} PASS, ${result.skipped} SKIP, ${result.failed} FAIL`;
  };
  const lines = [
    "GT-G1-06-A6 P32 exact-source CI verification transcript",
    `package_ref=${PACKAGE_REF}`,
    `task_anchor=${TASK_ANCHOR}`,
    `actual_starting_revision=${facts.actualStartingRevision}`,
    `source_ref=${common.sourceRef}`,
    `source_commit_parent=${common.sourceCommitParent}`,
    `ci_event=${facts.ci.event}`,
    `ci_ref=${facts.ci.ref}`,
    `ci_head_sha=${facts.ci.headSha}`,
    `checkout_sha=${facts.ci.checkoutSha}`,
    `hosted_run=${facts.ci.hostedRunUrl}`,
    `artifact=${facts.ci.artifactName}`,
    "",
    `accepted_A5_source_ref=${a5.sourceRef}`,
    `accepted_A5_materialized_ref=${a5.materializedRef}`,
    `accepted_A5_package_ref=${a5.packageRef}`,
    "",
    focusedLine("protobufOn", "integration"),
    focusedLine("protobufOn", "operationConformance"),
    focusedLine("protobufOn", "g1_06"),
    focusedLine("protobufOn", "g1_05"),
    focusedLine("protobufOff", "integration"),
    focusedLine("protobufOff", "operationConformance"),
    focusedLine("protobufOff", "g1_06"),
    focusedLine("protobufOff", "g1_05"),
    `protobuf_on_full_semantic=${facts.fullSemantic.total} total, ${facts.fullSemantic.passed} PASS, ${facts.fullSemantic.skipped} expected SKIP, ${facts.fullSemantic.failed} FAIL`,
    `protobuf_off_full_semantic=${facts.fullSemanticOff.total} total, ${facts.fullSemanticOff.passed} PASS, ${facts.fullSemanticOff.skipped} expected SKIP, ${facts.fullSemanticOff.failed} FAIL`,
    `protobuf_off_exact_skip_ids=${facts.fullSemanticOff.skipIds.join(",")}`,
    `evidence_contract_test=${facts.evidenceContractTest}`,
    `git_diff_check=${facts.gitDiffCheck}`,
    `clean_checkout_reproduction=${facts.cleanCheckoutReproduction}`,
    "status=PASS",
  ];
  return `${lines.join("\n")}\n`;
}

export function generateEvidence({ packageRef, taskAnchor, actualStartingRevision, sourceRef, factsPath, outputDir, ci }) {
  if (packageRef !== PACKAGE_REF || taskAnchor !== TASK_ANCHOR) fail("A6 package or task anchor is foreign");
  validateSourceRef(sourceRef);
  const output = assertSafeOutputDirectory(outputDir);
  if (existsSync(output) && readdirSync(output).length !== 0) fail("evidence output directory must be empty");
  mkdirSync(output, { recursive: true });
  const { parent: sourceCommitParent, inherited: inheritedSegment } = assertSourceLineage(sourceRef, actualStartingRevision);
  const a5 = loadAcceptedA5();
  const rawFacts = readFacts(factsPath);
  const facts = validateFacts(rawFacts, { packageRef, taskAnchor, actualStartingRevision, sourceRef, sourceCommitParent });
  const ciRecord = validateCiIdentity(ci, sourceRef);
  if (!ciIdentityEqual(ciRecord, facts.ci)) fail("CLI CI identity differs from facts CI identity");
  const inheritedEvidence = { replay: gitJson(PACKAGE_MATERIALIZATION_REF, `${a5.evidenceRoot}/G1-06-REPLAY.json`) };
  const common = commonEnvelope({ facts, sourceRef, sourceCommitParent, inherited: inheritedSegment, a5 });
  const documents = {};
  for (const name of REQUIRED_EVIDENCE_FILES) {
    if (name === "G1-06-CTEST.txt") documents[name] = renderCtest(common, facts, a5);
    else documents[name] = evidenceDocument(name, common, facts, a5, inheritedEvidence);
  }
  for (const [name, value] of Object.entries(documents)) {
    const destination = resolve(output, name);
    if (!destination.startsWith(`${output}/`)) fail("evidence destination escaped output directory");
    writeFileSync(destination, typeof value === "string" ? value : `${JSON.stringify(value, null, 2)}\n`, "utf8");
  }
  return { outputDir: output, files: REQUIRED_EVIDENCE_FILES.slice(), sourceRef, sourceCommitParent, packageRef, taskAnchor, status: "PASS" };
}

function parseArgs(argv) {
  const flags = ["package-ref", "task-anchor", "actual-starting-revision", "source-ref", "facts", "output-dir", "ci-run-id", "ci-run-attempt", "ci-event", "ci-ref", "ci-head-sha", "checkout-sha", "workflow-ref", "hosted-run-url", "artifact-name"];
  if (argv.length !== flags.length * 2) fail("A6 evidence generator requires the complete immutable identity surface");
  const result = {};
  for (let index = 0; index < flags.length; index += 1) {
    const flag = `--${flags[index]}`;
    if (argv[index * 2] !== flag || !argv[index * 2 + 1]) fail(`missing required A6 evidence argument: ${flag}`);
    result[flags[index].replace(/-([a-z])/g, (_, letter) => letter.toUpperCase())] = argv[index * 2 + 1];
  }
  return result;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const args = parseArgs(process.argv.slice(2));
    const ci = {
      runId: args.ciRunId,
      runAttempt: args.ciRunAttempt,
      event: args.ciEvent,
      ref: args.ciRef,
      headSha: args.ciHeadSha,
      checkoutSha: args.checkoutSha,
      workflowRef: args.workflowRef,
      workflowName: WORKFLOW_NAME,
      workflowPath: WORKFLOW_PATH,
      hostedRunUrl: args.hostedRunUrl,
      artifactName: args.artifactName,
    };
    const result = generateEvidence({ ...args, factsPath: args.facts, ci });
    process.stdout.write(`${JSON.stringify(result)}\n`);
  } catch (error) {
    process.stderr.write(`A6 evidence generation failed closed: ${error.message}\n`);
    process.exitCode = 1;
  }
}
