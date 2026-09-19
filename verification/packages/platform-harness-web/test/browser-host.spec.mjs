import { test, expect } from "@playwright/test";

test("browser host maps DOM facts and pointer input directly to the WASM facade", async ({ page }) => {
  await page.goto("/verification/packages/platform-harness-web/host/");
  const observed = await page.evaluate(() => {
    const batches = [];
    const host = globalThis.__axiomVerificationHost;
    host.setWasmFacade({ deliverPointerSampleBatch: (batch) => batches.push(batch) });
    host.updateMetrics({ logicalWidth: 100, logicalHeight: 50, physicalWidth: 200, physicalHeight: 100, deviceScale: 2, visible: true, occluded: false });
    host.loseSurface();
    host.rebindSurface();
    const canvas = document.getElementById("axiom-verification-canvas");
    canvas.dispatchEvent(new PointerEvent("pointermove", { pointerId: 7, pointerType: "pen", clientX: 12, clientY: 18, pressure: 0.5 }));
    return { facts: host.facts(), batches, width: canvas.width, height: canvas.height, profile: host.profile };
  });
  expect(observed.profile).toEqual({ platformFamily: "WEB", platformVariant: "browser-wasm-webgl2", arc: "DISABLED_BY_CONTRACT" });
  expect(observed.width).toBe(200);
  expect(observed.height).toBe(100);
  expect(observed.facts.map((fact) => fact.kind)).toEqual(["HOST_READY", "WASM_FACADE_BOUND", "METRICS_CHANGED", "SURFACE_UNAVAILABLE", "SURFACE_REBOUND", "INPUT_BATCH_DELIVERED"]);
  expect(observed.batches).toHaveLength(1);
  expect(observed.batches[0].samples[0]).toMatchObject({ x: 12, y: 18, pressure: 0.5, device: "pen" });
});

test("production WASM host binds WebGL2 and preserves generation-bound browser facts", async ({ page }) => {
  test.skip(!process.env.AXIOM_G3_WEB_BUILD_DIR, "dedicated G3-10 Web build is not present");
  await page.goto("/out/g3-web-release/apps/axiom_canvas_demo/index.html");
  await page.evaluate(() => globalThis.axiomG310Ready);
  const observed = await page.evaluate(async () => {
    const host = await globalThis.axiomG310Ready;
    host.resize(200, 100, 2);
    host.loseSurface();
    host.resize(240, 120, 2);
    const canvas = document.getElementById("axiom-canvas");
    canvas.dispatchEvent(new PointerEvent("pointermove", {
      pointerId: 9, pointerType: "pen", clientX: 12, clientY: 18, pressure: 0.5,
    }));
    host.present();
    return host.observation();
  });
  expect(observed.width).toBe(480);
  expect(observed.height).toBe(240);
  expect(observed.surfaceGeneration).toBe(3);
  expect(observed.metricsGeneration).toBe(3);
  expect(observed.pointerSamples).toBe(1);
  expect(observed.presentedFrames).toBe(1);
  expect(observed.facts.map((fact) => fact.kind)).toEqual([
    "WEBGL2_BOUND", "HOST_READY", "METRICS_CHANGED", "SURFACE_UNAVAILABLE",
    "METRICS_CHANGED", "POINTER_BATCH_FORWARDED", "PRESENTED",
  ]);
});
