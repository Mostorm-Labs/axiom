import { type ActionCompletionPayload, type ActionReceiptPayload, type ActionRequestPayload, type HarnessEnvelope, type HelloPayload, parseActionCompletion, parseActionReceipt, parseHello, asSessionId, asTaggedU64 } from "@axiom/platform-harness-protocol";
import { HarnessEnvelopeCodec } from "./codec/HarnessEnvelopeCodec.js";
import { AdapterHandshake } from "./handshake/AdapterHandshake.js";
import { ActionRegistry } from "./registry/ActionRegistry.js";
import { CompletionRegistry } from "./registry/CompletionRegistry.js";
import { SessionRegistry } from "./registry/SessionRegistry.js";
import { CommandSequencer } from "./sequencing/CommandSequencer.js";
import { ProtocolViolationCollector, type ProtocolDivergence } from "./diagnostics/ProtocolViolationCollector.js";

export interface RunnerCheckpoint { ingressSeq: string; session: object; actions: object[]; completions: object[]; transitionTrace: object[]; firstDivergence: ProtocolDivergence | null; }
export class ReferencePlatformRunner {
  readonly violations = new ProtocolViolationCollector();
  private readonly transitionValues: object[] = [];
  readonly handshake = new AdapterHandshake(this.violations);
  readonly sessions = new SessionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly commands = new CommandSequencer(this.violations);
  readonly actions = new ActionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly completions = new CompletionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly codec = new HarnessEnvelopeCodec(this.violations);
  private connectionBoundary: "IN_PROCESS" | "OUT_OF_PROCESS" | null = null;
  private ingress = 0n;

  connect(boundary: "IN_PROCESS" | "OUT_OF_PROCESS"): void { this.connectionBoundary = boundary; this.handshake.reset(); }
  acceptHello(payload: HelloPayload, location = "handshake.hello"): void { if (!this.connectionBoundary) throw new Error("connect required"); this.handshake.accept(payload, this.connectionBoundary, location); }
  openSession(sessionId: string, sessionEpoch: string, adapterInstanceId: string, location = "session.open"): void {
    this.handshake.assertAdapter(adapterInstanceId, location); this.sessions.open(adapterInstanceId, asSessionId(sessionId), asTaggedU64(sessionEpoch), location);
  }
  dispatchAction(request: ActionRequestPayload, location = "action.request"): { commandSeq: string; actionId: string } {
    this.sessions.requireCurrentOpen(location);
    const commandSeq = this.commands.next(); const normalized = { ...request, commandSeq };
    this.actions.create(normalized, location); return { commandSeq, actionId: request.actionId };
  }
  receiveReceipt(payload: ActionReceiptPayload, location = "action.receipt"): void {
    if (payload.receiptStatus === "DISPATCHED" && payload.tokenId && this.completions.has(payload.tokenId)) throw this.violations.record({ category: "DUPLICATE_COMPLETION", location, diagnostic: "completion token already registered" });
    this.ingress += 1n; const action = this.actions.receipt(payload, location);
    if (action.tokenId && action.state === "COMPLETION_PENDING") this.completions.register(action.tokenId, action.actionId, location);
  }
  receiveCompletion(payload: ActionCompletionPayload, location = "action.completion"): void {
    this.ingress += 1n; this.completions.complete(payload.tokenId, payload.actionId, payload.outcome, location); this.actions.complete(payload.actionId, payload.tokenId, payload.outcome, location);
  }
  receiveEnvelope(bytes: Uint8Array, location = `ingress.${this.ingress + 1n}`): HarnessEnvelope {
    const envelope = this.codec.decode(bytes, location);
    if (envelope.messageType === "HELLO") { this.ingress += 1n; this.acceptHello(parseHello(envelope.payload), location); return envelope; }
    this.sessions.requireOpen(envelope.sessionId, envelope.sessionEpoch, location);
    if (envelope.messageType === "ACTION_RECEIPT") { this.receiveReceipt(parseActionReceipt(envelope.payload), location); return envelope; }
    if (envelope.messageType === "ACTION_COMPLETION") { this.receiveCompletion(parseActionCompletion(envelope.payload), location); return envelope; }
    this.ingress += 1n;
    return envelope;
  }
  checkpoint(): RunnerCheckpoint { return { ingressSeq: `u64:${this.ingress.toString(16).padStart(16, "0")}`, session: this.sessions.snapshot(), actions: this.actions.snapshot(), completions: this.completions.snapshot(), transitionTrace: this.transitionValues.map((value) => ({ ...value })), firstDivergence: this.violations.first() }; }
  finalize(location = "finalize"): void {
    if (this.actions.unresolved().length || this.completions.unresolved().length) throw this.violations.record({ category: "UNRESOLVED_REGISTRY", location });
    const current = this.sessions.requireCurrentOpen(location); this.sessions.close(current.sessionId, current.sessionEpoch, location);
  }
}
