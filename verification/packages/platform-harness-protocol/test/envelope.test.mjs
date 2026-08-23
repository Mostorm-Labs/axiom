import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";
import {
  HARNESS_PROTOCOL, ProtocolRejection, decodeEnvelope, encodeEnvelope, isSafeArtifactPath, parseEnvelopeValue,
} from "../dist/index.js";

const base = (messageType, payload, sequence = 42) => ({
  protocol: HARNESS_PROTOCOL,
  protocolVersion: 1,
  messageId: `msg:${String(sequence).padStart(5, "0")}`,
  messageType,
  sessionId: "session:001",
  sessionEpoch: "u64:0000000000000007",
  payload,
});

const roundTripCases = [
  base("HELLO", { adapter: "reference", capabilities: ["capture"] }, 1),
  base("OPEN_SESSION", { client: "unit-test" }, 2),
  base("ACTION_REQUEST", { actionId: "action:001", action: "render", parameters: { frame: 1 } }, 3),
  base("ACTION_RECEIPT", { actionId: "action:001", accepted: true }, 4),
  base("ACTION_COMPLETION", { actionId: "action:001", outcome: "SUCCEEDED", artifacts: ["captures/frame.rgba"] }, 5),
  base("EVENT_DRAFT", { event: "surface-ready", data: { width: 800 } }, 6),
];

const everyMessageType = [
  ...roundTripCases,
  base("OPEN_SESSION_RESULT", { accepted: true }, 7),
  base("SOURCE_LEASE_OPEN", { sourceId: "source:camera" }, 8),
  base("SOURCE_LEASE_CLOSE", { sourceId: "source:camera" }, 9),
  base("SOURCE_ATTEMPT", { sourceId: "source:camera" }, 10),
  base("FAULT_STATUS", { fault: "surface-lost", active: true }, 11),
  base("FENCE_STATUS", { generation: "u64:ffffffffffffffff" }, 12),
  base("CAPTURE_REQUEST", { captureId: "capture:001", options: {} }, 13),
  base("CAPTURE_RESULT", { captureId: "capture:001", artifactPath: "captures/frame.rgba" }, 14),
  base("CLOSE_SESSION", { reason: "complete" }, 15),
  base("CLOSE_SESSION_RESULT", { closed: true }, 16),
  base("ADAPTER_ERROR", { code: "SURFACE_LOST", message: "surface unavailable" }, 17),
];

for (const value of roundTripCases) test(`round-trip ${value.messageType}`, () => {
  const parsed = parseEnvelopeValue(value);
  assert.deepEqual(decodeEnvelope(encodeEnvelope(parsed)), parsed);
});

test("all v1 message types have a strict typed payload parser", () => {
  assert.equal(everyMessageType.length, 17);
  for (const value of everyMessageType) assert.equal(parseEnvelopeValue(value).messageType, value.messageType);
});

test("tagged-u64 max value round-trips as exact text", () => {
  const value = everyMessageType.find((entry) => entry.messageType === "FENCE_STATUS");
  const parsed = decodeEnvelope(encodeEnvelope(parseEnvelopeValue(value)));
  assert.equal(parsed.payload.generation, "u64:ffffffffffffffff");
});

function rejection(value, category) {
  assert.throws(() => parseEnvelopeValue(value), (error) => error instanceof ProtocolRejection && error.category === category);
}

test("rejects unknown envelope field", () => rejection({ ...roundTripCases[0], extra: true }, "SCHEMA_REJECTED"));
test("rejects protocol version", () => rejection({ ...roundTripCases[0], protocolVersion: 2 }, "VERSION_REJECTED"));
test("rejects unknown message type", () => rejection({ ...roundTripCases[0], messageType: "UNKNOWN" }, "MESSAGE_TYPE_REJECTED"));
test("rejects invalid tagged-u64 rather than accepting JS number", () => rejection({ ...roundTripCases[0], sessionEpoch: 7 }, "SEMANTIC_REJECTED"));
test("rejects unknown payload field", () => rejection({ ...roundTripCases[0], payload: { ...roundTripCases[0].payload, extra: true } }, "SEMANTIC_REJECTED"));
test("rejects invalid UTF-8", () => assert.throws(() => decodeEnvelope(Uint8Array.of(0xff)), (error) => error.category === "INVALID_UTF8"));
test("rejects invalid JSON", () => assert.throws(() => decodeEnvelope(new TextEncoder().encode("{")), (error) => error.category === "INVALID_JSON"));

test("safe artifact path helper is strict", () => {
  assert.equal(isSafeArtifactPath("captures/frame.rgba"), true);
  for (const path of ["/tmp/frame", "../frame", "a/../frame", "C:/frame", "a//frame", "a\\frame", "a frame"]) assert.equal(isSafeArtifactPath(path), false, path);
});

test("schema and typed parser accept the canonical fixture", async () => {
  const fixture = JSON.parse(await readFile(new URL("../../../schemas/platform/fixtures/platform-harness-envelope.valid.json", import.meta.url), "utf8"));
  assert.equal(parseEnvelopeValue(fixture).messageType, "HELLO");
});
