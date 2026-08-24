import { encodeEnvelope, type HarnessEnvelope } from "@axiom/platform-harness-protocol";
import type { ITransport, TransportReceiver } from "./ITransport.js";
import { JsonLineCodec } from "./JsonLineCodec.js";
import { TransportError } from "./TransportDiagnostics.js";
export class SerializedLoopbackTransport implements ITransport {
  readonly boundaryMode = "OUT_OF_PROCESS" as const; private receiver: TransportReceiver | null = null; private readonly codec = new JsonLineCodec();
  get connected(): boolean { return this.receiver !== null; }
  connect(receiver: TransportReceiver): void { this.receiver = receiver; }
  send(envelope: HarnessEnvelope, location = "transport.serialized"): void { const decoded = this.codec.decode(this.codec.encode(envelope, location), location); this.require(location)(encodeEnvelope(decoded), location); }
  inject(bytes: Uint8Array, location = "transport.serialized.inject"): void { const decoded = this.codec.decode(bytes, location); this.require(location)(encodeEnvelope(decoded), location); }
  disconnect(): void { this.receiver = null; }
  private require(location: string): TransportReceiver { if (!this.receiver) throw new TransportError({ category: "DISCONNECTED", location }); return this.receiver; }
}
