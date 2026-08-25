import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { execFileSync } from "node:child_process";
import { mkdtemp, readFile, readdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const root = resolve(fileURLToPath(new URL("../../", import.meta.url)));
const generator = join(root, "verification/tools/generate_g0_15_evidence.mjs");

test("GT-G0-15 WORKTREE Evidence has five profiles, PG groups and complete manifest hashes", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-g0-15-evidence-"));
  execFileSync(process.execPath, [generator], { cwd: root, env: { ...process.env, AXIOM_G0_15_OUTPUT: output, AXIOM_EVIDENCE_SOURCE_COMMIT: "WORKTREE" }, stdio: "pipe" });
  const runSet = JSON.parse(await readFile(join(output, "run-set.json"), "utf8"));
  assert.equal(runSet.authority, "G0_WIRING_ONLY");
  assert.deepEqual(runSet.requiredProfiles.map(({ profileKey }) => profileKey), ["web", "windows", "android", "ios", "ipados"]);
  const decision = JSON.parse(await readFile(join(output, "platform-release-decision.json"), "utf8"));
  assert.equal(decision.authority, "G0_WIRING_ONLY");
  assert.equal(decision.decision, "PASS");
  assert.deepEqual(decision.pgStatuses.map(({ group }) => group), ["PG-01", "PG-02", "PG-03", "PG-04", "PG-05", "PG-06"]);
  const comparison = JSON.parse(await readFile(join(output, "reproducibility-comparison.json"), "utf8"));
  assert.equal(comparison.status, "PASS");
  const summary = JSON.parse(await readFile(join(output, "summary.json"), "utf8"));
  assert.equal(summary.sourceCommit, "WORKTREE");
  assert.ok(summary.limitations.includes("NOT_COMMIT_BOUND"));
  const manifest = JSON.parse(await readFile(join(output, "manifest.json"), "utf8"));
  for (const entry of manifest.files) {
    const bytes = await readFile(join(output, entry.path));
    assert.equal(createHash("sha256").update(bytes).digest("hex"), entry.sha256);
  }
  assert.ok((await readdir(join(output, "deliberate-failures"))).length >= 5);
});
