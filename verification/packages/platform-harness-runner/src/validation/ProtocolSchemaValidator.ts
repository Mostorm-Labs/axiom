import { parseEnvelopeValue, ProtocolRejection, type HarnessEnvelope } from "@axiom/platform-harness-protocol";
import { ProtocolViolationCollector } from "../diagnostics/ProtocolViolationCollector.js";
export class ProtocolSchemaValidator {
  constructor(private readonly violations: ProtocolViolationCollector) {}
  validate(value: unknown, location: string): HarnessEnvelope {
    try { return parseEnvelopeValue(value); }
    catch (error) {
      if (error instanceof ProtocolRejection) {
        const category = error.category === "VERSION_REJECTED" ? "PROTOCOL_VERSION_MISMATCH" : error.category === "MESSAGE_TYPE_REJECTED" ? "UNKNOWN_MESSAGE_TYPE" : "INVALID_ENVELOPE";
        throw this.violations.record({ category, location, diagnostic: error.message });
      }
      throw error;
    }
  }
}
