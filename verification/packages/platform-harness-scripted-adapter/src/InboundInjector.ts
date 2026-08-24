import type { ITransport } from "@axiom/platform-harness-transport";
export class InboundInjector { constructor(private readonly transport: ITransport) {} inject(bytes: Uint8Array, location: string): void { this.transport.inject(bytes, location); } }
