import { COMPLETION_MODES, COMPLETION_OUTCOMES, RECEIPT_STATUSES, type CompletionMode, type CompletionOutcome, type ReceiptStatus } from "./enums.js";
import { asActionId, asCompletionTokenId, asTaggedU64, type ActionId, type CompletionTokenId, type TaggedU64 } from "./ids.js";
import { requireRecord, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";

export interface ActionRequestPayload extends JsonObject { actionId: ActionId; stepId: string; commandSeq: TaggedU64; completionMode: CompletionMode; action: string; parameters: JsonObject; }
export interface ActionReceiptPayload extends JsonObject { actionId: ActionId; commandSeq: TaggedU64; receiptStatus: ReceiptStatus; tokenId: CompletionTokenId | null; terminalOutcome: CompletionOutcome | null; }

export function parseActionRequest(value: Record<string, unknown>): ActionRequestPayload {
  requireStrictKeys(value, ["actionId", "stepId", "commandSeq", "completionMode", "action", "parameters"]);
  if (typeof value.completionMode !== "string" || !COMPLETION_MODES.includes(value.completionMode as CompletionMode)) throw new TypeError("payload.completionMode: invalid completion mode");
  return { actionId: asActionId(value.actionId), stepId: requireString(value.stepId, "payload.stepId"), commandSeq: asTaggedU64(value.commandSeq), completionMode: value.completionMode as CompletionMode, action: requireString(value.action, "payload.action"), parameters: requireRecord(value.parameters, "payload.parameters") as JsonObject };
}
export function parseActionReceipt(value: Record<string, unknown>): ActionReceiptPayload {
  requireStrictKeys(value, ["actionId", "commandSeq", "receiptStatus"], ["tokenId", "terminalOutcome"]);
  if (typeof value.receiptStatus !== "string" || !RECEIPT_STATUSES.includes(value.receiptStatus as ReceiptStatus)) throw new TypeError("payload.receiptStatus: invalid receipt status");
  return {
    actionId: asActionId(value.actionId), commandSeq: asTaggedU64(value.commandSeq), receiptStatus: value.receiptStatus as ReceiptStatus,
    tokenId: value.tokenId === undefined || value.tokenId === null ? null : asCompletionTokenId(value.tokenId),
    terminalOutcome: value.terminalOutcome === undefined || value.terminalOutcome === null ? null : (() => {
      const outcome = requireString(value.terminalOutcome, "payload.terminalOutcome");
      if (!COMPLETION_OUTCOMES.includes(outcome as CompletionOutcome)) throw new TypeError("payload.terminalOutcome: invalid outcome");
      return outcome as CompletionOutcome;
    })(),
  };
}
