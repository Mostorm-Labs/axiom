export const WEB_PROFILE = Object.freeze({
  format: "axiom-platform-profile-v1",
  formatVersion: 1,
  profileId: "web-reference-v0-1",
  platformFamily: "WEB",
  platformVariant: "browser-wasm-webgl2",
  capabilities: [
    "semantic.projection.capture", "surface.generation", "metrics.generation",
    "surface.loss.inject", "device.loss.inject", "bridge.public_facade", "bridge.data_bridge",
    "bridge.callback_trace", "input.pointer_sample_batch", "platform.state.capture",
    "harness.completion_tokens", "harness.source_lease_registry", "harness.late_event_fence",
    "harness.source_attempt_trace",
  ],
  realization: { host: "DOM_CANVAS", runtime: "WASM", renderer: "WEBGL2", arc: "DISABLED_BY_CONTRACT" },
});
export { WebCanvasHost, createWebCanvasHost } from "./browser_host.js";
export type { WebMetrics, PointerBatch } from "./browser_host.js";

type Step = { stepId: string; kind: string; action: Record<string, unknown>; completion: Record<string, unknown> };
type Scenario = { id: string; steps: Step[] };
type ObservationStep = { stepId: string; kind: string; event: string; correlationId?: string; details?: Record<string, unknown> };
export type WebObservation = {
  format: "axiom-platform-observation-v1"; formatVersion: 1; scenarioId: string; profileId: string;
  platformFamily: "WEB"; execution: { adapter: string; boundary: "IN_PROCESS"; deterministic: true };
  capabilities: string[]; terminal: { state: "OBSERVED" }; steps: ObservationStep[]; artifacts: Record<string, string>;
  semanticCheckpoints: object[]; stateCheckpoints: object[]; targetBindings: object[]; realization: Record<string, unknown>; diagnostics: string[];
};

const eventFor = (step: Step): string | null => {
  const op = String(step.action.operation ?? "");
  if (op === "CREATE_CANVAS") return "CANVAS_CREATED";
  if (op === "ATTACH_HOST") return "HOST_ATTACHED";
  if (op === "DETACH_HOST") return "HOST_DETACHED";
  if (op === "ATTACH_DOCUMENT") return "DOCUMENT_ATTACHED";
  if (op === "DETACH_DOCUMENT") return "DOCUMENT_DETACHED";
  if (op === "DESTROY_CANVAS") return "CANVAS_DESTROYED";
  if (op === "SUSPEND_CANVAS") return "CANVAS_SUSPENDED";
  if (op === "RESUME_CANVAS") return "CANVAS_RUNNING";
  if (op === "PROVIDE_SURFACE_REBIND") return "SURFACE_REBOUND";
  if (op === "DRAIN_SOURCES") return "SOURCES_DRAINED";
  if (op === "ARM_LATE_EVENT_FENCE") return "LATE_EVENT_FENCE_ARMED";
  if (op === "REPLAY_CANONICAL_FIXTURE" || op === "RESTORE_CANONICAL_FIXTURE" || op === "APPLY_CANONICAL_FIXTURE") return "CANONICAL_FIXTURE_REPLAYED";
  if (op === "DELIVER_POINTER_SAMPLE_BATCH") return "INPUT_BATCH_DELIVERED";
  if (op === "SUBSCRIBE_LOCAL_CANONICAL") return "LOCAL_CANONICAL_SUBSCRIBED";
  if (op === "APPLY_EXTERNAL") return "DATA_BRIDGE_APPLY_COMPLETED";
  if (op === "BACKGROUND") return "APP_BACKGROUND";
  if (op === "FOREGROUND") return "APP_FOREGROUND";
  if (step.kind === "METRICS_UPDATE") return step.action.visible === undefined && step.action.logicalWidth === undefined ? "VISIBILITY_CHANGED" : "METRICS_CHANGED";
  if (step.kind === "FAULT") {
    if (step.action.mode === "ACTIVATE" && step.action.type === "PRESENT_COMPLETION_HELD") return "PRESENT_COMPLETION_HELD";
    if (step.action.mode === "CLEAR" && step.action.type === "PRESENT_COMPLETION_HELD") return "PRESENT_COMPLETION_RELEASED";
    if (step.action.type === "SURFACE_LOST") return "SURFACE_UNAVAILABLE";
    if (step.action.type === "DEVICE_LOST") return step.action.mode === "CLEAR" ? "DEVICE_RECOVERED" : "DEVICE_LOST";
    if (step.action.type === "STALE_SURFACE_GENERATION_PUBLISH") return "STALE_GENERATION_REJECTED";
  }
  return null;
};

export class WebReferenceAdapter {
  readonly profile = WEB_PROFILE;
  private surfaceGeneration = 1;
  execute(scenario: Scenario): WebObservation {
    const steps: ObservationStep[] = [];
    for (const step of scenario.steps) {
      const encoded = JSON.stringify(step.action);
      if (/\b(?:sleep|delayMs|waitMs)\b/i.test(encoded)) throw new Error("correctness sleep is forbidden");
      if (step.kind === "FAULT" && step.action.type === "SURFACE_LOST") this.surfaceGeneration += 1;
      const event = eventFor(step);
      if (!event) continue;
      const observed: ObservationStep = { stepId: step.stepId, kind: step.kind, event };
      if (typeof step.action.correlationId === "string") observed.correlationId = step.action.correlationId;
      if (step.kind === "METRICS_UPDATE") observed.details = { logicalWidth: step.action.logicalWidth, logicalHeight: step.action.logicalHeight, deviceScale: step.action.deviceScale, visible: step.action.visible, occluded: step.action.occluded };
      steps.push(observed);
    }
    return {
      format: "axiom-platform-observation-v1", formatVersion: 1, scenarioId: scenario.id, profileId: this.profile.profileId,
      platformFamily: "WEB", execution: { adapter: "web-reference-v0.1", boundary: "IN_PROCESS", deterministic: true },
      capabilities: [...this.profile.capabilities], terminal: { state: "OBSERVED" }, steps, artifacts: {}, semanticCheckpoints: [], stateCheckpoints: [], targetBindings: [],
      realization: { ...this.profile.realization, surfaceGeneration: `u64:${this.surfaceGeneration.toString(16).padStart(16, "0")}`, inputPath: "PointerEvents.coalescedEvents→WASM.batch" }, diagnostics: [],
    };
  }
}
