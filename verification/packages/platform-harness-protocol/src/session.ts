import { BOUNDARY_MODES, type BoundaryMode } from "./enums.js";
import { requireBoolean, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";

export interface HelloPayload extends JsonObject { adapterInstanceId: string; supportedProtocolVersions: number[]; boundaryMode: BoundaryMode; }
export interface OpenSessionPayload extends JsonObject { client: string; }
export interface OpenSessionResultPayload extends JsonObject { accepted: boolean; }
export interface CloseSessionPayload extends JsonObject { reason: string; }
export interface CloseSessionResultPayload extends JsonObject { closed: boolean; }

export function parseHello(value: Record<string, unknown>): HelloPayload {
  requireStrictKeys(value, ["adapterInstanceId", "supportedProtocolVersions", "boundaryMode"]);
  if (!Array.isArray(value.supportedProtocolVersions) || value.supportedProtocolVersions.length === 0 || value.supportedProtocolVersions.some((version) => !Number.isInteger(version))) {
    throw new TypeError("payload.supportedProtocolVersions: expected non-empty integer[]");
  }
  if (typeof value.boundaryMode !== "string" || !BOUNDARY_MODES.includes(value.boundaryMode as BoundaryMode)) throw new TypeError("payload.boundaryMode: invalid boundary mode");
  return {
    adapterInstanceId: requireString(value.adapterInstanceId, "payload.adapterInstanceId"),
    supportedProtocolVersions: [...value.supportedProtocolVersions] as number[],
    boundaryMode: value.boundaryMode as BoundaryMode,
  };
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
