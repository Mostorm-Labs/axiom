import assert from "node:assert/strict";
import test from "node:test";
import { asSourceId, asTaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolation, ReferencePlatformRunner } from "../dist/index.js";

const hello = { adapterInstanceId: "adapter:scripted:001", supportedProtocolVersions: [1], boundaryMode: "IN_PROCESS" };
function openRunner() { const runner = new ReferencePlatformRunner(); runner.connect("IN_PROCESS"); runner.acceptHello(hello); runner.openSession("session:001", "u64:0000000000000001", "adapter:scripted:001"); return runner; }
function category(fn, expected) { assert.throws(fn, (error) => error instanceof ProtocolViolation && error.divergence.category === expected); }

test("source leases gate events and event sequence is runner-owned", () => {
  const missing = openRunner(); const source = asSourceId("source:touch");
  category(() => missing.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event: "POINTER", data: {} }), "SOURCE_LEASE_UNKNOWN");
  const runner = openRunner(); runner.openSource(source);
  const first = runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event: "POINTER", data: {} });
  const second = runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event: "POINTER", data: { x: 1 } });
  assert.deepEqual([first.eventSeq, second.eventSeq], ["u64:0000000000000001", "u64:0000000000000002"]); runner.closeSource(source);
  category(() => runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event: "POINTER", data: {} }), "EVENT_SOURCE_INACTIVE");
});

test("duplicate and invalid source transitions are rejected", () => {
  const duplicate = openRunner(); const source = asSourceId("source:touch"); duplicate.openSource(source); category(() => duplicate.openSource(source), "SOURCE_LEASE_DUPLICATE");
  const unknown = openRunner(); category(() => unknown.closeSource(source), "SOURCE_LEASE_UNKNOWN");
});

test("generation fence rejects late events without consuming event sequence", () => {
  const runner = openRunner(); const source = asSourceId("source:touch"); runner.openSource(source); runner.advanceFence(asTaggedU64("u64:0000000000000001"));
  category(() => runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event: "POINTER", data: {} }), "LATE_EVENT_REJECTED");
  const event = runner.receiveEvent(source, asTaggedU64("u64:0000000000000001"), { event: "POINTER", data: {} }); assert.equal(event.eventSeq, "u64:0000000000000001");
});

test("generation fence advances monotonically", () => {
  const runner = openRunner(); runner.advanceFence(asTaggedU64("u64:0000000000000002")); category(() => runner.advanceFence(asTaggedU64("u64:0000000000000001")), "FENCE_GENERATION_INVALID");
});

test("fault handles have explicit activate and clear transitions", () => {
  const unknown = openRunner(); category(() => unknown.setFault("SURFACE_LOST", false), "FAULT_HANDLE_UNKNOWN");
  const runner = openRunner(); runner.setFault("SURFACE_LOST", true); category(() => runner.setFault("SURFACE_LOST", true), "FAULT_HANDLE_DUPLICATE");
});

test("faults and sources must be closed before finalization", () => {
  const runner = openRunner(); const source = asSourceId("source:touch"); runner.openSource(source); runner.setFault("SURFACE_LOST", true);
  category(() => runner.finalize(), "UNRESOLVED_REGISTRY"); runner.setFault("SURFACE_LOST", false); runner.closeSource(source); runner.finalize(); assert.equal(runner.checkpoint().session.state, "CLOSED");
});

test("core B transition trace and event replay are deterministic", () => {
  const replay = () => { const runner = openRunner(); const source = asSourceId("source:touch"); runner.openSource(source); runner.advanceFence(asTaggedU64("u64:0000000000000001")); runner.receiveEvent(source, asTaggedU64("u64:0000000000000001"), { event: "POINTER", data: { x: 1 } }); runner.closeSource(source); runner.finalize(); return runner.checkpoint(); };
  const expected = replay(); for (let index = 0; index < 10; index += 1) assert.deepEqual(replay(), expected);
});
