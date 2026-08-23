import type { TaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
const tagged = (value: bigint): TaggedU64 => `u64:${value.toString(16).padStart(16, "0")}` as TaggedU64;
export class CommandSequencer {
  private nextValue = 1n;
  constructor(private readonly violations: ProtocolViolationCollector) {}
  next(): TaggedU64 { const value = tagged(this.nextValue); this.nextValue += 1n; return value; }
  assert(expected: TaggedU64, actual: TaggedU64, location: string): void {
    if (expected !== actual) throw this.violations.record({ category: "COMMAND_SEQ_MISMATCH", location });
  }
}
