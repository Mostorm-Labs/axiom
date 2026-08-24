export type WebMetrics = {
  logicalWidth: number;
  logicalHeight: number;
  physicalWidth: number;
  physicalHeight: number;
  deviceScale: number;
  visible: boolean;
  occluded: boolean;
};

export type PointerBatch = { correlationId: string; samples: ReadonlyArray<Record<string, unknown>> };

/**
 * Thin browser seam used by the verification adapter. It owns DOM canvas and
 * browser facts; it deliberately has no scenario expected/comparator access.
 */
export class WebCanvasHost {
  readonly canvas: { width: number; height: number };
  private surfaceGeneration = 1;
  private metricsGeneration = 1;
  private visible = true;
  private readonly observers = new Set<(event: Record<string, unknown>) => void>();

  constructor(canvas: { width: number; height: number }) { this.canvas = canvas; }
  addObserver(observer: (event: Record<string, unknown>) => void): () => void { this.observers.add(observer); return () => this.observers.delete(observer); }
  resize(metrics: WebMetrics): void {
    this.canvas.width = Math.max(0, Math.floor(metrics.physicalWidth));
    this.canvas.height = Math.max(0, Math.floor(metrics.physicalHeight));
    this.metricsGeneration += 1;
    this.emit({ kind: "METRICS_CHANGED", metricsGeneration: this.metricsGeneration, metrics });
  }
  setVisibility(visible: boolean, occluded: boolean): void {
    this.visible = visible;
    this.emit({ kind: "VISIBILITY_CHANGED", visible, occluded });
  }
  bindContext(context: WebGL2RenderingContext): void { this.emit({ kind: "SURFACE_BOUND", surfaceGeneration: this.surfaceGeneration, context }); }
  loseSurface(): void { this.surfaceGeneration += 1; this.emit({ kind: "SURFACE_UNAVAILABLE", surfaceGeneration: this.surfaceGeneration }); }
  rebindSurface(context: WebGL2RenderingContext): void { this.emit({ kind: "SURFACE_REBOUND", surfaceGeneration: this.surfaceGeneration, context }); }
  deliverPointerBatch(batch: PointerBatch, wasmDeliver: (batch: PointerBatch) => void): void {
    wasmDeliver({ correlationId: batch.correlationId, samples: batch.samples.slice() });
    this.emit({ kind: "INPUT_BATCH_DELIVERED", correlationId: batch.correlationId, count: batch.samples.length });
  }
  isVisible(): boolean { return this.visible; }
  private emit(event: Record<string, unknown>): void { for (const observer of this.observers) observer(Object.freeze({ ...event })); }
}

export function createWebCanvasHost(documentLike: { getElementById(id: string): unknown }, canvasId: string): WebCanvasHost {
  const canvas = documentLike.getElementById(canvasId);
  if (!canvas || typeof canvas !== "object" || !("width" in canvas) || !("height" in canvas)) throw new Error(`canvas host not found: ${canvasId}`);
  return new WebCanvasHost(canvas as { width: number; height: number });
}
