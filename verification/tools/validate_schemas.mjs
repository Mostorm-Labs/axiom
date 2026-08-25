import { readdir, readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const dir = resolve(root, "schemas/platform");
const expected = [
  "platform-suite.schema.json", "platform-scenario.schema.json", "platform-profile.schema.json",
  "platform-trace.schema.json", "platform-observation.schema.json", "platform-result.schema.json",
  "platform-harness-envelope.schema.json", "platform-harness-session.schema.json",
  "platform-fault-hook.schema.json", "platform-late-event-fence.schema.json",
  "platform-protocol-suite.schema.json", "platform-protocol-vector.schema.json",
  "platform-protocol-meta-result.schema.json", "pr-run-set.schema.json",
  "pr-layer-record.schema.json", "pr-decision.schema.json",
  "semantic-bootstrap-summary.schema.json", "full-run-set.schema.json",
  "platform-evidence-record.schema.json", "platform-evidence-index.schema.json",
  "platform-release-decision.schema.json", "reproducibility-comparison.schema.json"
].sort();
const actual = (await readdir(dir)).filter((name) => name.endsWith(".schema.json")).sort();
if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`schema inventory mismatch: ${actual.length}/${expected.length}`);
const ids = new Set();
const schemaByName = new Map();
for (const name of actual) {
  const value = JSON.parse(await readFile(resolve(dir, name), "utf8"));
  if (value.$schema !== "https://json-schema.org/draft/2020-12/schema" || typeof value.$id !== "string") throw new Error(`${name}: invalid schema metadata`);
  if (ids.has(value.$id)) throw new Error(`${name}: duplicate $id`);
  ids.add(value.$id);
  if (value.type !== "object" || value.additionalProperties !== false) throw new Error(`${name}: top-level strict object required`);
  if (!Array.isArray(value.required) || value.required.length === 0) throw new Error(`${name}: required fields missing`);
  schemaByName.set(name, value);
}

function resolveRef(rootSchema, ref) {
  if (!ref.startsWith("#/")) throw new Error(`external $ref is not supported in G0-01 validator: ${ref}`);
  return ref.slice(2).split("/").reduce((value, key) => value[key.replaceAll("~1", "/").replaceAll("~0", "~")], rootSchema);
}

export function validateValue(schema, value, path = "$", rootSchema = schema) {
  if (schema.$ref) return validateValue(resolveRef(rootSchema, schema.$ref), value, path, rootSchema);
  if (Object.hasOwn(schema, "const") && !Object.is(value, schema.const)) throw new Error(`${path}: const mismatch`);
  if (schema.enum && !schema.enum.some((candidate) => Object.is(candidate, value))) throw new Error(`${path}: enum mismatch`);
  const types = Array.isArray(schema.type) ? schema.type : schema.type ? [schema.type] : [];
  if (types.length) {
    const actualType = value === null ? "null" : Array.isArray(value) ? "array" : Number.isInteger(value) ? "integer" : typeof value === "number" ? "number" : typeof value;
    if (!types.includes(actualType) && !(actualType === "integer" && types.includes("number"))) throw new Error(`${path}: expected ${types.join("|")}`);
  }
  if (typeof value === "string") {
    if (schema.minLength !== undefined && value.length < schema.minLength) throw new Error(`${path}: string too short`);
    if (schema.pattern && !new RegExp(schema.pattern, "u").test(value)) throw new Error(`${path}: pattern mismatch`);
  }
  if (Array.isArray(value)) {
    if (schema.minItems !== undefined && value.length < schema.minItems) throw new Error(`${path}: too few items`);
    if (schema.uniqueItems && new Set(value.map((item) => JSON.stringify(item))).size !== value.length) throw new Error(`${path}: duplicate items`);
    if (schema.items) value.forEach((item, index) => validateValue(schema.items, item, `${path}[${index}]`, rootSchema));
  }
  if (value !== null && typeof value === "object" && !Array.isArray(value)) {
    for (const key of schema.required ?? []) if (!Object.hasOwn(value, key)) throw new Error(`${path}: missing ${key}`);
    const known = schema.properties ?? {};
    if (schema.additionalProperties === false) for (const key of Object.keys(value)) if (!Object.hasOwn(known, key)) throw new Error(`${path}: unknown ${key}`);
    for (const [key, child] of Object.entries(known)) if (Object.hasOwn(value, key)) validateValue(child, value[key], `${path}.${key}`, rootSchema);
  }
}

const fixtureDir = resolve(dir, "fixtures");
for (const name of actual) {
  const fixtureName = name.replace(".schema.json", ".valid.json");
  const fixture = JSON.parse(await readFile(resolve(fixtureDir, fixtureName), "utf8"));
  validateValue(schemaByName.get(name), fixture);
}
console.log(`schema validation: ${actual.length} schemas and ${actual.length} fixtures valid`);
