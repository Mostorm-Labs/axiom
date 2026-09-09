import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { isAbsolute, resolve } from "node:path";

export const TASK_ID = "GT-G1-07";
export const PACKAGE_REF = "notion://3d64c57a-590c-810e-93d9-ff721caceb8a/GT-G1-07-P31-v0.2";
export const TASK_ANCHOR = "8d3570c7c5df6f718b444b102b8b01042c2af3dd";
export const REPAIR_BASE = "5da008c252162f9fe9677a1841bd9b63bbc20389";
export const PACKAGE_MATERIALIZATION_REF = TASK_ANCHOR;
export const EXECUTION_REF = "codex/gt-g1-07-p36-wire-trace-repair";
export const REPOSITORY = "Mostorm-Labs/axiom";
export const WORKFLOW_PATH = ".github/workflows/g1-07-exact-source.yml";
export const WORKFLOW_NAME = "GT-G1-07 Exact Source";
export const REQUIRED_EVIDENCE_FILES = [
  "G1-07-PLAN.json", "G1-07-TRACE.json", "G1-07-STEP.json", "G1-07-SEEK.json",
  "G1-07-OBSERVATION.json", "G1-07-PROJECTION.json", "G1-07-DETERMINISM.json",
  "G1-07-NEGATIVE.json", "G1-07-CLI.txt", "G1-07-CTEST.txt", "G1-07-DIFF.json",
  "G1-07-GATE-MANIFEST.json",
];

function fail(message) { throw new Error(message); }
function asRecord(value, label) {
  if (typeof value !== "object" || value === null || Array.isArray(value)) fail(`${label} must be an object`);
  return value;
}
function asString(value, label) {
  if (typeof value !== "string" || value.length === 0) fail(`${label} must be a non-empty string`);
  return value;
}
export function isSha(value) { return typeof value === "string" && /^[0-9a-f]{40}$/.test(value); }
export function validateSourceRef(value) { if (!isSha(value)) fail("source_ref must be a full immutable SHA"); return value; }
export function validateRepositoryIdentity(value) {
  const normalized = asString(value, "repository").replace(/\.git$/, "");
  if (normalized !== REPOSITORY && normalized !== "https://github.com/Mostorm-Labs/axiom" && normalized !== "git@github.com:Mostorm-Labs/axiom") fail(`repository identity must be ${REPOSITORY}`);
  return REPOSITORY;
}
export function validateAncestry({ taskAnchor, sourceRef, sourceCommitParent }) {
  validateSourceRef(taskAnchor); validateSourceRef(sourceRef); validateSourceRef(sourceCommitParent);
  if (taskAnchor !== TASK_ANCHOR) fail("task anchor does not bind the authorized package");
  return true;
}
export function validateSourceDelta(entries) {
  const value = asRecord(entries, "source delta");
  if (!Array.isArray(value.paths) || value.paths.length !== 13) fail("source delta must contain exactly thirteen authorized paths");
  const allowed = new Set([
    "runtime/semantic/tools/g1_07_replay_inspector.hpp",
    "runtime/semantic/tools/g1_07_replay_inspector.cpp",
    "runtime/semantic/tools/g1_07_replay_corpus.hpp",
    "runtime/semantic/tools/g1_07_replay_corpus.cpp",
    "runtime/semantic/tools/g1_07_replay_inspector_main.cpp",
    "runtime/semantic/tests/g1_07_replay_inspector_test.cpp",
    "runtime/semantic/tests/g1_07_replay_inspector_cli_test.cpp",
    "runtime/semantic/tests/CMakeLists.txt",
    "runtime/semantic/include/canvas/semantic/codec.hpp",
    WORKFLOW_PATH,
    "runtime/semantic/tools/CMakeLists.txt",
    "verification/tools/generate_g1_07_evidence.mjs",
    "verification/packages/semantic-conformance-cli/test/g1-07-exact-source-evidence.test.mjs",
  ]);
  if (new Set(value.paths).size !== value.paths.length || value.paths.some((path) => !allowed.has(path))) fail("source delta contains unauthorized or duplicate paths");
  return value.paths;
}
export function validateFacts(facts, expected = {}) {
  const value = asRecord(facts, "facts");
  if (value.format !== "axiom-gt-g1-07-facts-v1") fail("facts format is invalid");
  if (value.taskId !== TASK_ID || value.packageRef !== PACKAGE_REF) fail("task/package identity is invalid");
  validateRepositoryIdentity(value.repository);
  validateSourceRef(value.sourceRef); validateSourceRef(value.actualStartingRevision); validateSourceRef(value.sourceCommitParent);
  if (value.taskAnchor !== TASK_ANCHOR || value.actualStartingRevision !== REPAIR_BASE) fail("anchor/starting revision is invalid");
  if (expected.sourceRef && value.sourceRef !== expected.sourceRef) fail("source_ref does not match expected source");
  validateAncestry(value);
  if (!value.implementationSteps || value.implementationSteps.completed !== 16 || value.implementationSteps.total !== 16) fail("implementation steps are incomplete");
  if (value.acceptanceStatus !== "PASS" || value.cleanCheckout !== "PASS") fail("acceptance/clean-checkout status is not PASS");
  validateSourceDelta(value.sourceDelta);
  if (!value.semantic || value.semantic.projectionPrimary !== true || value.semantic.digestSecondaryOnly !== true || value.semantic.digestOnlyPassAssertions !== 0) fail("semantic oracle contract is invalid");
  if (!value.ci || value.ci.workflowName !== WORKFLOW_NAME || value.ci.workflowPath !== WORKFLOW_PATH || value.ci.sourceRef !== value.sourceRef || value.ci.checkoutSha !== value.sourceRef || value.ci.event !== "push" || value.ci.ref !== `refs/heads/${EXECUTION_REF}` || !/^[1-9][0-9]*$/.test(value.ci.runId ?? "") || !/^[1-9][0-9]*$/.test(value.ci.runAttempt ?? "") || value.ci.hostedRunUrl !== `https://github.com/${REPOSITORY}/actions/runs/${value.ci.runId}` || value.ci.artifactName !== `gt-g1-07-${value.sourceRef}`) fail("CI provenance is invalid");
  const cliProcess = asRecord(value.cliProcess, "CLI process facts");
  if (cliProcess.processLevel !== true || cliProcess.allCommandsCovered !== true ||
      cliProcess.exit2Covered !== true || cliProcess.exit3Covered !== true ||
      !Array.isArray(cliProcess.commands) || cliProcess.commands.length < 7) {
    fail("CLI process facts are incomplete");
  }
  const commandNames = new Set(["run", "step", "seek", "object", "projection"]);
  const scenarios = new Set(["valid", "argument-error", "boundary-error", "semantic-error"]);
  const seenValidCommands = new Set();
  for (const item of cliProcess.commands) {
    const command = asRecord(item, "CLI process command");
    if (typeof command.command !== "string" || !commandNames.has(command.command) ||
        typeof command.scenario !== "string" || !scenarios.has(command.scenario) ||
        typeof command.provider !== "string" || !["reference", "indexed"].includes(command.provider) ||
        typeof command.configuration !== "string" || !["protobuf-on", "protobuf-off"].includes(command.configuration) ||
        !Number.isInteger(command.exitCode) || ![0, 2, 3].includes(command.exitCode) ||
        !/^[0-9a-f]{64}$/.test(command.stdoutSha256 ?? "") ||
        !/^[0-9a-f]{64}$/.test(command.stderrSha256 ?? "") ||
        command.stdoutDocumentCount !== 1 || command.stdoutEndsWithNewline !== true ||
        command.stdoutSchemaValid !== true || command.deterministicRepeat !== true ||
        command.independentOracle !== true || command.status !== "PASS") {
      fail("CLI process command facts are invalid");
    }
    if (command.scenario === "valid" || command.scenario === "boundary-error") {
      seenValidCommands.add(command.command);
      if (command.scenario === "valid" && command.exitCode !== 0) fail("CLI command success accounting is invalid");
    }
    if (command.scenario === "argument-error" && command.exitCode !== 2) fail("CLI argument error accounting is invalid");
    if (command.scenario === "boundary-error" && command.exitCode !== 2) fail("CLI boundary error accounting is invalid");
    if (command.scenario === "semantic-error" && command.exitCode !== 3) fail("CLI semantic error accounting is invalid");
  }
  if (seenValidCommands.size !== commandNames.size) fail("CLI process command coverage is incomplete");
  const replay = asRecord(value.replay, "replay facts");
  const expectedFamilies = ["InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms", "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent"];
  if (replay.operationCount !== 15 || JSON.stringify(replay.orderedOperationFamilies) !== JSON.stringify(expectedFamilies) || replay.singleReplayCoordinatorInvocation !== true || replay.omittedValidExternalSuffixClaimed !== false || !Array.isArray(replay.checkpoints) || JSON.stringify(replay.checkpoints) !== JSON.stringify([0, 5, 10, 15])) fail("replay facts are incomplete or reordered");
  const step = asRecord(value.stepMatrix, "step matrix");
  if (step.applied !== 15 || step.alreadyApplied !== 0 || step.rejected !== 0 || step.commitBlocked !== 0 || step.firstFailureIndex !== null || step.firstFailureOperationId !== null) fail("step matrix is incomplete");
  const seek = asRecord(value.seek, "seek matrix");
  if (JSON.stringify(seek.positions) !== JSON.stringify([0, 1, 5, 10, 15]) || seek.projectionParity !== "5/5" || seek.digestParity !== "5/5") fail("seek matrix is incomplete");
  const observations = asRecord(value.observationsDetail, "observation facts");
  if (observations.objectFound !== true || observations.objectMissing !== true || observations.readOnly !== true || observations.lifecycle !== "READY" || observations.generation !== 15 || observations.commitOrdinal !== 15) fail("observation facts are incomplete");
  const negative = asRecord(value.negativeMatrix, "negative matrix");
  for (const key of ["malformed", "invalidPosition", "rejectedSuffix", "commitBlocked", "snapshotBootstrapFailure"]) if (negative[key] !== "PASS") fail("negative matrix is incomplete");
  const regression = asRecord(value.regressionTests, "regression tests");
  const verifySuite = (suite, label, expectedSkips) => {
    const item = asRecord(suite, label);
    for (const key of ["total", "passed", "skipped", "failed"]) if (!Number.isInteger(item[key]) || item[key] < 0) fail(`${label} accounting is invalid`);
    if (item.failed !== 0 || item.total !== item.passed + item.skipped || JSON.stringify(item.skipIds ?? []) !== JSON.stringify(expectedSkips)) fail(`${label} accounting is invalid`);
    return item;
  };
  verifySuite(regression.protobufOn, "protobuf-on regression", []);
  verifySuite(regression.protobufOff, "protobuf-off regression", [...Array.from({ length: 12 }, (_, i) => i + 25), ...Array.from({ length: 11 }, (_, i) => i + 54)]);
  const focused = asRecord(regression.focused, "focused regression");
  for (const config of ["protobufOn", "protobufOff"]) {
    const item = asRecord(focused[config], `focused ${config}`);
    if (item.failed !== 0 || item.total !== item.passed + item.skipped || item.total < 16) fail(`focused ${config} accounting is invalid`);
  }
  const components = asRecord(regression.components, "component regressions");
  for (const component of ["g1_06", "g1_05"]) {
    const componentFacts = asRecord(components[component], `${component} regression`);
    for (const config of ["protobufOn", "protobufOff"]) {
      const item = asRecord(componentFacts[config], `${component} ${config} regression`);
      if (item.failed !== 0 || item.total <= 0 || item.total !== item.passed + item.skipped) fail(`${component} ${config} accounting is invalid`);
    }
  }
  if (typeof regression.fullSemantic !== "string" || regression.fullSemantic.length === 0) fail("full semantic regression record is missing");
  if (!Array.isArray(value.acceptanceMatrix) || value.acceptanceMatrix.length < 7 || value.acceptanceMatrix.some((item) => !item || typeof item.id !== "string" || typeof item.test !== "string" || typeof item.evidence !== "string")) fail("acceptance traceability is incomplete");
  return value;
}
export function assertSafeOutputDirectory(out, repositoryRoot = process.cwd()) {
  if (typeof out !== "string" || out.length === 0 || isAbsolute(out)) fail("evidence output must be a relative path");
  if (!out.replaceAll("\\", "/").startsWith("out/")) fail("evidence output must stay under the disposable out/ staging tree");
  const root = resolve(repositoryRoot);
  const target = resolve(root, out);
  if (target !== root && !target.startsWith(`${root}/`)) fail("evidence output escapes repository root");
  return target;
}
export function validateEvidenceInventory(directory) {
  const names = readdirSync(directory).sort();
  if (JSON.stringify(names) !== JSON.stringify([...REQUIRED_EVIDENCE_FILES].sort())) fail("evidence inventory does not contain exactly twelve files");
  return names;
}
function git(args, cwd) { return execFileSync("git", args, { cwd, encoding: "utf8" }).trim(); }
function json(value) { return `${JSON.stringify(value, null, 2)}\n`; }

export function generateEvidence({ sourceRef, facts, out, repositoryRoot = process.cwd() }) {
  const value = validateFacts(facts, { sourceRef });
  validateSourceRef(sourceRef);
  const target = assertSafeOutputDirectory(out, repositoryRoot);
  mkdirSync(target, { recursive: true });
  const sourceCommitParent = git(["rev-parse", `${sourceRef}^`], repositoryRoot);
  if (sourceCommitParent !== value.sourceCommitParent) fail("source commit parent does not match facts");
  const root = { taskId: TASK_ID, packageRef: PACKAGE_REF, repository: REPOSITORY, taskAnchor: TASK_ANCHOR, sourceRef, actualStartingRevision: value.actualStartingRevision, executionRef: EXECUTION_REF, ci: value.ci };
  const records = new Map([
    ["G1-07-PLAN.json", { ...root, implementationSteps: value.implementationSteps, acceptanceStatus: value.acceptanceStatus, cleanCheckout: value.cleanCheckout, scope: "authorized P31 package; P34 not claimed", acceptanceMatrix: value.acceptanceMatrix, regressionAccounting: value.regressionTests }],
    ["G1-07-TRACE.json", { ...root, traceFormat: "axiom-semantic-replay-trace-v1", exactOperationOrder: true, operationCount: value.replay.operationCount, orderedOperationFamilies: value.replay.orderedOperationFamilies, singleReplayCoordinatorInvocation: value.replay.singleReplayCoordinatorInvocation, omittedValidExternalSuffixClaimed: value.replay.omittedValidExternalSuffixClaimed }],
    ["G1-07-STEP.json", { ...root, canonicalApplySource: "kRestoreReplay", stepMatrix: value.stepMatrix, commitIdentityFields: ["CanonicalCommitRecord", "ChangeSet", "generation", "runtimeEpoch", "commitOrdinal"] }],
    ["G1-07-SEEK.json", { ...root, strategy: "deterministic_baseline_plus_exact_prefix_reconstruction", checkpoints: value.replay.checkpoints, seekMatrix: value.seek }],
    ["G1-07-OBSERVATION.json", { ...root, observations: value.observationsDetail, readOnlyObjectInspection: true }],
    ["G1-07-PROJECTION.json", { ...root, projectionPrimary: value.semantic.projectionPrimary, digestSecondaryOnly: value.semantic.digestSecondaryOnly, digestOnlyPassAssertions: value.semantic.digestOnlyPassAssertions, projectionBackstop: "required for every same-state and mutation comparison" }],
    ["G1-07-DETERMINISM.json", { ...root, byteDeterministic: true, deterministicCommands: ["run", "step", "seek", "object", "projection"], seekPositions: value.seek.positions, parity: { projection: value.seek.projectionParity, digest: value.seek.digestParity } }],
    ["G1-07-NEGATIVE.json", { ...root, negativeMatrix: value.negativeMatrix, failClosed: true }],
    ["G1-07-DIFF.json", { ...root, sourceDelta: value.sourceDelta, materializedDelta: REQUIRED_EVIDENCE_FILES, forbiddenProductionChanges: true }],
    ["G1-07-GATE-MANIFEST.json", { ...root, requiredEvidenceFiles: REQUIRED_EVIDENCE_FILES, evidenceStatus: "FACTS_BOUND", sourceToMaterialized: "EVIDENCE_ONLY_DESCENDANT", p34Claimed: false }],
  ]);
  for (const [name, body] of records) writeFileSync(resolve(target, name), json(body));
  writeFileSync(resolve(target, "G1-07-CLI.txt"), `${value.ci.workflowName}\nsource_ref=${sourceRef}\nprocess_level=true\ncommands=run,step,seek,object,projection\ntrace_operation_count=${value.replay.operationCount}\nstep_applied=${value.stepMatrix.applied}\nseek_positions=${value.seek.positions.join(",")}\ncli_process=${JSON.stringify(value.cliProcess)}\nstatus=PASS\n`);
  const on = value.regressionTests.protobufOn;
  const off = value.regressionTests.protobufOff;
  const full = value.regressionTests.fullSemantic;
  writeFileSync(resolve(target, "G1-07-CTEST.txt"), `protobuf_on=${JSON.stringify(on)}\nprotobuf_off=${JSON.stringify(off)}\nfull_semantic=${JSON.stringify(full)}\nclean_checkout=${value.cleanCheckout}\n`);
  validateEvidenceInventory(target);
  return target;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const args = process.argv.slice(2);
  const value = (name) => { const index = args.indexOf(name); return index >= 0 ? args[index + 1] : undefined; };
  const sourceRef = value("--source-ref"); const factsPath = value("--facts"); const out = value("--out");
  if (!sourceRef || !factsPath || !out) fail("usage: generate_g1_07_evidence.mjs --source-ref SHA --facts FILE --out DIR");
  const facts = JSON.parse(await import("node:fs").then(({ readFileSync }) => readFileSync(factsPath, "utf8")));
  generateEvidence({ sourceRef, facts, out });
}
