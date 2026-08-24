import type { BoundaryMode, HarnessEnvelope } from "@axiom/platform-harness-protocol";
export type TransportReceiver = (bytes: Uint8Array, location: string) => void;
export interface ITransport {
  readonly boundaryMode: BoundaryMode;
  connect(receiver: TransportReceiver): void;
  send(envelope: HarnessEnvelope, location?: string): void;
  inject(bytes: Uint8Array, location?: string): void;
  disconnect(): void;
  readonly connected: boolean;
}
