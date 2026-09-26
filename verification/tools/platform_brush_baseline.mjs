#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const fixturePath = join(root, "fixtures/platform-brush-baseline-v1/fixture.json");
const platforms = ["android", "web", "windows"];
const q = value => Math.round(Number(value) * 1000);
const sha256 = bytes => createHash("sha256").update(bytes).digest("hex");
const canonical = value => {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === "object") return Object.fromEntries(Object.keys(value).sort().map(key => [key, canonical(value[key])]));
  return value;
};
const digest = value => sha256(JSON.stringify(canonical(value)));
const strictKeys = (value, expected, path) => {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${path}: object required`);
  const actual = Object.keys(value);
  const unknown = actual.filter(key => !expected.includes(key));
  const missing = expected.filter(key => !actual.includes(key));
  if (unknown.length || missing.length) throw new Error(`${path}: strict keys mismatch unknown=${unknown.join(",")} missing=${missing.join(",")}`);
};

function requireObservation(value, platform, fixtureDigest) {
  const required = ["schema_version", "fixture_digest", "platform", "reality", "artifact_identity", "runtime_identity", "input", "brush", "render_path", "preview_surface", "classification", "diagnostics"];
  strictKeys(value, required, platform);
  if (value.schema_version !== "platform-brush-baseline-v1" || value.fixture_digest !== fixtureDigest || value.platform !== platform) throw new Error(`${platform}: identity mismatch`);
  if (!["PHYSICAL", "EMULATOR", "HOSTED", "STATIC_ONLY"].includes(value.reality)) throw new Error(`${platform}: invalid reality`);
  if (value.reality !== "STATIC_ONLY" && (typeof value.artifact_identity !== "string" || value.artifact_identity.length === 0)) throw new Error(`${platform}: real observation requires artifact_identity`);
  if (!["PASS", "PASS_WITH_FINDINGS", "BLOCKED_EVIDENCE"].includes(value.classification)) throw new Error(`${platform}: invalid classification`);
  const inputKeys = ["normalized_samples", "pointer_identity", "phase_lifecycle", "viewport_claim", "content_coordinates", "interaction_decisions"];
  strictKeys(value.input, inputKeys, `${platform}.input`);
  for (const key of inputKeys) if (!Array.isArray(value.input[key])) throw new Error(`${platform}: input.${key} array required`);
  if (value.reality !== "STATIC_ONLY" && value.classification !== "BLOCKED_EVIDENCE" && inputKeys.some(key => value.input[key].length === 0)) throw new Error(`${platform}: passing observation requires non-empty canonical input evidence`);
  strictKeys(value.brush, ["profile_id", "package_id", "package_revision", "package_digest", "resolved_state_digest", "preview_outline_digest", "sealed_outline_digest", "replay_digest"], `${platform}.brush`);
  strictKeys(value.render_path, ["input_boundary", "runtime_boundary", "renderer", "surface_type", "submission_count", "readback_count", "cpu_copy_count", "present_count", "viewport_transform_applied"], `${platform}.render_path`);
  strictKeys(value.preview_surface, ["canonical_surface_generation", "preview_surface_generation", "preview_content_revision", "preview_submitted_revision", "preview_dirty", "preview_submission_count", "preview_present_count", "geometry_source", "preview_style", "resize_events", "surface_lost_events", "rebind_events"], `${platform}.preview_surface`);
  strictKeys(value.preview_surface.preview_style, ["color", "blend"], `${platform}.preview_surface.preview_style`);
  if (value.preview_surface.geometry_source !== "BrushPreviewDelta.outline" || value.preview_surface.blend === "invalid") throw new Error(`${platform}: invalid preview surface contract`);
  if (JSON.stringify(value.preview_surface.preview_style.color) !== JSON.stringify([1, 0.85, 0, 0.55]) || value.preview_surface.preview_style.blend !== "src_over") throw new Error(`${platform}: invalid preview style`);
  for (const key of ["submission_count", "readback_count", "cpu_copy_count", "present_count"]) if (!Number.isInteger(value.render_path?.[key]) || value.render_path[key] < 0) throw new Error(`${platform}: invalid render counter ${key}`);
  if (value.reality !== "STATIC_ONLY" && value.classification !== "BLOCKED_EVIDENCE" && (!value.brush?.resolved_state_digest || !value.brush?.preview_outline_digest || !value.brush?.sealed_outline_digest || !value.brush?.replay_digest)) throw new Error(`${platform}: passing observation requires BrushSession digests`);
  if (value.reality !== "STATIC_ONLY" && value.classification !== "BLOCKED_EVIDENCE" && value.brush.sealed_outline_digest !== value.brush.replay_digest) throw new Error(`${platform}: sealed/replay digest mismatch`);
  return value;
}

function projection(observation, logicalSources) {
  const allowedSources = new Set(Object.keys(logicalSources));
  const normalized = observation.input.normalized_samples.map(sample => ({
    source: String(sample.source), pointer: Number(sample.pointer), generation: Number(sample.generation),
    sequence: Number(sample.sequence), x: q(sample.x), y: q(sample.y), pressure: q(sample.pressure), phase: String(sample.phase)
  }));
  if (normalized.some(sample => !allowedSources.has(sample.source))) throw new Error(`${observation.platform}: normalized source is not a fixture logical source`);
  return canonical({
    normalized,
    pointer_identity: observation.input.pointer_identity,
    phase_lifecycle: observation.input.phase_lifecycle,
    viewport_claim: observation.input.viewport_claim.map(item => ({ ...item, scale: q(item.scale), translation_x: q(item.translation_x), translation_y: q(item.translation_y) })),
    content_coordinates: observation.input.content_coordinates.map(item => ({ ...item, x: q(item.x), y: q(item.y) })),
    brush: observation.brush,
    preview_surface: {
      geometry_source: observation.preview_surface.geometry_source,
      preview_style: observation.preview_surface.preview_style,
      preview_content_revision: Number(observation.preview_surface.preview_content_revision),
      preview_submitted_revision: Number(observation.preview_surface.preview_submitted_revision)
    }
  });
}

function staticObservation(platform, fixtureDigest, fixture) {
  const paths = {
    android: ["MotionEvent.history→JNI→PlatformPointerBatch", "C++ InkPlaygroundHost", "Skia raster", "CPU raster buffer"],
    web: ["PointerEvent.coalescedEvents→WASM", "C++ InkPlaygroundHost", "Skia Ganesh", "WebGL2 canvas framebuffer"],
    windows: ["WM_POINTER history→PlatformPointerBatch", "C++ InkPlaygroundHost", "Skia raster", "CPU raster buffer→GDI"]
  }[platform];
  return {
    schema_version: "platform-brush-baseline-v1", fixture_digest: fixtureDigest, platform,
    reality: "STATIC_ONLY", artifact_identity: null,
    input: { normalized_samples: [], pointer_identity: [], phase_lifecycle: [], viewport_claim: [], content_coordinates: [], interaction_decisions: [] },
    runtime_identity: { semantic_generation: 0, scene_revision: 0, surface_generation: 0, canonical_commit_ordinal: 0 },
    brush: { profile_id: fixture.brush_package.profile_id, package_id: fixture.brush_package.package_id, package_revision: fixture.brush_package.revision, package_digest: null, resolved_state_digest: null, preview_outline_digest: null, sealed_outline_digest: null, replay_digest: null },
    render_path: { input_boundary: paths[0], runtime_boundary: paths[1], renderer: paths[2], surface_type: paths[3], submission_count: 0, readback_count: 0, cpu_copy_count: 0, present_count: 0, viewport_transform_applied: "unknown" },
    preview_surface: { canonical_surface_generation: 0, preview_surface_generation: 0, preview_content_revision: 0, preview_submitted_revision: 0, preview_dirty: false, preview_submission_count: 0, preview_present_count: 0, geometry_source: "BrushPreviewDelta.outline", preview_style: { color: [1, 0.85, 0, 0.55], blend: "src_over" }, resize_events: 0, surface_lost_events: 0, rebind_events: 0 },
    classification: "BLOCKED_EVIDENCE", diagnostics: ["No real platform observation supplied; static path inventory is not end-to-end evidence."]
  };
}

async function writeManifest(output, fixtureDigest, observations, files) {
  const entries = [];
  for (const path of files) { const bytes = await readFile(join(output, path)); entries.push({ path, bytes: bytes.length, sha256: sha256(bytes) }); }
  const manifest = { schema_version: "platform-brush-baseline-manifest-v1", fixture_digest: fixtureDigest, runner_version: "1.0.0", observations: observations.map(value => ({ platform: value.platform, reality: value.reality, artifact_identity: value.artifact_identity, classification: value.classification })), files: entries };
  await writeFile(join(output, "manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
}

async function main(args) {
  const command = args[0];
  const outputFlag = args.indexOf("--output");
  const observationsFlag = args.indexOf("--observations");
  if (!["scaffold", "compare"].includes(command) || outputFlag < 0 || !args[outputFlag + 1]) throw new Error("usage: platform_brush_baseline.mjs scaffold|compare --output DIR [--observations DIR]");
  const output = resolve(args[outputFlag + 1]);
  const fixtureBytes = await readFile(fixturePath);
  const fixture = JSON.parse(fixtureBytes);
  const fixtureDigest = sha256(fixtureBytes);
  await mkdir(output, { recursive: true });
  await writeFile(join(output, "fixture.json"), fixtureBytes);
  const observations = [];
  for (const platform of platforms) {
    let value;
    if (command === "compare" && observationsFlag >= 0 && args[observationsFlag + 1]) {
      try { value = JSON.parse(await readFile(resolve(args[observationsFlag + 1], platform, "observation.json"), "utf8")); }
      catch { value = staticObservation(platform, fixtureDigest, fixture); }
    } else value = staticObservation(platform, fixtureDigest, fixture);
    requireObservation(value, platform, fixtureDigest);
    observations.push(value);
    await mkdir(join(output, platform), { recursive: true });
    await writeFile(join(output, platform, "observation.json"), `${JSON.stringify(value, null, 2)}\n`);
  }
  const comparable = observations.filter(value => value.reality !== "STATIC_ONLY" && value.classification !== "BLOCKED_EVIDENCE");
  const projections = comparable.map(value => ({ platform: value.platform, digest: digest(projection(value, fixture.logical_sources)) }));
  const allReal = comparable.length === platforms.length;
  const equivalent = allReal && new Set(projections.map(value => value.digest)).size === 1;
  const findings = observations.some(value => value.classification === "PASS_WITH_FINDINGS" || value.render_path.submission_count === 0 || value.render_path.present_count === 0 || value.render_path.readback_count > 0 || value.render_path.cpu_copy_count > 0 || value.render_path.viewport_transform_applied !== "true" || value.preview_surface.preview_submission_count === 0);
  const classification = !allReal ? "BLOCKED_EVIDENCE" : equivalent ? (findings ? "PASS_WITH_FINDINGS" : "PASS") : "BLOCKED_EVIDENCE";
  const comparison = { schema_version: "platform-brush-comparison-v1", fixture_digest: fixtureDigest, classification, all_platforms_real: allReal, canonical_equivalence: equivalent, projections, diagnostics: !allReal ? ["Real Android, Web, and Windows observations are required."] : equivalent ? [] : ["Canonical platform projections diverged."] };
  await writeFile(join(output, "comparison.json"), `${JSON.stringify(comparison, null, 2)}\n`);
  await writeFile(join(output, "render-path-summary.json"), `${JSON.stringify({ schema_version: "platform-render-path-summary-v1", platforms: observations.map(({ platform, reality, render_path, classification }) => ({ platform, reality, render_path, classification })) }, null, 2)}\n`);
  const files = ["fixture.json", ...platforms.map(value => `${value}/observation.json`), "comparison.json", "render-path-summary.json"];
  await writeManifest(output, fixtureDigest, observations, files);
  console.log(`${classification}: ${output}`);
  process.exitCode = classification === "BLOCKED_EVIDENCE" ? 20 : 0;
}

main(process.argv.slice(2)).catch(error => { console.error(error.message); process.exitCode = 10; });
