export interface TransportDiagnostic { category: "DISCONNECTED" | "NONPORTABLE_PAYLOAD" | "INVALID_JSON_LINE"; location: string; }
export class TransportError extends Error { constructor(public readonly diagnostic: TransportDiagnostic) { super(`${diagnostic.category} at ${diagnostic.location}`); this.name = "TransportError"; } }
