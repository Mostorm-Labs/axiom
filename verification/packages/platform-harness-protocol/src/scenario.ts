import { requireRecord, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";
export interface CaptureRequestPayload extends JsonObject { captureId: string; options: JsonObject; }
export function parseCaptureRequest(value: Record<string, unknown>): CaptureRequestPayload {
  requireStrictKeys(value, ["captureId", "options"]);
  return { captureId: requireString(value.captureId, "payload.captureId"), options: requireRecord(value.options, "payload.options") as JsonObject };
}
