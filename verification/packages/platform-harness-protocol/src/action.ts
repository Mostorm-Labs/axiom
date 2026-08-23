import { asActionId, type ActionId } from "./ids.js";
import { requireBoolean, requireRecord, requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";

export interface ActionRequestPayload extends JsonObject { actionId: ActionId; action: string; parameters: JsonObject; }
export interface ActionReceiptPayload extends JsonObject { actionId: ActionId; accepted: boolean; }

export function parseActionRequest(value: Record<string, unknown>): ActionRequestPayload {
  requireStrictKeys(value, ["actionId", "action", "parameters"]);
  return { actionId: asActionId(value.actionId), action: requireString(value.action, "payload.action"), parameters: requireRecord(value.parameters, "payload.parameters") as JsonObject };
}
export function parseActionReceipt(value: Record<string, unknown>): ActionReceiptPayload {
  requireStrictKeys(value, ["actionId", "accepted"]);
  return { actionId: asActionId(value.actionId), accepted: requireBoolean(value.accepted, "payload.accepted") };
}
