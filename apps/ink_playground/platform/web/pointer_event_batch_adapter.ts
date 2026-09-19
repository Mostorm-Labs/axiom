// Thin browser ingress only. Brush, selection, eraser and canonical writes
// remain in the production C++ runtime reached by the WASM bridge.
export type InkPlaygroundWasmBridge = {
  pointerBatch(samples: readonly PointerEvent[]): void;
  cancelPointer(pointerId: number): void;
};

export function attachInkPlaygroundPointerEvents(
  canvas: HTMLCanvasElement,
  bridge: InkPlaygroundWasmBridge,
): () => void {
  canvas.style.touchAction = "none";
  const move = (event: PointerEvent) => bridge.pointerBatch(event.getCoalescedEvents?.() ?? [event]);
  const down = (event: PointerEvent) => bridge.pointerBatch([event]);
  const up = (event: PointerEvent) => bridge.pointerBatch([event]);
  const cancel = (event: PointerEvent) => bridge.cancelPointer(event.pointerId);
  canvas.addEventListener("pointerdown", down);
  canvas.addEventListener("pointermove", move);
  canvas.addEventListener("pointerup", up);
  canvas.addEventListener("pointercancel", cancel);
  return () => {
    canvas.removeEventListener("pointerdown", down);
    canvas.removeEventListener("pointermove", move);
    canvas.removeEventListener("pointerup", up);
    canvas.removeEventListener("pointercancel", cancel);
  };
}
