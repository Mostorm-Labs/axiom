import { mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { isAbsolute, resolve } from "node:path";

export const TASK_ID = "GT-G1-08";
export const PACKAGE_REF = "notion://3d64c57a-590c-8196-813d-ffd71b4f52f7/GT-G1-08-P31-v0.2";
export const PACKAGE_MATERIALIZATION_REF = "da844b3bd495608ebc5b12c14512fa0399dae580";
export const TASK_ANCHOR = "a2c3bfa05930b53739886d4d151be31dbcd6be15";
export const EXECUTION_REF = "codex/gt-g1-08-reference-differential-locality";
export const SOURCE_PATHS = [
  ".github/workflows/g1-08-exact-source.yml",
  "runtime/semantic/src/g1_08_indexed_access_probe_internal.hpp",
  "runtime/semantic/src/indexed_object_store.cpp",
  "runtime/semantic/tests/CMakeLists.txt",
  "runtime/semantic/tests/g1_08_locality_test.cpp",
  "runtime/semantic/tests/g1_08_reference_differential_test.cpp",
  "runtime/semantic/tools/CMakeLists.txt",
  "runtime/semantic/tools/g1_08_locality_workload.cpp",
  "runtime/semantic/tools/g1_08_locality_workload.hpp",
  "runtime/semantic/tools/g1_08_verifier.cpp",
  "runtime/semantic/tools/g1_08_verifier.hpp",
  "runtime/semantic/tools/g1_08_verifier_main.cpp",
  "verification/packages/semantic-conformance-cli/test/g1-08-exact-source-evidence.test.mjs",
  "verification/tools/generate_g1_08_evidence.mjs",
];
export const REQUIRED = ["G1-08-CORRECTNESS.json", "G1-08-LOCALITY.json", "G1-08-CTEST-ON.xml", "G1-08-CTEST-OFF.xml", "G1-08-RUN-MANIFEST.json"];
const RELEASE_SET_ID = "14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea";
const sha = value => typeof value === "string" && /^[0-9a-f]{40}$/.test(value);
const fail = message => { throw new Error(message); };
const record = (value, label) => { if (!value || typeof value !== "object" || Array.isArray(value)) fail(`${label} must be an object`); return value; };
const nonEmpty = (value, label) => { if (typeof value !== "string" || value.length === 0) fail(`${label} is missing`); return value; };
const suite = (value, label) => { const result = record(value, label); for (const key of ["total", "passed", "skipped", "failed"]) if (!Number.isInteger(result[key]) || result[key] < 0) fail(`${label} accounting invalid`); if (result.failed !== 0 || result.total !== result.passed + result.skipped) fail(`${label} did not pass`); return result; };

export function validateFacts(facts, expected = {}) {
  const value = record(facts, "facts");
  if (value.format !== "axiom-gt-g1-08-facts-v1" || value.taskId !== TASK_ID || value.packageRef !== PACKAGE_REF) fail("task/package identity mismatch");
  if (value.packageMaterializationRef !== PACKAGE_MATERIALIZATION_REF || value.executionRef !== EXECUTION_REF) fail("package materialization/execution binding mismatch");
  if (value.repository !== "Mostorm-Labs/axiom" || value.taskAnchor !== TASK_ANCHOR || value.actualStartingRevision !== TASK_ANCHOR) fail("repository/anchor binding mismatch");
  if (!sha(value.sourceRef) || (expected.sourceRef && value.sourceRef !== expected.sourceRef) || !sha(value.sourceCommitParent)) fail("source binding invalid");
  const delta = record(value.sourceDelta, "source delta");
  if (!Array.isArray(delta.paths) || delta.paths.length !== SOURCE_PATHS.length || JSON.stringify([...delta.paths].sort()) !== JSON.stringify([...SOURCE_PATHS].sort())) fail("source path inventory mismatch");
  const provider = record(value.provider, "provider facts");
  for (const key of ["runId", "attempt", "jobId", "artifactIdentity", "workflow"]) nonEmpty(provider[key], `provider.${key}`);
  if (provider.sourceSha !== value.sourceRef || provider.workflow !== ".github/workflows/g1-08-exact-source.yml") fail("provider source binding mismatch");
  const materialization = record(value.materialization, "materialization facts");
  if (!["EVIDENCE_ONLY_DESCENDANT", "EVIDENCE_ONLY_DESCENDANT_PENDING"].includes(materialization.relation) || materialization.sourceRef !== value.sourceRef || materialization.sourceChanges !== false) fail("materialization provenance mismatch");
  if (materialization.relation === "EVIDENCE_ONLY_DESCENDANT" && (!sha(materialization.ref) || materialization.ref === value.sourceRef)) fail("materialization provenance mismatch");
  const lock = record(value.lock, "lock facts");
  if (lock.path !== "semantic-sdk.lock.json" || !sha(lock.blobSha) || lock.releaseSetId !== RELEASE_SET_ID) fail("locked SDK binding mismatch");
  const machine = record(value.machine, "machine facts");
  for (const key of ["negativePreflight", "protobufOff", "legacyDecoder", "cleanCheckout"]) if (machine[key] !== "PASS") fail(`machine fact ${key} missing`);
  for (const key of ["correctness", "locality"]) if (record(value[key], `${key} facts`).status !== "PASS") fail(`${key} facts did not pass`);
  for (const key of ["ctestOn", "ctestOff"]) { const item = record(value[key], key); nonEmpty(item.command, `${key}.command`); suite(item.result, `${key}.result`); nonEmpty(item.xml, `${key}.xml`); }
  if (!Array.isArray(value.familyOracles) || value.familyOracles.length !== 15 || value.familyOracles.some(item => !item || typeof item.ref !== "string" || item.status !== "PASS")) fail("family oracle inventory incomplete");
  return value;
}
export function assertSafeOutputDirectory(out, repositoryRoot = process.cwd()) {
  if (typeof out !== "string" || out.length === 0 || isAbsolute(out) || !out.replaceAll("\\", "/").startsWith("out/")) fail("evidence output must stay under out/");
  const root = resolve(repositoryRoot); const target = resolve(root, out); if (!target.startsWith(`${root}/`)) fail("evidence output escapes repository root"); return target;
}
export function validateEvidenceInventory(directory) { const names = readdirSync(directory).sort(); if (JSON.stringify(names) !== JSON.stringify([...REQUIRED].sort())) fail("evidence inventory mismatch"); return names; }
export function generateEvidence({ sourceRef, facts, out, repositoryRoot = process.cwd() }) {
  const value = validateFacts(facts, { sourceRef }); const target = assertSafeOutputDirectory(out, repositoryRoot); mkdirSync(target, { recursive: true });
  const root = { format: "axiom-gt-g1-08-evidence-v1", taskId: TASK_ID, packageRef: PACKAGE_REF, packageMaterializationRef: PACKAGE_MATERIALIZATION_REF, repository: "Mostorm-Labs/axiom", taskAnchor: TASK_ANCHOR, executionRef: EXECUTION_REF, sourceRef, provider: value.provider };
  writeFileSync(resolve(target, "G1-08-CORRECTNESS.json"), JSON.stringify({ ...root, correctness: value.correctness, familyOracles: value.familyOracles }, null, 2) + "\n");
  writeFileSync(resolve(target, "G1-08-LOCALITY.json"), JSON.stringify({ ...root, locality: value.locality, machine: value.machine }, null, 2) + "\n");
  const materialization = { ...value.materialization };
  if (materialization.relation === "EVIDENCE_ONLY_DESCENDANT_PENDING") delete materialization.ref;
  writeFileSync(resolve(target, "G1-08-RUN-MANIFEST.json"), JSON.stringify({ ...root, sourceDelta: value.sourceDelta, lock: value.lock, materialization, evidenceInventory: REQUIRED, p34Claimed: false }, null, 2) + "\n");
  writeFileSync(resolve(target, "G1-08-CTEST-ON.xml"), value.ctestOn.xml);
  writeFileSync(resolve(target, "G1-08-CTEST-OFF.xml"), value.ctestOff.xml);
  validateEvidenceInventory(target); return target;
}
