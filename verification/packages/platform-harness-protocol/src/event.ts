import { requireRecord, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";
export interface EventDraftPayload extends JsonObject { event: string; data: JsonObject; }
export function parseEventDraft(value: Record<string, unknown>): EventDraftPayload {
  requireStrictKeys(value, ["event", "data"]);
  return { event: requireString(value.event, "payload.event"), data: requireRecord(value.data, "payload.data") as JsonObject };
}
