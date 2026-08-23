import { asSourceId, type SourceId } from "./ids.js";
import { requireStrictKeys, type JsonObject } from "./schema_loader.js";
export interface SourcePayload extends JsonObject { sourceId: SourceId; }
export function parseSourcePayload(value: Record<string, unknown>): SourcePayload {
  requireStrictKeys(value, ["sourceId"]); return { sourceId: asSourceId(value.sourceId) };
}
