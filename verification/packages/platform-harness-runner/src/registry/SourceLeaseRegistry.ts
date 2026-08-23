import type { SourceId } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";

export type SourceLeaseState = "OPEN" | "CLOSED";
export class SourceLeaseRegistry {
  private readonly leases = new Map<SourceId, SourceLeaseState>();
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  open(sourceId: SourceId, location: string): void {
    if (this.leases.has(sourceId)) throw this.violations.record({ category: "SOURCE_LEASE_DUPLICATE", location });
    this.leases.set(sourceId, "OPEN"); this.onTransition({ component: "SOURCE", id: sourceId, from: "UNREGISTERED", to: "OPEN", location });
  }
  close(sourceId: SourceId, location: string): void {
    const state = this.leases.get(sourceId);
    if (!state) throw this.violations.record({ category: "SOURCE_LEASE_UNKNOWN", location });
    if (state === "CLOSED") throw this.violations.record({ category: "SOURCE_LEASE_CLOSED", location });
    this.leases.set(sourceId, "CLOSED"); this.onTransition({ component: "SOURCE", id: sourceId, from: "OPEN", to: "CLOSED", location });
  }
  requireOpen(sourceId: SourceId, location: string): void {
    const state = this.leases.get(sourceId);
    if (!state) throw this.violations.record({ category: "SOURCE_LEASE_UNKNOWN", location });
    if (state !== "OPEN") throw this.violations.record({ category: "EVENT_SOURCE_INACTIVE", location });
  }
  unresolved(): SourceId[] { return [...this.leases].filter(([, state]) => state === "OPEN").map(([id]) => id); }
  snapshot(): object[] { return [...this.leases].map(([sourceId, state]) => ({ sourceId, state })); }
}
