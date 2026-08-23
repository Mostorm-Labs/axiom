import assert from "node:assert/strict";
import { readdir, readFile } from "node:fs/promises";
import { test } from "node:test";

test("protocol package has no runtime or platform dependency", async () => {
  const packageJson = JSON.parse(await readFile(new URL("../package.json", import.meta.url), "utf8"));
  assert.equal(packageJson.dependencies, undefined);
  const files = (await readdir(new URL("../src/", import.meta.url))).filter((name) => name.endsWith(".ts"));
  const externalImport = /\bfrom\s+["'](?!\.)[^"']+["']/;
  const forbiddenRuntimeReference = /(?:node:child_process|node:net|react-native|axiom_runtime)/i;
  for (const file of files) {
    const source = await readFile(new URL(`../src/${file}`, import.meta.url), "utf8");
    assert.doesNotMatch(source, externalImport, `${file}: external import`);
    assert.doesNotMatch(source, forbiddenRuntimeReference, `${file}: runtime/platform reference`);
  }
});
