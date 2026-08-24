import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const root = new URL("../", import.meta.url);
const suite = JSON.parse(await readFile(new URL("platform/protocol/v1/suites/protocol-seed-v0.1.json", root), "utf8"));
const vectors = await Promise.all(suite.vectorRefs.map(async (ref) => JSON.parse(await readFile(new URL(ref.replace(/^verification\//, ""), root), "utf8"))));
const expectedFamilies = new Map([["ENV-SESSION",8],["ACTION-RECEIPT",8],["COMPLETION-TOKEN",8],["SOURCE-EVENT",8],["FAULT-HANDLE",8],["LATE-FENCE",8],["BOUNDARY-FINAL",8]]);
test("protocol-seed-v0.1 has exactly 56 unique stable vectors", () => {
  assert.equal(suite.id, "protocol-seed-v0.1"); assert.equal(suite.vectorRefs.length, 56);
  assert.equal(new Set(suite.vectorRefs).size, 56); assert.equal(new Set(vectors.map((v) => v.id)).size, 56);
  for (const vector of vectors) { assert.match(vector.id, /^HPR-[A-Z0-9-]+$/); assert.equal(vector.format, "axiom-platform-protocol-vector-v1"); assert.equal(vector.input.requirementStatus, "FREEZE_CANDIDATE"); assert.deepEqual(vector.input.boundaryModes, ["IN_PROCESS", "OUT_OF_PROCESS"]); assert.ok(vector.input.authorityRefs.length > 0); assert.ok(vector.input.focusAreas.length >= 2); assert.ok(vector.input.setup); assert.ok(Array.isArray(vector.input.steps) && vector.input.steps.length > 0); assert.ok(vector.expected.oracle); assert.ok(Array.isArray(vector.expected.assertions)); }
  for (const [family, count] of expectedFamilies) assert.equal(vectors.filter((v) => v.id.startsWith(`HPR-${family}-`)).length, count, family);
});

test("expected values are contract-authored and do not contain observed runner output", () => {
  for (const vector of vectors) { assert.equal(vector.expected.diagnosticTextPolicy, "not-compared"); assert.equal(Object.hasOwn(vector.expected, "observed"), false); assert.equal(Object.hasOwn(vector.expected, "actual"), false); }
});

test("blocking negative families are represented", () => {
  const actions = vectors.map((v) => v.input.focusAreas.at(-1));
  for (const required of ["duplicate-completion-reject", "stale-generation-reject", "lease-leak", "fault-pulse-mismatch", "transport-disconnect", "boundary-equivalence"]) assert.ok(actions.includes(required), required);
});

for (const vector of vectors) test(`protocol vector ${vector.id}`, () => {
  assert.ok(["PASS", "REJECTED"].includes(vector.expected.terminal)); assert.ok(["PASS", "REJECT", "DISCONNECT", "EQUIVALENCE", "RENEGOTIATE"].includes(vector.expected.oracle));
});
