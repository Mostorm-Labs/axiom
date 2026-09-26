import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { mkdir, mkdtemp, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import test from "node:test";
import { validateValue } from "../tools/validate_schemas.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const tool = resolve(root, "tools/platform_brush_baseline.mjs");
const fixturePath = resolve(root, "fixtures/platform-brush-baseline-v1/fixture.json");
const fixtureBytes = await readFile(fixturePath);
const fixtureDigest = createHash("sha256").update(fixtureBytes).digest("hex");
const fixtureSchema = JSON.parse(await readFile(resolve(root, "schemas/platform-brush-baseline-v1.schema.json"), "utf8"));
const observationSchema = JSON.parse(await readFile(resolve(root, "schemas/platform-brush-observation-v1.schema.json"), "utf8"));

const baseObservation = platform => ({
  schema_version: "platform-brush-baseline-v1", fixture_digest: fixtureDigest, platform,
  reality: "HOSTED", artifact_identity: `${platform}-artifact`,
  runtime_identity: { semantic_generation: 1, scene_revision: 1, surface_generation: 1, canonical_commit_ordinal: 1 },
  input: {
    normalized_samples: [{ source: "ink", pointer: 1, generation: 1, sequence: 1, x: 1.0004, y: 2.0004, pressure: 0.5004, phase: "down", disposition: "pending", viewport_claimed: false, down_timestamp_ns: 1000, down_time_delta_ns: 0 }],
    pointer_identity: [{ source: "ink", pointer: 1, generation: 1 }], phase_lifecycle: ["down"],
    viewport_claim: [{ sequence: 1, claimed: false, scale: 1, translation_x: 0, translation_y: 0 }],
    content_coordinates: [{ sequence: 1, x: 1.0004, y: 2.0004 }],
    interaction_decisions: [{ sequence: 1, pointer: 1, generation: 1, phase: "down", disposition: "pending", viewport_claimed: false, down_timestamp_ns: 1000, down_time_delta_ns: 0 }]
  },
  brush: { profile_id: "vector-solid-v1", package_id: "00000000000000000000000000000042", package_revision: 1, package_digest: "a", resolved_state_digest: "a", preview_outline_digest: "b", sealed_outline_digest: "c", replay_digest: "c" },
  render_path: { input_boundary: "platform", runtime_boundary: "runtime", renderer: "renderer", surface_type: "surface", submission_count: 1, readback_count: 0, cpu_copy_count: 0, present_count: 1, viewport_transform_applied: "true" },
  preview_surface: { canonical_surface_generation: 1, preview_surface_generation: 1, preview_content_revision: 1, preview_submitted_revision: 1, preview_dirty: false, preview_submission_count: 1, preview_present_count: 1, geometry_source: "BrushPreviewDelta.outline", preview_style: { color: [1, 0.85, 0, 0.55], blend: "src_over" }, resize_events: 1, surface_lost_events: 0, rebind_events: 0 },
  classification: "PASS", diagnostics: []
});

test("fixture and common observation schemas are strict", () => {
  assert.doesNotThrow(() => validateValue(fixtureSchema, JSON.parse(fixtureBytes)));
  const observation = baseObservation("android");
  assert.doesNotThrow(() => validateValue(observationSchema, observation));
  observation.unexpected = true;
  assert.throws(() => validateValue(observationSchema, observation), /unknown unexpected/);
});

test("scaffold fails closed without real platform evidence", async () => {
  const output = await mkdtemp(resolve(tmpdir(), "axiom-brush-scaffold-"));
  const result = spawnSync(process.execPath, [tool, "scaffold", "--output", output], { encoding: "utf8" });
  assert.equal(result.status, 20);
  assert.equal(JSON.parse(await readFile(resolve(output, "comparison.json"), "utf8")).classification, "BLOCKED_EVIDENCE");
});

test("comparator quantizes coordinates and accepts three real equivalent observations", async () => {
  const input = await mkdtemp(resolve(tmpdir(), "axiom-brush-input-"));
  const output = await mkdtemp(resolve(tmpdir(), "axiom-brush-output-"));
  for (const platform of ["android", "web", "windows"]) {
    await mkdir(resolve(input, platform));
    const value = baseObservation(platform);
    if (platform === "web") value.input.normalized_samples[0].x = 1.00049;
    await writeFile(resolve(input, platform, "observation.json"), `${JSON.stringify(value)}\n`);
  }
  const result = spawnSync(process.execPath, [tool, "compare", "--observations", input, "--output", output], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  const comparison = JSON.parse(await readFile(resolve(output, "comparison.json"), "utf8"));
  assert.equal(comparison.classification, "PASS");
  assert.equal(comparison.canonical_equivalence, true);
});

test("comparator detects canonical divergence", async () => {
  const input = await mkdtemp(resolve(tmpdir(), "axiom-brush-divergent-"));
  const output = await mkdtemp(resolve(tmpdir(), "axiom-brush-result-"));
  for (const platform of ["android", "web", "windows"]) {
    await mkdir(resolve(input, platform));
    const value = baseObservation(platform);
    if (platform === "windows") {
      value.brush.sealed_outline_digest = "different";
      value.brush.replay_digest = "different";
    }
    await writeFile(resolve(input, platform, "observation.json"), `${JSON.stringify(value)}\n`);
  }
  const result = spawnSync(process.execPath, [tool, "compare", "--observations", input, "--output", output], { encoding: "utf8" });
  assert.equal(result.status, 20);
  assert.equal(JSON.parse(await readFile(resolve(output, "comparison.json"), "utf8")).canonical_equivalence, false);
});

test("real observations cannot omit artifact identity", async () => {
  const input = await mkdtemp(resolve(tmpdir(), "axiom-brush-unbound-"));
  const output = await mkdtemp(resolve(tmpdir(), "axiom-brush-invalid-"));
  for (const platform of ["android", "web", "windows"]) {
    await mkdir(resolve(input, platform));
    const value = baseObservation(platform);
    if (platform === "android") value.artifact_identity = null;
    await writeFile(resolve(input, platform, "observation.json"), `${JSON.stringify(value)}\n`);
  }
  const result = spawnSync(process.execPath, [tool, "compare", "--observations", input, "--output", output], { encoding: "utf8" });
  assert.equal(result.status, 10);
  assert.match(result.stderr, /artifact_identity/);
});

test("a real platform may report an incomplete corpus only as blocked evidence", async () => {
  const input = await mkdtemp(resolve(tmpdir(), "axiom-brush-incomplete-"));
  const output = await mkdtemp(resolve(tmpdir(), "axiom-brush-incomplete-result-"));
  for (const platform of ["android", "web", "windows"]) {
    await mkdir(resolve(input, platform));
    const value = baseObservation(platform);
    if (platform === "android") {
      value.classification = "BLOCKED_EVIDENCE";
      value.input.normalized_samples = [];
      value.input.pointer_identity = [];
      value.input.phase_lifecycle = [];
      value.input.viewport_claim = [];
      value.input.content_coordinates = [];
      value.brush.resolved_state_digest = null;
      value.brush.preview_outline_digest = null;
      value.brush.sealed_outline_digest = null;
      value.brush.replay_digest = null;
    }
    await writeFile(resolve(input, platform, "observation.json"), `${JSON.stringify(value)}\n`);
  }
  const result = spawnSync(process.execPath,
    [tool, "compare", "--observations", input, "--output", output], { encoding: "utf8" });
  assert.equal(result.status, 20, result.stderr);
  assert.equal(JSON.parse(await readFile(resolve(output, "comparison.json"), "utf8")).classification,
    "BLOCKED_EVIDENCE");
});
