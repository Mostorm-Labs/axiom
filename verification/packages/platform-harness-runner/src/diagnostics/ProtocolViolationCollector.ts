export const CORE_A_ERROR_CATEGORIES = [
  "INVALID_ENVELOPE", "UNKNOWN_MESSAGE_TYPE", "PROTOCOL_VERSION_MISMATCH", "SESSION_EPOCH_INVALID",
  "SESSION_EPOCH_REUSED", "STALE_SESSION_EPOCH", "SESSION_CLOSED", "ACTION_ID_DUPLICATE",
  "COMMAND_SEQ_MISMATCH", "MISSING_COMPLETION_TOKEN", "INVALID_SYNC_TERMINAL", "ADAPTER_ERROR",
  "UNKNOWN_COMPLETION_TOKEN", "DUPLICATE_COMPLETION", "COMPLETION_ACTION_MISMATCH", "UNRESOLVED_REGISTRY",
  "EVENT_SOURCE_INACTIVE", "SOURCE_LEASE_DUPLICATE", "SOURCE_LEASE_UNKNOWN", "SOURCE_LEASE_CLOSED",
  "FAULT_HANDLE_DUPLICATE", "FAULT_HANDLE_UNKNOWN", "FAULT_HANDLE_STATE_INVALID", "LATE_EVENT_REJECTED",
  "FENCE_GENERATION_INVALID",
] as const;
export type CoreAProtocolErrorCategory = (typeof CORE_A_ERROR_CATEGORIES)[number];

export interface ProtocolDivergence { category: CoreAProtocolErrorCategory; location: string; diagnostic?: string; }
export class ProtocolViolation extends Error {
  constructor(public readonly divergence: ProtocolDivergence) { super(`${divergence.category} at ${divergence.location}`); this.name = "ProtocolViolation"; }
}
export class ProtocolViolationCollector {
  private firstValue: ProtocolDivergence | null = null;
  private readonly diagnosticValues: ProtocolDivergence[] = [];
  record(divergence: ProtocolDivergence): ProtocolViolation {
    if (this.firstValue === null) this.firstValue = Object.freeze({ ...divergence });
    this.diagnosticValues.push(Object.freeze({ ...divergence }));
    return new ProtocolViolation(this.firstValue);
  }
  first(): ProtocolDivergence | null { return this.firstValue && { ...this.firstValue }; }
  diagnostics(): ProtocolDivergence[] { return this.diagnosticValues.map((item) => ({ ...item })); }
}
