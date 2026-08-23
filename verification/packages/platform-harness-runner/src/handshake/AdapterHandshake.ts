import type { BoundaryMode, HelloPayload } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";

export class AdapterHandshake {
  private accepted: { adapterInstanceId: string; boundaryMode: BoundaryMode } | null = null;
  constructor(private readonly violations: ProtocolViolationCollector, private readonly requiredVersion = 1) {}
  accept(hello: HelloPayload, actualBoundaryMode: BoundaryMode, location: string): void {
    if (!hello.supportedProtocolVersions.includes(this.requiredVersion) || hello.boundaryMode !== actualBoundaryMode) {
      throw this.violations.record({ category: "PROTOCOL_VERSION_MISMATCH", location });
    }
    this.accepted = { adapterInstanceId: hello.adapterInstanceId, boundaryMode: hello.boundaryMode };
  }
  requireAccepted(location: string): { adapterInstanceId: string; boundaryMode: BoundaryMode } {
    if (!this.accepted) throw this.violations.record({ category: "PROTOCOL_VERSION_MISMATCH", location, diagnostic: "HELLO required" });
    return { ...this.accepted };
  }
  assertAdapter(adapterInstanceId: string, location: string): void {
    const accepted = this.requireAccepted(location);
    if (accepted.adapterInstanceId !== adapterInstanceId) throw this.violations.record({ category: "STALE_SESSION_EPOCH", location, diagnostic: "adapter instance mismatch" });
  }
  reset(): void { this.accepted = null; }
}
