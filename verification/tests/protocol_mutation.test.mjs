import assert from "node:assert/strict";
import { mkdtemp, readFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import test from "node:test";

const mutations = ["duplicate-completion","stale-epoch","lease-leak","fault-transition","late-fence","transport-drop","boundary-equivalence"];
for (const mutation of mutations) test(`mutation guard ${mutation}`, async () => {
  const output = await mkdtemp(join(tmpdir(), `axiom-${mutation}-`));
  const result = spawnSync(process.execPath, ["tools/run_protocol_vectors.mjs"], { cwd:new URL("../", import.meta.url), encoding:"utf8", env:{...process.env,AXIOM_PROTOCOL_MUTATION:mutation,AXIOM_PROTOCOL_EVIDENCE_ROOT:output} });
  assert.notEqual(result.status, 0, `${mutation} must break at least one vector`);
  const manifest = JSON.parse(await readFile(join(output,"corpus-integrity.json"),"utf8"));
  assert.ok(manifest.results.some((item) => item.status === "FAIL"), mutation);
});
