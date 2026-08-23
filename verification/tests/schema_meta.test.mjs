import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
async function pair(name) {
  const schema = JSON.parse(await readFile(resolve(root, `schemas/platform/${name}.schema.json`), "utf8"));
  const value = JSON.parse(await readFile(resolve(root, `schemas/platform/fixtures/${name}.valid.json`), "utf8"));
  return { schema, value };
}

test("unknown top-level field is rejected", async () => {
  const { schema, value } = await pair("platform-profile");
  value.unexpected = true;
  assert.throws(() => validateValue(schema, value), /unknown unexpected/);
});

test("wrong formatVersion is rejected", async () => {
  const { schema, value } = await pair("platform-scenario");
  value.formatVersion = 2;
  assert.throws(() => validateValue(schema, value), /const mismatch/);
});

test("unsafe artifact paths are rejected", async () => {
  const { schema, value } = await pair("platform-observation");
  for (const path of ["/tmp/trace.json", "../trace.json", "C:/trace.json"]) {
    value.artifacts = { lifecycleTrace: path };
    assert.throws(() => validateValue(schema, value), /pattern mismatch/);
  }
});

test("invalid tagged u64 generation is rejected", async () => {
  const { schema, value } = await pair("platform-late-event-fence");
  for (const generation of ["1", "u64:1", "u64:000000000000000G"]) {
    value.generation = generation;
    assert.throws(() => validateValue(schema, value), /pattern mismatch/);
  }
});
