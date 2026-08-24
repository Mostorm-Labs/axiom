import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";
import { WebReferenceAdapter, WEB_PROFILE, WebCanvasHost } from "../dist/index.js";
import { validateValue } from "../../../tools/validate_schemas.mjs";

const verificationRoot = resolve(fileURLToPath(new URL("../../../", import.meta.url)));
const evidenceRoot = resolve(verificationRoot, "evidence/g0/gt-g0-10");
const schema = async (name) => JSON.parse(await readFile(resolve(verificationRoot, `schemas/platform/${name}`), "utf8"));

test("web profile is explicit and does not claim Arc", () => {
  assert.equal(WEB_PROFILE.format, "axiom-platform-profile-v1");
  assert.equal(WEB_PROFILE.platformFamily, "WEB");
  assert.equal(WEB_PROFILE.platformVariant, "browser-wasm-webgl2");
  assert.equal(WEB_PROFILE.capabilities.includes("arc.preview"), false);
  assert.equal(WEB_PROFILE.capabilities.includes("input.pointer_sample_batch"), true);
});

test("adapter reports observed facts without expected or verdict fields", () => {
  const adapter = new WebReferenceAdapter();
  const observation = adapter.execute({
    id: "PLAT-CREATE-CANVAS-001",
    steps: [{ stepId: "s01", kind: "CONTROL", action: { operation: "CREATE_CANVAS" }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } }],
  });
  assert.equal(observation.platformFamily, "WEB");
  assert.deepEqual(observation.steps.map((step) => step.event), ["CANVAS_CREATED"]);
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
  assert.equal(Object.hasOwn(observation.steps[0], "eventSeq"), false);
});

test("logical surface and input actions are deterministic and correlation-preserving", () => {
  const observation = new WebReferenceAdapter().execute({
    id: "PLAT-INPUT-BATCH-NORMALIZED-001",
    steps: [
      { stepId: "s01", kind: "CONTROL", action: { operation: "CREATE_CANVAS" }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } },
      { stepId: "s02", kind: "METRICS_UPDATE", action: { logicalWidth: 100, logicalHeight: 50, deviceScale: 2, visible: true, occluded: false }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } },
      { stepId: "s03", kind: "INPUT", action: { operation: "DELIVER_POINTER_SAMPLE_BATCH", fixtureRef: "POINTER-PEN-MOVE-COALESCED-001", correlationId: "batch-01" }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } },
      { stepId: "s04", kind: "FAULT", action: { type: "SURFACE_LOST", mode: "PULSE" }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } },
      { stepId: "s05", kind: "CONTROL", action: { operation: "PROVIDE_SURFACE_REBIND" }, completion: { mode: "WAIT_FOR_ACTION_COMPLETION" } },
    ],
  });
  assert.deepEqual(observation.steps.map((step) => step.event), ["CANVAS_CREATED", "METRICS_CHANGED", "INPUT_BATCH_DELIVERED", "SURFACE_UNAVAILABLE", "SURFACE_REBOUND"]);
  assert.equal(observation.steps[2].correlationId, "batch-01");
  assert.equal(observation.realization.surfaceGeneration, "u64:0000000000000002");
});

test("adapter rejects correctness sleeps and never exposes scenario expected", () => {
  assert.throws(() => new WebReferenceAdapter().execute({ id: "x", steps: [{ stepId: "s01", kind: "WAIT", action: { sleep: 10 }, completion: { mode: "DISPATCH_ONLY" } }] }), /correctness sleep/);
});

test("browser host owns canvas facts and forwards pointer batches without React state", () => {
  const canvas = { width: 1, height: 1 };
  const host = new WebCanvasHost(canvas);
  const events = [];
  host.addObserver((event) => events.push(event));
  let delivered;
  host.resize({ logicalWidth: 10, logicalHeight: 5, physicalWidth: 20, physicalHeight: 10, deviceScale: 2, visible: true, occluded: false });
  host.deliverPointerBatch({ correlationId: "batch-01", samples: [{ x: 1, pressure: 0.5 }] }, (batch) => { delivered = batch; });
  assert.deepEqual(delivered, { correlationId: "batch-01", samples: [{ x: 1, pressure: 0.5 }] });
  assert.equal(canvas.width, 20);
  assert.deepEqual(events.map((event) => event.kind), ["METRICS_CHANGED", "INPUT_BATCH_DELIVERED"]);
});

test("generated Web Evidence conforms to shared profile, observation and result schemas", async () => {
  validateValue(await schema("platform-profile.schema.json"), JSON.parse(await readFile(resolve(evidenceRoot, "profile.json"), "utf8")));
  const observationSchema = await schema("platform-observation.schema.json");
  const resultSchema = await schema("platform-result.schema.json");
  const observations = await readdir(resolve(evidenceRoot, "observations"));
  const results = await readdir(resolve(evidenceRoot, "results"));
  assert.equal(observations.length, 25);
  assert.equal(results.length, 25);
  for (const name of observations) validateValue(observationSchema, JSON.parse(await readFile(resolve(evidenceRoot, "observations", name), "utf8")));
  for (const name of results) validateValue(resultSchema, JSON.parse(await readFile(resolve(evidenceRoot, "results", name), "utf8")));
});
