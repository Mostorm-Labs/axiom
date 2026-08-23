import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";

export class FaultRegistry {
  private readonly faults = new Map<string, boolean>();
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  activate(fault: string, location: string): void {
    if (this.faults.has(fault)) throw this.violations.record({ category: "FAULT_HANDLE_DUPLICATE", location });
    this.faults.set(fault, true); this.onTransition({ component: "FAULT", id: fault, from: "UNREGISTERED", to: "ACTIVE", location });
  }
  clear(fault: string, location: string): void {
    const active = this.faults.get(fault);
    if (active === undefined) throw this.violations.record({ category: "FAULT_HANDLE_UNKNOWN", location });
    if (!active) throw this.violations.record({ category: "FAULT_HANDLE_STATE_INVALID", location });
    this.faults.set(fault, false); this.onTransition({ component: "FAULT", id: fault, from: "ACTIVE", to: "CLEARED", location });
  }
  unresolved(): string[] { return [...this.faults].filter(([, active]) => active).map(([id]) => id); }
  snapshot(): object[] { return [...this.faults].map(([fault, active]) => ({ fault, state: active ? "ACTIVE" : "CLEARED" })); }
}
