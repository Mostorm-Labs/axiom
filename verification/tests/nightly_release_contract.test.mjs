import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const root = resolve(fileURLToPath(new URL("../../", import.meta.url)));
const readWorkflow = (name) => readFile(join(root, ".github/workflows", name), "utf8");

test("nightly and release use the same reusable full-conformance graph", async () => {
  const reusable = await readWorkflow("g0-full-platform-conformance.yml");
  const nightly = await readWorkflow("g0-nightly.yml");
  const release = await readWorkflow("g0-release-conformance.yml");
  assert.match(reusable, /workflow_call:/);
  assert.match(nightly, /uses:\s*\.\/\.github\/workflows\/g0-full-platform-conformance\.yml/);
  assert.match(release, /uses:\s*\.\/\.github\/workflows\/g0-full-platform-conformance\.yml/);
});

test("release workflow is read-only and pins a 40-character main revision", async () => {
  const release = await readWorkflow("g0-release-conformance.yml");
  assert.match(release, /contents:\s*read/);
  assert.doesNotMatch(release, /contents:\s*write/);
  assert.match(release, /source_commit/);
  assert.match(release, /grep -Eq '.*\{40\}/);
  assert.match(release, /merge-base\s+--is-ancestor/);
  assert.doesNotMatch(release, /gh release create|softprops\/action-gh-release/);
});

test("reusable graph enumerates trusted roots, five profiles, aggregate and artifact chain", async () => {
  const workflow = await readWorkflow("g0-full-platform-conformance.yml");
  for (const term of ["schema", "protocol", "semantic", "web", "windows", "android", "ios", "ipados", "aggregate-full", "compare-full", "actions/upload-artifact"]) assert.match(workflow, new RegExp(term));
  assert.match(workflow, /if:\s*\$\{\{\s*always\(\)\s*\}\}/);
  assert.match(workflow, /repeat_count/);
});

test("workflow matrix never blesses or updates checked-in corpus", async () => {
  for (const name of ["g0-full-platform-conformance.yml", "g0-nightly.yml", "g0-release-conformance.yml"]) {
    const workflow = await readWorkflow(name);
    assert.doesNotMatch(workflow, /--bless|--update-golden|git\s+push/);
  }
});
