import type { ActionId, CompletionTokenId } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
export type CompletionState = "REGISTERED" | "COMPLETED";
export interface CompletionRecord { tokenId: CompletionTokenId; actionId: ActionId; state: CompletionState; outcome: string | null; }
export class CompletionRegistry {
  private readonly records = new Map<string, CompletionRecord>();
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  has(tokenId: CompletionTokenId): boolean { return this.records.has(tokenId); }
  register(tokenId: CompletionTokenId, actionId: ActionId, location: string): CompletionRecord {
    if (this.records.has(tokenId)) throw this.violations.record({ category: "DUPLICATE_COMPLETION", location });
    const record = { tokenId, actionId, state: "REGISTERED" as const, outcome: null }; this.records.set(tokenId, record); this.onTransition({ component: "COMPLETION", id: tokenId, from: "UNREGISTERED", to: "REGISTERED", location }); return { ...record };
  }
  complete(tokenId: CompletionTokenId, actionId: ActionId, outcome: string, location: string): CompletionRecord {
    const record = this.records.get(tokenId);
    if (!record) throw this.violations.record({ category: "UNKNOWN_COMPLETION_TOKEN", location });
    if (record.state === "COMPLETED") throw this.violations.record({ category: "DUPLICATE_COMPLETION", location });
    if (record.actionId !== actionId) throw this.violations.record({ category: "COMPLETION_ACTION_MISMATCH", location });
    record.state = "COMPLETED"; record.outcome = outcome; this.onTransition({ component: "COMPLETION", id: tokenId, from: "REGISTERED", to: "COMPLETED", location }); return { ...record };
  }
  unresolved(): CompletionRecord[] { return [...this.records.values()].filter((record) => record.state !== "COMPLETED").map((record) => ({ ...record })); }
  snapshot(): CompletionRecord[] { return [...this.records.values()].map((record) => ({ ...record })); }
}
