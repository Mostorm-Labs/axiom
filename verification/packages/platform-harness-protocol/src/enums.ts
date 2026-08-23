export const MESSAGE_TYPES = [
  "HELLO", "OPEN_SESSION", "OPEN_SESSION_RESULT", "ACTION_REQUEST", "ACTION_RECEIPT",
  "ACTION_COMPLETION", "EVENT_DRAFT", "SOURCE_LEASE_OPEN", "SOURCE_LEASE_CLOSE",
  "SOURCE_ATTEMPT", "FAULT_STATUS", "FENCE_STATUS", "CAPTURE_REQUEST", "CAPTURE_RESULT",
  "CLOSE_SESSION", "CLOSE_SESSION_RESULT", "ADAPTER_ERROR",
] as const;

export type MessageType = (typeof MESSAGE_TYPES)[number];
export const MESSAGE_TYPE_SET: ReadonlySet<string> = new Set(MESSAGE_TYPES);

export const BOUNDARY_MODES = ["IN_PROCESS", "OUT_OF_PROCESS"] as const;
export type BoundaryMode = (typeof BOUNDARY_MODES)[number];

export const COMPLETION_MODES = ["DISPATCH_ONLY", "WAIT_FOR_ACTION_COMPLETION"] as const;
export type CompletionMode = (typeof COMPLETION_MODES)[number];

export const RECEIPT_STATUSES = ["DISPATCHED", "COMPLETED_SYNC", "NOT_SUPPORTED", "ADAPTER_ERROR"] as const;
export type ReceiptStatus = (typeof RECEIPT_STATUSES)[number];

export const COMPLETION_OUTCOMES = ["SUCCEEDED", "FAILED", "CANCELLED", "REJECTED"] as const;
export type CompletionOutcome = (typeof COMPLETION_OUTCOMES)[number];
