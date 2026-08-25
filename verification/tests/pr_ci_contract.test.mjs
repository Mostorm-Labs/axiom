import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtemp, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const root = resolve(fileURLToPath(new URL("../../", import.meta.url)));

test("semantic bootstrap is explicitly non-G1 and bound to deterministic sources", async () => {
  const temp = await mkdtemp(join(tmpdir(), "axiom-semantic-bootstrap-"));
  const junit = join(temp, "ctest.xml");
  const output = join(temp, "summary.json");
  await writeFile(junit, '<testsuites><testsuite><testcase name="OperationReplay.Deterministic"/><testcase name="DocumentDigest.Stable"/></testsuite></testsuites>');
  const result = spawnSync("python3", [join(root, "verification/tools/generate_semantic_bootstrap.py"), "--junit", junit, "--output", output, "--source-commit", "WORKTREE"], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const summary = JSON.parse(await readFile(output, "utf8"));
  assert.equal(summary.status, "PASS");
  assert.equal(summary.authority, "GT-G0-14_BOOTSTRAP_ONLY");
  assert.ok(summary.limitations.includes("NOT_G1_SEMANTIC_ACCEPTANCE"));
  assert.match(summary.corpusSha256, /^[0-9a-f]{64}$/);
});

test("PR workflow exposes the accepted dependency layers and aggregate always runs", async () => {
  const workflow = await readFile(join(root, ".github/workflows/g0-pr-ci-dag.yml"), "utf8");
  for (const name of ["verification/schema-validate", "platform/protocol-seed", "conformance/pr-seed", "platform/pr-contract", "platform/pr-gate"]) {
    assert.match(workflow, new RegExp(name.replace("/", "\\/")));
  }
  assert.match(workflow, /needs:\s*\[schema-validate, protocol-seed, semantic-seed\]/);
  assert.match(workflow, /if:\s*\$\{\{ always\(\) \}\}/);
  assert.match(workflow, /npm run test --workspace @axiom\/platform-harness-runner/);
  for (const layer of ["schema", "protocol", "semantic", "platform"]) {
    assert.match(workflow, new RegExp(`inputs\\.failure_layer \\}\\}" = ${layer}`));
  }
  const protocolWorkflow = await readFile(join(root, ".github/workflows/g0-platform-protocol-seed.yml"), "utf8");
  assert.doesNotMatch(protocolWorkflow, /npm exec -- axiom-platform-conformance/);
  assert.match(protocolWorkflow, /node packages\/platform-conformance-cli\/dist\/main\.js protocol/);
});

test("checked-in GT-G0-14 evidence binds hosted success and four failure layers", async () => {
  const evidence = join(root, "verification/evidence/g0/gt-g0-14");
  const manifest = JSON.parse(await readFile(join(evidence, "manifest.json"), "utf8"));
  assert.equal(manifest.sourceCommit, "7d9c22df422e8e983d49cf3011865804aa09189c");
  const summary = JSON.parse(await readFile(join(evidence, "summary.json"), "utf8"));
  assert.equal(summary.status, "COMMIT_BOUND_HOSTED_VALIDATED");
  assert.equal(summary.hostedValidation, "PASS");
  assert.equal(summary.hostedRunCount, 5);
  assert.ok(!summary.limitations.includes("HOSTED_PR_DAG_PENDING"));
  const hosted = JSON.parse(await readFile(join(evidence, "hosted-runs.json"), "utf8"));
  assert.equal(hosted.implementationCommit, manifest.sourceCommit);
  assert.equal(hosted.normalRun.decision, "PASS");
  assert.equal(hosted.normalRun.conclusion, "success");
  const expected = new Map([["schema", "INVALID_EVIDENCE"], ["protocol", "FAIL"], ["semantic", "FAIL"], ["platform", "FAIL"]]);
  for (const run of hosted.deliberateFailureRuns) {
    assert.equal(run.conclusion, "failure");
    assert.equal(run.failedLayer, run.injectedLayer);
    assert.equal(run.decision, expected.get(run.injectedLayer));
    assert.match(run.artifact.digest, /^sha256:[0-9a-f]{64}$/);
    expected.delete(run.injectedLayer);
  }
  assert.equal(expected.size, 0);
  for (const layer of ["schema", "protocol", "semantic", "platform"]) {
    const decision = JSON.parse(await readFile(join(evidence, "failure-attribution", `${layer}-failure.json`), "utf8"));
    assert.equal(decision.failedLayer, layer);
  }
  for (const entry of manifest.files) {
    const bytes = await readFile(join(evidence, entry.path));
    const { createHash } = await import("node:crypto");
    assert.equal(createHash("sha256").update(bytes).digest("hex"), entry.sha256);
  }
});
