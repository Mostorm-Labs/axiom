import assert from "node:assert/strict";
import test from "node:test";
import { asActionId, asCompletionTokenId, asTaggedU64 } from "@axiom/platform-harness-protocol";
import { ProtocolViolation, ReferencePlatformRunner } from "../dist/index.js";

const hello = (boundaryMode = "IN_PROCESS", versions = [1]) => ({ adapterInstanceId: "adapter:scripted:001", supportedProtocolVersions: versions, boundaryMode });
const action = (actionId = "action:001", completionMode = "WAIT_FOR_ACTION_COMPLETION") => ({
  actionId: asActionId(actionId), stepId: "step:001", commandSeq: asTaggedU64("u64:0000000000000000"), completionMode, action: "render", parameters: {},
});
const receipt = (commandSeq, overrides = {}) => ({ actionId: asActionId("action:001"), commandSeq: commandSeq, receiptStatus: "DISPATCHED", tokenId: asCompletionTokenId("completion:001"), terminalOutcome: null, ...overrides });
const completion = (overrides = {}) => ({ tokenId: asCompletionTokenId("completion:001"), actionId: asActionId("action:001"), outcome: "SUCCEEDED", artifacts: [], ...overrides });
function openRunner(boundaryMode = "IN_PROCESS", epoch = "u64:0000000000000001") {
  const runner = new ReferencePlatformRunner(); runner.connect(boundaryMode); runner.acceptHello(hello(boundaryMode)); runner.openSession("session:001", epoch, "adapter:scripted:001"); return runner;
}
function category(fn, expected) { assert.throws(fn, (error) => error instanceof ProtocolViolation && error.divergence.category === expected); }

test("valid handshake/session/action async completion happy path", () => {
  const runner = openRunner(); const dispatched = runner.dispatchAction(action()); runner.receiveReceipt(receipt(dispatched.commandSeq)); runner.receiveCompletion(completion()); runner.finalize();
  const state = runner.checkpoint(); assert.equal(state.session.state, "CLOSED"); assert.equal(state.actions[0].state, "TERMINAL"); assert.equal(state.completions[0].state, "COMPLETED"); assert.equal(state.firstDivergence, null);
});

test("handshake is required and incompatible version is rejected", () => {
  const runner = new ReferencePlatformRunner(); runner.connect("IN_PROCESS");
  category(() => runner.openSession("session:001", "u64:0000000000000001", "adapter:scripted:001"), "PROTOCOL_VERSION_MISMATCH");
  const incompatible = new ReferencePlatformRunner(); incompatible.connect("IN_PROCESS"); category(() => incompatible.acceptHello(hello("IN_PROCESS", [2])), "PROTOCOL_VERSION_MISMATCH");
});

test("zero and reused epoch are rejected", () => {
  const zero = new ReferencePlatformRunner(); zero.connect("IN_PROCESS"); zero.acceptHello(hello()); category(() => zero.openSession("session:001", "u64:0000000000000000", "adapter:scripted:001"), "SESSION_EPOCH_INVALID");
  const runner = openRunner(); runner.finalize(); category(() => runner.openSession("session:002", "u64:0000000000000001", "adapter:scripted:001"), "SESSION_EPOCH_REUSED");
});

test("stale session epoch inbound is rejected before registry mutation", () => {
  const runner = openRunner(); const before = runner.checkpoint();
  const envelope = { protocol: "axiom-platform-harness-exec-v1", protocolVersion: 1, messageId: "msg:00001", messageType: "ACTION_RECEIPT", sessionId: "session:001", sessionEpoch: "u64:0000000000000000", payload: { actionId: "action:001", commandSeq: "u64:0000000000000001", receiptStatus: "DISPATCHED", tokenId: "completion:001" } };
  category(() => runner.receiveEnvelope(new TextEncoder().encode(JSON.stringify(envelope))), "STALE_SESSION_EPOCH"); assert.deepEqual(runner.checkpoint().actions, before.actions);
});

test("duplicate actionId and command sequence mismatch are rejected", () => {
  const duplicate = openRunner(); duplicate.dispatchAction(action()); category(() => duplicate.dispatchAction(action()), "ACTION_ID_DUPLICATE");
  const mismatch = openRunner(); mismatch.dispatchAction(action()); category(() => mismatch.receiveReceipt(receipt(asTaggedU64("u64:0000000000000009"))), "COMMAND_SEQ_MISMATCH");
});

test("missing completion token and invalid sync terminal are rejected", () => {
  const missing = openRunner(); const d1 = missing.dispatchAction(action()); const missingBefore = missing.checkpoint(); category(() => missing.receiveReceipt(receipt(d1.commandSeq, { tokenId: null })), "MISSING_COMPLETION_TOKEN"); assert.deepEqual(missing.checkpoint().actions, missingBefore.actions);
  const sync = openRunner(); const d2 = sync.dispatchAction(action()); const syncBefore = sync.checkpoint(); category(() => sync.receiveReceipt(receipt(d2.commandSeq, { receiptStatus: "COMPLETED_SYNC", tokenId: null, terminalOutcome: null })), "INVALID_SYNC_TERMINAL"); assert.deepEqual(sync.checkpoint().actions, syncBefore.actions);
});

test("unknown duplicate and action-mismatched completion are rejected", () => {
  const unknown = openRunner(); category(() => unknown.receiveCompletion(completion()), "UNKNOWN_COMPLETION_TOKEN");
  const duplicate = openRunner(); const d = duplicate.dispatchAction(action()); duplicate.receiveReceipt(receipt(d.commandSeq)); duplicate.receiveCompletion(completion()); category(() => duplicate.receiveCompletion(completion()), "DUPLICATE_COMPLETION");
  const mismatch = openRunner(); const m = mismatch.dispatchAction(action()); mismatch.receiveReceipt(receipt(m.commandSeq)); category(() => mismatch.receiveCompletion(completion({ actionId: asActionId("action:002") })), "COMPLETION_ACTION_MISMATCH");
});

test("dispatch-only and completed-sync reach terminal without completion token", () => {
  const direct = openRunner(); const d1 = direct.dispatchAction(action("action:001", "DISPATCH_ONLY")); direct.receiveReceipt(receipt(d1.commandSeq, { tokenId: null })); direct.finalize(); assert.equal(direct.checkpoint().actions[0].state, "TERMINAL");
  const sync = openRunner(); const d2 = sync.dispatchAction(action()); sync.receiveReceipt(receipt(d2.commandSeq, { receiptStatus: "COMPLETED_SYNC", tokenId: null, terminalOutcome: "SUCCEEDED" })); sync.finalize(); assert.equal(sync.checkpoint().actions[0].outcome, "SUCCEEDED");
});

test("finalizer rejects unresolved action/completion registries", () => {
  const runner = openRunner(); const d = runner.dispatchAction(action()); runner.receiveReceipt(receipt(d.commandSeq)); category(() => runner.finalize(), "UNRESOLVED_REGISTRY");
});

test("first divergence category and location are deterministic across replay", () => {
  const replay = () => { const runner = openRunner(); const d = runner.dispatchAction(action()); try { runner.receiveReceipt(receipt(d.commandSeq, { tokenId: null }), "vector.p04"); } catch {} return runner.checkpoint().firstDivergence; };
  const values = Array.from({ length: 10 }, replay); for (const value of values) assert.deepEqual(value, { category: "MISSING_COMPLETION_TOKEN", location: "vector.p04" });
});

test("collector keeps deterministic first divergence while retaining diagnostics", () => {
  const runner = openRunner(); const d = runner.dispatchAction(action()); category(() => runner.receiveReceipt(receipt(d.commandSeq, { tokenId: null }), "p04"), "MISSING_COMPLETION_TOKEN"); category(() => runner.receiveCompletion(completion(), "p05"), "MISSING_COMPLETION_TOKEN");
  assert.equal(runner.violations.diagnostics().length, 2); assert.deepEqual(runner.violations.first(), { category: "MISSING_COMPLETION_TOKEN", location: "p04" });
});

test("malformed wire is classified as INVALID_ENVELOPE and does not mutate registries", () => {
  const runner = openRunner(); const before = runner.checkpoint();
  category(() => runner.receiveEnvelope(new TextEncoder().encode("not-json"), "wire.p01"), "INVALID_ENVELOPE");
  const after = runner.checkpoint(); assert.deepEqual(after.actions, before.actions); assert.deepEqual(after.completions, before.completions); assert.deepEqual(after.session, before.session);
});

test("transition trace is deterministic and exposes session/action/completion states", () => {
  const runner = openRunner(); const dispatched = runner.dispatchAction(action()); runner.receiveReceipt(receipt(dispatched.commandSeq)); runner.receiveCompletion(completion());
  const trace = runner.checkpoint().transitionTrace;
  assert.deepEqual(trace.map((item) => `${item.component}:${item.from}->${item.to}`), [
    "SESSION:NEW->OPEN", "ACTION:CREATED->REQUEST_SENT", "ACTION:REQUEST_SENT->RECEIPT_ACCEPTED", "ACTION:RECEIPT_ACCEPTED->COMPLETION_PENDING",
    "COMPLETION:UNREGISTERED->REGISTERED", "COMPLETION:REGISTERED->COMPLETED", "ACTION:COMPLETION_PENDING->TERMINAL",
  ]);
});
