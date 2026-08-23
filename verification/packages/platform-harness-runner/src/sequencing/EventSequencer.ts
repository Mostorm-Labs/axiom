import type { EventDraftPayload, SourceId, TaggedU64 } from "@axiom/platform-harness-protocol";

export interface SequencedEvent {
  eventSeq: TaggedU64;
  sourceId: SourceId;
  event: string;
  data: EventDraftPayload["data"];
}

export class EventSequencer {
  private value = 0n;
  private readonly events: SequencedEvent[] = [];
  append(sourceId: SourceId, draft: EventDraftPayload): SequencedEvent {
    this.value += 1n;
    const event = Object.freeze({ eventSeq: `u64:${this.value.toString(16).padStart(16, "0")}` as TaggedU64, sourceId, event: draft.event, data: Object.freeze({ ...draft.data }) });
    this.events.push(event); return event;
  }
  snapshot(): SequencedEvent[] { return this.events.map((event) => ({ ...event, data: { ...event.data } })); }
}
