import { decodeEnvelope, encodeEnvelope, type HarnessEnvelope } from "@axiom/platform-harness-protocol";
import { TransportError } from "./TransportDiagnostics.js";
export class JsonLineCodec {
  encode(envelope: HarnessEnvelope, location: string): Uint8Array {
    try {
      const bytes = encodeEnvelope(envelope); const text = new TextDecoder().decode(bytes);
      if (text.includes("\n") || text.includes("\r")) throw new TypeError("embedded newline");
      return new TextEncoder().encode(`${text}\n`);
    } catch { throw new TransportError({ category: "NONPORTABLE_PAYLOAD", location }); }
  }
  decode(frame: Uint8Array, location: string): HarnessEnvelope {
    const text = new TextDecoder("utf-8", { fatal: true }).decode(frame);
    if (!text.endsWith("\n") || text.slice(0, -1).includes("\n") || text.includes("\r")) throw new TransportError({ category: "INVALID_JSON_LINE", location });
    try { return decodeEnvelope(new TextEncoder().encode(text.slice(0, -1))); }
    catch { throw new TransportError({ category: "INVALID_JSON_LINE", location }); }
  }
}
