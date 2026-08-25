import assert from "node:assert/strict";
import test from "node:test";
import { AndroidReferenceAdapter, ANDROID_PROFILE } from "../dist/index.js";

test("Android profile declares native Activity/View/JNI and Arc capability", () => {
  assert.equal(ANDROID_PROFILE.platformFamily, "ANDROID");
  assert.equal(ANDROID_PROFILE.realization.host, "ACTIVITY_VIEW_JNI");
  assert.equal(ANDROID_PROFILE.capabilities.includes("input.pointer_sample_batch"), true);
  assert.equal(ANDROID_PROFILE.capabilities.includes("arc.preview"), true);
});

test("Android adapter emits facts only and advances surface generation on rebind", () => {
  const observation = new AndroidReferenceAdapter().execute({
    id: "PLAT-SURFACE-REBIND-001",
    steps: [
      { stepId: "s01", kind: "CONTROL", action: { operation: "CREATE_CANVAS" }, completion: {} },
      { stepId: "s02", kind: "FAULT", action: { type: "SURFACE_LOST", mode: "PULSE" }, completion: {} },
      { stepId: "s03", kind: "CONTROL", action: { operation: "PROVIDE_SURFACE_REBIND" }, completion: {} },
    ],
  });
  assert.deepEqual(observation.steps.map((step) => step.event), [
    "CANVAS_CREATED", "SURFACE_UNAVAILABLE", "SURFACE_REBOUND",
  ]);
  assert.equal(observation.realization.surfaceGeneration, "u64:0000000000000002");
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
});
