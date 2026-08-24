import type { SessionId, TaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
export type SessionState = "NEW" | "OPEN" | "CLOSING" | "CLOSED";
const epochValue = (epoch: TaggedU64): bigint => BigInt(`0x${epoch.slice(4)}`);
export class SessionRegistry {
  private readonly lastEpochByAdapter = new Map<string, bigint>();
  private active: { sessionId: SessionId; sessionEpoch: TaggedU64; adapterInstanceId: string; state: SessionState } | null = null;
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  open(adapterInstanceId: string, sessionId: SessionId, sessionEpoch: TaggedU64, location: string): void {
    const epoch = epochValue(sessionEpoch); const last = this.lastEpochByAdapter.get(adapterInstanceId) ?? 0n;
    if (epoch === 0n) throw this.violations.record({ category: "SESSION_EPOCH_INVALID", location });
    if (epoch <= last) throw this.violations.record({ category: "SESSION_EPOCH_REUSED", location });
    if (this.active && this.active.state !== "CLOSED") throw this.violations.record({ category: "SESSION_CLOSED", location, diagnostic: "another session is active" });
    this.active = { sessionId, sessionEpoch, adapterInstanceId, state: "OPEN" }; this.lastEpochByAdapter.set(adapterInstanceId, epoch);
    this.onTransition({ component: "SESSION", id: sessionId, from: "NEW", to: "OPEN", location });
  }
  requireOpen(sessionId: SessionId, sessionEpoch: TaggedU64, location: string): void {
    const active = this.active;
    if (!active || active.state !== "OPEN") throw this.violations.record({ category: "SESSION_CLOSED", location });
    if (active.sessionId !== sessionId || active.sessionEpoch !== sessionEpoch) throw this.violations.record({ category: "STALE_SESSION_EPOCH", location });
  }
  requireCurrentOpen(location: string): { sessionId: SessionId; sessionEpoch: TaggedU64; adapterInstanceId: string } {
    const active = this.active;
    if (!active || active.state !== "OPEN") throw this.violations.record({ category: "SESSION_CLOSED", location });
    return { sessionId: active.sessionId, sessionEpoch: active.sessionEpoch, adapterInstanceId: active.adapterInstanceId };
  }
  close(sessionId: SessionId, sessionEpoch: TaggedU64, location: string): void {
    this.requireOpen(sessionId, sessionEpoch, location); this.active!.state = "CLOSING";
    this.onTransition({ component: "SESSION", id: sessionId, from: "OPEN", to: "CLOSING", location });
    this.active!.state = "CLOSED"; this.onTransition({ component: "SESSION", id: sessionId, from: "CLOSING", to: "CLOSED", location });
  }
  transportLost(location: string): void {
    if (!this.active || this.active.state !== "OPEN") return;
    this.active.state = "CLOSED"; this.onTransition({ component: "SESSION", id: this.active.sessionId, from: "OPEN", to: "CLOSED", reason: "TRANSPORT_LOST", location });
  }
  state(): SessionState { return this.active?.state ?? "NEW"; }
  snapshot(): object { return this.active ? { ...this.active } : { state: "NEW" }; }
}
