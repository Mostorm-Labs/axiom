import { encodeEnvelope, type HarnessEnvelope } from "@axiom/platform-harness-protocol";
import type { ITransport, TransportReceiver } from "./ITransport.js";
import { TransportError } from "./TransportDiagnostics.js";
export class InProcessTransport implements ITransport {
  readonly boundaryMode = "IN_PROCESS" as const; private receiver: TransportReceiver | null = null;
  get connected(): boolean { return this.receiver !== null; }
  connect(receiver: TransportReceiver): void { this.receiver = receiver; }
  send(envelope: HarnessEnvelope, location = "transport.in-process"): void { this.require(location)(encodeEnvelope(envelope), location); }
  inject(bytes: Uint8Array, location = "transport.in-process.inject"): void { this.require(location)(bytes.slice(), location); }
  disconnect(): void { this.receiver = null; }
  private require(location: string): TransportReceiver { if (!this.receiver) throw new TransportError({ category: "DISCONNECTED", location }); return this.receiver; }
}
