import type { TaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
const numeric = (value: TaggedU64): bigint => BigInt(`0x${value.slice(4)}`);
export class LateEventFence {
  private generation: TaggedU64 = "u64:0000000000000000" as TaggedU64;
  constructor(private readonly violations: ProtocolViolationCollector, private readonly onTransition: (value: object) => void = () => {}) {}
  advance(generation: TaggedU64, location: string): void {
    if (numeric(generation) <= numeric(this.generation)) throw this.violations.record({ category: "FENCE_GENERATION_INVALID", location, diagnostic: "generation must advance" });
    const previous = this.generation; this.generation = generation; this.onTransition({ component: "FENCE", id: "generation", from: previous, to: generation, location });
  }
  accept(generation: TaggedU64, location: string): void {
    if (generation !== this.generation) throw this.violations.record({ category: "LATE_EVENT_REJECTED", location, diagnostic: `current generation ${this.generation}` });
  }
  snapshot(): object { return { generation: this.generation }; }
}
