import { requireBoolean, requireStrictKeys, requireString, requireStringArray, type JsonObject } from "./schema_loader.js";

export interface HelloPayload extends JsonObject { adapter: string; capabilities: string[]; }
export interface OpenSessionPayload extends JsonObject { client: string; }
export interface OpenSessionResultPayload extends JsonObject { accepted: boolean; }
export interface CloseSessionPayload extends JsonObject { reason: string; }
export interface CloseSessionResultPayload extends JsonObject { closed: boolean; }

export function parseHello(value: Record<string, unknown>): HelloPayload {
  requireStrictKeys(value, ["adapter", "capabilities"]);
  return { adapter: requireString(value.adapter, "payload.adapter"), capabilities: requireStringArray(value.capabilities, "payload.capabilities") };
}
export function parseOpenSession(value: Record<string, unknown>): OpenSessionPayload {
  requireStrictKeys(value, ["client"]); return { client: requireString(value.client, "payload.client") };
}
export function parseOpenSessionResult(value: Record<string, unknown>): OpenSessionResultPayload {
  requireStrictKeys(value, ["accepted"]); return { accepted: requireBoolean(value.accepted, "payload.accepted") };
}
export function parseCloseSession(value: Record<string, unknown>): CloseSessionPayload {
  requireStrictKeys(value, ["reason"]); return { reason: requireString(value.reason, "payload.reason") };
}
export function parseCloseSessionResult(value: Record<string, unknown>): CloseSessionResultPayload {
  requireStrictKeys(value, ["closed"]); return { closed: requireBoolean(value.closed, "payload.closed") };
}
