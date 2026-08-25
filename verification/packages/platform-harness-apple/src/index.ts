export const IOS_PROFILE = Object.freeze({
  format: "axiom-platform-profile-v1", formatVersion: 1,
  profileId: "ios-rn-objcxx-reference-v0-1", platformFamily: "APPLE",
  platformVariant: "ios-rn-objcxx-metal", capabilities: [
    "semantic.projection.capture", "surface.generation", "metrics.generation",
    "surface.loss.inject", "device.loss.inject", "presentation.feedback",
    "bridge.public_facade", "bridge.data_bridge", "bridge.callback_trace",
    "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
    "platform.state.capture", "surface.ownership.capture", "fault.stale_generation.inject",
    "fault.present_completion_hold", "harness.completion_tokens", "harness.source_lease_registry",
    "harness.late_event_fence", "harness.source_attempt_trace",
  ], realization: {
    host: "RN_FABRIC_OBJCXX", surface: "UIVIEW_CAMETAL_LAYER", backend: "METAL",
    deviceClass: "IPHONE", arc: "ENABLED", inputPath: "UITouch.coalescedTouches→ObjC++→PointerSampleBatch",
  },
});

export const IPADOS_PROFILE = Object.freeze({
  ...IOS_PROFILE,
  profileId: "ipados-rn-objcxx-reference-v0-1",
  platformVariant: "ipados-rn-objcxx-metal",
  realization: { ...IOS_PROFILE.realization, deviceClass: "IPAD" },
});

type Step = { stepId: string; kind: string; action: Record<string, unknown>; completion: Record<string, unknown> };
type Scenario = { id: string; steps: Step[] };
type ObservationStep = { stepId: string; kind: string; event: string; correlationId?: string; details?: Record<string, unknown> };
export type AppleMotionSample = { x: number; y: number; pressure: number; tiltX: number; tiltY: number; timestampNs: number; toolType: "FINGER" | "PENCIL" | "STYLUS" | "UNKNOWN" };
export type ApplePointerBatch = { correlationId: string; samples: AppleMotionSample[] };
export type AppleProfile = { format: string; formatVersion: number; profileId: string; platformFamily: "APPLE"; platformVariant: string; capabilities: string[]; realization: Record<string, unknown> };
export type AppleObservation = {
  format: "axiom-platform-observation-v1"; formatVersion: 1; scenarioId: string; profileId: string; platformFamily: "APPLE";
  execution: { adapter: string; boundary: "OUT_OF_PROCESS"; deterministic: true; host: string };
  capabilities: string[]; terminal: { state: "OBSERVED" }; steps: ObservationStep[]; artifacts: Record<string, string>;
  semanticCheckpoints: object[]; stateCheckpoints: object[]; targetBindings: object[]; realization: Record<string, unknown>; diagnostics: string[];
};

export function normalizeApplePointerHistory(correlationId: string, historicalSamples: AppleMotionSample[], currentSample: AppleMotionSample): ApplePointerBatch {
  return { correlationId, samples: [...historicalSamples, currentSample] };
}

function eventFor(step: Step): string | null {
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
}

export class AppleReferenceAdapter {
  readonly profile: AppleProfile;
  private surfaceGeneration = 1;
  constructor(profile: AppleProfile = IOS_PROFILE) { this.profile = profile; }
  execute(scenario: Scenario): AppleObservation {
    const steps: ObservationStep[] = [];
    let surfaceLost = false;
    for (const step of scenario.steps) {
      if (/\b(?:sleep|delayMs|waitMs)\b/i.test(JSON.stringify(step.action))) throw new Error("correctness sleep is forbidden");
      if (step.kind === "FAULT" && step.action.type === "SURFACE_LOST") { surfaceLost = true; }
      if (String(step.action.operation ?? "") === "PROVIDE_SURFACE_REBIND" && surfaceLost) { this.surfaceGeneration += 1; surfaceLost = false; }
      const event = eventFor(step); if (!event) continue;
      const observed: ObservationStep = { stepId: step.stepId, kind: step.kind, event };
      if (typeof step.action.correlationId === "string") observed.correlationId = step.action.correlationId;
      if (step.kind === "METRICS_UPDATE") observed.details = { logicalWidth: step.action.logicalWidth, logicalHeight: step.action.logicalHeight, deviceScale: step.action.deviceScale, visible: step.action.visible, occluded: step.action.occluded, orientation: step.action.orientation };
      steps.push(observed);
    }
    const requiresArc = scenario.steps.some((step) => step.action.type === "ARC_PREVIEW_LOST" || step.action.operation === "CAPTURE_TARGET_BINDINGS");
    return {
      format: "axiom-platform-observation-v1", formatVersion: 1, scenarioId: scenario.id, profileId: this.profile.profileId, platformFamily: "APPLE",
      execution: { adapter: "apple-xctest-reference-v0.1", boundary: "OUT_OF_PROCESS", deterministic: true, host: "XCTest-style" },
      capabilities: [...this.profile.capabilities], terminal: { state: "OBSERVED" }, steps, artifacts: {}, semanticCheckpoints: [], stateCheckpoints: [],
      targetBindings: requiresArc ? [{ canonicalOwner: "AXIOM", previewOwner: "ARC", distinct: true }] : [],
      realization: { ...this.profile.realization, surfaceGeneration: `u64:${this.surfaceGeneration.toString(16).padStart(16, "0")}`, surfacePrimitive: "CAMETAL_LAYER", backend: "METAL" }, diagnostics: [],
    };
  }
}
