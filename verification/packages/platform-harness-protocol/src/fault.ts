import { requireBoolean, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";
export interface FaultStatusPayload extends JsonObject { fault: string; active: boolean; }
export interface AdapterErrorPayload extends JsonObject { code: string; message: string; }
export function parseFaultStatus(value: Record<string, unknown>): FaultStatusPayload {
  requireStrictKeys(value, ["fault", "active"]); return { fault: requireString(value.fault, "payload.fault"), active: requireBoolean(value.active, "payload.active") };
}
export function parseAdapterError(value: Record<string, unknown>): AdapterErrorPayload {
  requireStrictKeys(value, ["code", "message"]); return { code: requireString(value.code, "payload.code"), message: requireString(value.message, "payload.message") };
}
