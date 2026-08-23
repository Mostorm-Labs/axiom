import type { ActionId, ActionRequestPayload, ActionReceiptPayload, CompletionMode, CompletionTokenId, TaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
export type ActionState = "CREATED" | "REQUEST_SENT" | "RECEIPT_ACCEPTED" | "COMPLETION_PENDING" | "TERMINAL";
export interface ActionRecord { actionId: ActionId; commandSeq: TaggedU64; completionMode: CompletionMode; state: ActionState; tokenId: CompletionTokenId | null; outcome: string | null; }
export class ActionRegistry {
  private readonly records = new Map<string, ActionRecord>();
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  create(request: ActionRequestPayload, location: string): ActionRecord {
    if (this.records.has(request.actionId)) throw this.violations.record({ category: "ACTION_ID_DUPLICATE", location });
    const record: ActionRecord = { actionId: request.actionId, commandSeq: request.commandSeq, completionMode: request.completionMode, state: "REQUEST_SENT", tokenId: null, outcome: null };
    this.records.set(request.actionId, record); this.onTransition({ component: "ACTION", id: request.actionId, from: "CREATED", to: "REQUEST_SENT", location }); return { ...record };
  }
  receipt(receipt: ActionReceiptPayload, location: string): ActionRecord {
    const record = this.records.get(receipt.actionId); if (!record) throw this.violations.record({ category: "ADAPTER_ERROR", location, diagnostic: "receipt for unknown action" });
    if (record.commandSeq !== receipt.commandSeq) throw this.violations.record({ category: "COMMAND_SEQ_MISMATCH", location });
    if (receipt.receiptStatus === "DISPATCHED" && record.completionMode === "WAIT_FOR_ACTION_COMPLETION" && !receipt.tokenId) {
      throw this.violations.record({ category: "MISSING_COMPLETION_TOKEN", location });
    }
    if (receipt.receiptStatus === "COMPLETED_SYNC" && (!receipt.terminalOutcome || receipt.tokenId)) {
      throw this.violations.record({ category: "INVALID_SYNC_TERMINAL", location });
    }
    record.state = "RECEIPT_ACCEPTED"; this.onTransition({ component: "ACTION", id: record.actionId, from: "REQUEST_SENT", to: "RECEIPT_ACCEPTED", location });
    if (receipt.receiptStatus === "DISPATCHED") {
      if (record.completionMode === "WAIT_FOR_ACTION_COMPLETION") {
        record.tokenId = receipt.tokenId!; record.state = "COMPLETION_PENDING"; this.onTransition({ component: "ACTION", id: record.actionId, from: "RECEIPT_ACCEPTED", to: "COMPLETION_PENDING", location });
      } else { record.outcome = "DISPATCHED"; record.state = "TERMINAL"; this.onTransition({ component: "ACTION", id: record.actionId, from: "RECEIPT_ACCEPTED", to: "TERMINAL", location }); }
    } else if (receipt.receiptStatus === "COMPLETED_SYNC") {
      record.outcome = receipt.terminalOutcome!; record.state = "TERMINAL"; this.onTransition({ component: "ACTION", id: record.actionId, from: "RECEIPT_ACCEPTED", to: "TERMINAL", location });
    } else if (receipt.receiptStatus === "NOT_SUPPORTED" || receipt.receiptStatus === "ADAPTER_ERROR") {
      record.outcome = receipt.receiptStatus; record.state = "TERMINAL"; this.onTransition({ component: "ACTION", id: record.actionId, from: "RECEIPT_ACCEPTED", to: "TERMINAL", location });
    }
    return { ...record };
  }
  complete(actionId: ActionId, tokenId: CompletionTokenId, outcome: string, location: string): ActionRecord {
    const record = this.records.get(actionId); if (!record) throw this.violations.record({ category: "ADAPTER_ERROR", location, diagnostic: "completion for unknown action" });
    if (record.tokenId !== tokenId) throw this.violations.record({ category: "COMPLETION_ACTION_MISMATCH", location });
    const from = record.state; record.outcome = outcome; record.state = "TERMINAL"; this.onTransition({ component: "ACTION", id: record.actionId, from, to: "TERMINAL", location }); return { ...record };
  }
  unresolved(): ActionRecord[] { return [...this.records.values()].filter((record) => record.state !== "TERMINAL").map((record) => ({ ...record })); }
  snapshot(): ActionRecord[] { return [...this.records.values()].map((record) => ({ ...record })); }
}
