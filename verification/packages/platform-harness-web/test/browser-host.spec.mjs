import { test, expect } from "@playwright/test";

test("browser host maps DOM facts and pointer input directly to the WASM facade", async ({ page }) => {
  await page.goto("/");
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
