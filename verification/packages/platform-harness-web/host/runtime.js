const canvas = document.getElementById("axiom-verification-canvas");
let wasmFacade = null;
let surfaceGeneration = 1;
let metricsGeneration = 1;
const facts = [];

function record(kind, details = {}) {
  facts.push(Object.freeze({ kind, ...details }));
}

function normalizePointer(event, sample, index) {
  return {
    x: sample.clientX,
    y: sample.clientY,
    pressure: sample.pressure,
    tiltX: sample.tiltX,
    tiltY: sample.tiltY,
    timestamp: sample.timeStamp,
    device: event.pointerType,
    predicted: false,
    historyIndex: index,
  };
}

canvas.addEventListener("pointermove", (event) => {
  if (!wasmFacade) return;
  const coalesced = typeof event.getCoalescedEvents === "function" ? event.getCoalescedEvents() : [];
  const history = coalesced.length ? coalesced : [event];
  const correlationId = `pointer:${event.pointerId}:${event.timeStamp}`;
  const batch = { correlationId, samples: history.map((sample, index) => normalizePointer(event, sample, index)) };
  wasmFacade.deliverPointerSampleBatch(batch);
  record("INPUT_BATCH_DELIVERED", { correlationId, count: batch.samples.length });
});

globalThis.__axiomVerificationHost = Object.freeze({
  setWasmFacade(facade) {
    if (!facade || typeof facade.deliverPointerSampleBatch !== "function") throw new TypeError("invalid WASM facade");
    wasmFacade = facade;
    record("WASM_FACADE_BOUND");
  },
  updateMetrics(metrics) {
    canvas.width = Math.max(0, Math.floor(metrics.physicalWidth));
    canvas.height = Math.max(0, Math.floor(metrics.physicalHeight));
    metricsGeneration += 1;
    record("METRICS_CHANGED", { metricsGeneration, metrics });
  },
  loseSurface() {
    surfaceGeneration += 1;
    record("SURFACE_UNAVAILABLE", { surfaceGeneration });
  },
  rebindSurface() { record("SURFACE_REBOUND", { surfaceGeneration }); },
  facts() { return facts.map((fact) => ({ ...fact })); },
  profile: {
    platformFamily: "WEB",
    platformVariant: "browser-wasm-webgl2",
    arc: "DISABLED_BY_CONTRACT",
  },
});
record("HOST_READY", { surfaceGeneration, metricsGeneration });
