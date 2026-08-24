import type { EventDraftPayload, HarnessEnvelope, SourceId, TaggedU64 } from "@axiom/platform-harness-protocol";
export type ScriptStep = { kind: "SEND"; envelope: HarnessEnvelope; location: string } | { kind: "EVENT_DRAFT_CONTEXT"; sourceId: SourceId; generation: TaggedU64; draft: EventDraftPayload; location: string } | { kind: "INJECT"; bytes: Uint8Array; location: string } | { kind: "DISCONNECT" } | { kind: "RECONNECT" };
export interface ScriptProgram { readonly steps: readonly ScriptStep[]; }
