import { asTaggedU64, type TaggedU64 } from "./ids.js";
import { requireStrictKeys, type JsonObject } from "./schema_loader.js";
export interface FenceStatusPayload extends JsonObject { generation: TaggedU64; }
export function parseFenceStatus(value: Record<string, unknown>): FenceStatusPayload {
  requireStrictKeys(value, ["generation"]); return { generation: asTaggedU64(value.generation) };
}
