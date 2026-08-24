import assert from "node:assert/strict";
import { mkdtemp, readFile, readdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import test from "node:test";

test("all 56 vectors execute on both boundaries", async () => {
  const output = await mkdtemp(join(tmpdir(), "axiom-protocol-execution-"));
  const result = spawnSync(process.execPath, ["tools/run_protocol_vectors.mjs"], { cwd:new URL("../", import.meta.url), encoding:"utf8", env:{...process.env,AXIOM_PROTOCOL_EVIDENCE_ROOT:output} });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const manifest = JSON.parse(await readFile(join(output,"corpus-integrity.json"),"utf8"));
  assert.equal(manifest.resultCount, 112); assert.equal(manifest.results.filter((item) => item.status === "FAIL").length, 0);
  for (const boundary of ["IN_PROCESS","OUT_OF_PROCESS"]) assert.equal((await readdir(join(output,"protocol-meta-results",boundary))).length,56);
});
