import { MESSAGE_TYPE_SET, type MessageType } from "./enums.js";
import { asMessageId, asSessionId, asTaggedU64, type MessageId, type SessionId, type TaggedU64 } from "./ids.js";
import { parseActionReceipt, parseActionRequest } from "./action.js";
import { parseActionCompletion } from "./completion.js";
import { parseEventDraft } from "./event.js";
import { parseAdapterError, parseFaultStatus } from "./fault.js";
import { parseFenceStatus } from "./fence.js";
import { parseCaptureResult } from "./observation.js";
import { parseCaptureRequest } from "./scenario.js";
import { requireRecord, requireStrictKeys, type JsonObject } from "./schema_loader.js";
import { parseCloseSession, parseCloseSessionResult, parseHello, parseOpenSession, parseOpenSessionResult } from "./session.js";
import { parseSourcePayload } from "./source.js";

export const HARNESS_PROTOCOL = "axiom-platform-harness-exec-v1" as const;
export const HARNESS_PROTOCOL_VERSION = 1 as const;

export type RejectionCategory = "INVALID_UTF8" | "INVALID_JSON" | "SCHEMA_REJECTED" | "VERSION_REJECTED" | "MESSAGE_TYPE_REJECTED" | "SEMANTIC_REJECTED";

export class ProtocolRejection extends Error {
  constructor(public readonly category: RejectionCategory, message: string) { super(message); this.name = "ProtocolRejection"; }
}

export interface HarnessEnvelope {
  protocol: typeof HARNESS_PROTOCOL;
  protocolVersion: typeof HARNESS_PROTOCOL_VERSION;
  messageId: MessageId;
  messageType: MessageType;
  sessionId: SessionId;
  sessionEpoch: TaggedU64;
  payload: JsonObject;
}

const ENVELOPE_KEYS = ["protocol", "protocolVersion", "messageId", "messageType", "sessionId", "sessionEpoch", "payload"] as const;

function parsePayload(messageType: MessageType, payload: Record<string, unknown>): JsonObject {
  switch (messageType) {
    case "HELLO": return parseHello(payload);
    case "OPEN_SESSION": return parseOpenSession(payload);
    case "OPEN_SESSION_RESULT": return parseOpenSessionResult(payload);
    case "ACTION_REQUEST": return parseActionRequest(payload);
    case "ACTION_RECEIPT": return parseActionReceipt(payload);
    case "ACTION_COMPLETION": return parseActionCompletion(payload);
    case "EVENT_DRAFT": return parseEventDraft(payload);
    case "SOURCE_LEASE_OPEN": case "SOURCE_LEASE_CLOSE": case "SOURCE_ATTEMPT": return parseSourcePayload(payload);
    case "FAULT_STATUS": return parseFaultStatus(payload);
    case "FENCE_STATUS": return parseFenceStatus(payload);
    case "CAPTURE_REQUEST": return parseCaptureRequest(payload);
    case "CAPTURE_RESULT": return parseCaptureResult(payload);
    case "CLOSE_SESSION": return parseCloseSession(payload);
    case "CLOSE_SESSION_RESULT": return parseCloseSessionResult(payload);
    case "ADAPTER_ERROR": return parseAdapterError(payload);
  }
}

export function parseEnvelopeValue(input: unknown): HarnessEnvelope {
  let raw: Record<string, unknown>;
  try { raw = requireRecord(input, "envelope"); requireStrictKeys(raw, ENVELOPE_KEYS); }
  catch (error) { throw new ProtocolRejection("SCHEMA_REJECTED", String(error)); }
  if (raw.protocol !== HARNESS_PROTOCOL || raw.protocolVersion !== HARNESS_PROTOCOL_VERSION) {
    throw new ProtocolRejection("VERSION_REJECTED", "unsupported protocol or protocolVersion");
  }
  if (typeof raw.messageType !== "string" || !MESSAGE_TYPE_SET.has(raw.messageType)) {
    throw new ProtocolRejection("MESSAGE_TYPE_REJECTED", "unknown messageType");
  }
  try {
    const messageType = raw.messageType as MessageType;
    return {
      protocol: HARNESS_PROTOCOL, protocolVersion: HARNESS_PROTOCOL_VERSION,
      messageId: asMessageId(raw.messageId), messageType,
      sessionId: asSessionId(raw.sessionId), sessionEpoch: asTaggedU64(raw.sessionEpoch),
      payload: parsePayload(messageType, requireRecord(raw.payload, "payload")),
    };
  } catch (error) {
    throw new ProtocolRejection("SEMANTIC_REJECTED", String(error));
  }
}

export function decodeEnvelope(bytes: Uint8Array): HarnessEnvelope {
  let text: string;
  try { text = new TextDecoder("utf-8", { fatal: true }).decode(bytes); }
  catch (error) { throw new ProtocolRejection("INVALID_UTF8", String(error)); }
  let value: unknown;
  try { value = JSON.parse(text); }
  catch (error) { throw new ProtocolRejection("INVALID_JSON", String(error)); }
  return parseEnvelopeValue(value);
}

export function encodeEnvelope(envelope: HarnessEnvelope): Uint8Array {
  const validated = parseEnvelopeValue(envelope);
  return new TextEncoder().encode(JSON.stringify(validated));
}
