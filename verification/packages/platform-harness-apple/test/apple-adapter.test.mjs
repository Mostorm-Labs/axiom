import assert from "node:assert/strict";
import test from "node:test";
import {
  IOS_PROFILE,
  IPADOS_PROFILE,
  AppleReferenceAdapter,
  normalizeApplePointerHistory,
} from "../dist/index.js";

test("Apple profiles keep iPhone and iPadOS identities separate", () => {
  assert.equal(IOS_PROFILE.platformFamily, "APPLE");
  assert.equal(IPADOS_PROFILE.platformFamily, "APPLE");
  assert.notEqual(IOS_PROFILE.profileId, IPADOS_PROFILE.profileId);
  assert.equal(IOS_PROFILE.realization.deviceClass, "IPHONE");
  assert.equal(IPADOS_PROFILE.realization.deviceClass, "IPAD");
  assert.equal(IOS_PROFILE.realization.backend, "METAL");
  assert.equal(IOS_PROFILE.capabilities.includes("input.pointer_sample_batch"), true);
});

test("Apple adapter preserves lifecycle facts and generation semantics", () => {
  const observation = new AppleReferenceAdapter(IOS_PROFILE).execute({
    id: "PLAT-RECOVERY-REPEATED-001",
    steps: [
      { stepId: "s01", kind: "CONTROL", action: { operation: "CREATE_CANVAS" }, completion: {} },
      { stepId: "s02", kind: "FAULT", action: { type: "SURFACE_LOST", mode: "PULSE" }, completion: {} },
      { stepId: "s03", kind: "CONTROL", action: { operation: "PROVIDE_SURFACE_REBIND" }, completion: {} },
      { stepId: "s04", kind: "FAULT", action: { type: "DEVICE_LOST", mode: "PULSE" }, completion: {} },
      { stepId: "s05", kind: "FAULT", action: { type: "DEVICE_LOST", mode: "CLEAR" }, completion: {} },
      { stepId: "s06", kind: "INPUT", action: { operation: "DELIVER_POINTER_SAMPLE_BATCH", correlationId: "pencil-01" }, completion: {} },
    ],
  });
  assert.equal(observation.profileId, IOS_PROFILE.profileId);
  assert.equal(observation.realization.deviceClass, "IPHONE");
  assert.equal(observation.realization.surfaceGeneration, "u64:0000000000000002");
  assert.deepEqual(observation.steps.map((step) => step.event), [
    "CANVAS_CREATED", "SURFACE_UNAVAILABLE", "SURFACE_REBOUND", "DEVICE_LOST",
    "DEVICE_RECOVERED", "INPUT_BATCH_DELIVERED",
  ]);
  assert.equal(observation.steps.at(-1).correlationId, "pencil-01");
  assert.equal(Object.hasOwn(observation, "expected"), false);
  assert.equal(Object.hasOwn(observation, "result"), false);
});

test("Apple pointer normalization keeps coalesced samples in one batch", () => {
  const batch = normalizeApplePointerHistory("pencil-01", [
    { x: 1, y: 2, pressure: 0.2, tiltX: 0, tiltY: 1, timestampNs: 10, toolType: "PENCIL" },
  ], { x: 3, y: 4, pressure: 0.8, tiltX: 2, tiltY: 3, timestampNs: 20, toolType: "PENCIL" });
  assert.equal(batch.correlationId, "pencil-01");
  assert.equal(batch.samples.length, 2);
  assert.equal(batch.samples.at(-1).pressure, 0.8);
});
