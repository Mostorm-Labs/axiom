import { type ActionCompletionPayload, type ActionReceiptPayload, type ActionRequestPayload, type EventDraftPayload, type HarnessEnvelope, type HelloPayload, type SourceId, type TaggedU64, parseActionCompletion, parseActionReceipt, parseEventDraft, parseFaultStatus, parseFenceStatus, parseHello, parseSourcePayload, asSessionId, asTaggedU64 } from "@axiom/platform-harness-protocol";
import { HarnessEnvelopeCodec } from "./codec/HarnessEnvelopeCodec.js";
import { AdapterHandshake } from "./handshake/AdapterHandshake.js";
import { ActionRegistry } from "./registry/ActionRegistry.js";
import { CompletionRegistry } from "./registry/CompletionRegistry.js";
import { SessionRegistry } from "./registry/SessionRegistry.js";
import { CommandSequencer } from "./sequencing/CommandSequencer.js";
import { EventSequencer } from "./sequencing/EventSequencer.js";
import { SourceLeaseRegistry } from "./registry/SourceLeaseRegistry.js";
import { FaultRegistry } from "./registry/FaultRegistry.js";
import { LateEventFence } from "./fence/LateEventFence.js";
import { ProtocolViolationCollector, type ProtocolDivergence } from "./diagnostics/ProtocolViolationCollector.js";

export interface RunnerCheckpoint { ingressSeq: string; session: object; actions: object[]; completions: object[]; sources: object[]; faults: object[]; fence: object; events: object[]; transitionTrace: object[]; firstDivergence: ProtocolDivergence | null; }
export class ReferencePlatformRunner {
  readonly violations = new ProtocolViolationCollector();
  private readonly transitionValues: object[] = [];
  readonly handshake = new AdapterHandshake(this.violations);
  readonly sessions = new SessionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly commands = new CommandSequencer(this.violations);
  readonly actions = new ActionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly completions = new CompletionRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly sources = new SourceLeaseRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly faults = new FaultRegistry(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly fence = new LateEventFence(this.violations, (value) => this.transitionValues.push(Object.freeze({ ...value })));
  readonly events = new EventSequencer();
  readonly codec = new HarnessEnvelopeCodec(this.violations);
  private connectionBoundary: "IN_PROCESS" | "OUT_OF_PROCESS" | null = null;
  private ingress = 0n;

  connect(boundary: "IN_PROCESS" | "OUT_OF_PROCESS"): void { this.connectionBoundary = boundary; this.handshake.reset(); }
  transportLost(location = "transport.lost"): void { this.handshake.reset(); this.sessions.transportLost(location); }
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
  openSource(sourceId: SourceId, location = "source.open"): void { this.sessions.requireCurrentOpen(location); this.sources.open(sourceId, location); }
  closeSource(sourceId: SourceId, location = "source.close"): void { this.sessions.requireCurrentOpen(location); this.sources.close(sourceId, location); }
  advanceFence(generation: TaggedU64, location = "fence.advance"): void { this.sessions.requireCurrentOpen(location); this.fence.advance(generation, location); }
  receiveEvent(sourceId: SourceId, generation: TaggedU64, draft: EventDraftPayload, location = "event.draft"): object {
    this.sessions.requireCurrentOpen(location); this.sources.requireOpen(sourceId, location); this.fence.accept(generation, location); this.ingress += 1n;
    return this.events.append(sourceId, draft);
  }
  setFault(fault: string, active: boolean, location = "fault.status"): void { this.sessions.requireCurrentOpen(location); if (active) this.faults.activate(fault, location); else this.faults.clear(fault, location); }
  receiveEnvelope(bytes: Uint8Array, location = `ingress.${this.ingress + 1n}`): HarnessEnvelope {
    const envelope = this.codec.decode(bytes, location);
    if (envelope.messageType === "HELLO") { this.ingress += 1n; this.acceptHello(parseHello(envelope.payload), location); return envelope; }
    this.sessions.requireOpen(envelope.sessionId, envelope.sessionEpoch, location);
    if (envelope.messageType === "ACTION_RECEIPT") { this.receiveReceipt(parseActionReceipt(envelope.payload), location); return envelope; }
    if (envelope.messageType === "ACTION_COMPLETION") { this.receiveCompletion(parseActionCompletion(envelope.payload), location); return envelope; }
    if (envelope.messageType === "SOURCE_LEASE_OPEN") { this.ingress += 1n; this.openSource(parseSourcePayload(envelope.payload).sourceId, location); return envelope; }
    if (envelope.messageType === "SOURCE_LEASE_CLOSE") { this.ingress += 1n; this.closeSource(parseSourcePayload(envelope.payload).sourceId, location); return envelope; }
    if (envelope.messageType === "FENCE_STATUS") { this.ingress += 1n; this.advanceFence(parseFenceStatus(envelope.payload).generation, location); return envelope; }
    if (envelope.messageType === "FAULT_STATUS") { this.ingress += 1n; const fault = parseFaultStatus(envelope.payload); this.setFault(fault.fault, fault.active, location); return envelope; }
    if (envelope.messageType === "EVENT_DRAFT") { throw this.violations.record({ category: "EVENT_SOURCE_INACTIVE", location, diagnostic: "EVENT_DRAFT requires source and generation transport context" }); }
    this.ingress += 1n;
    return envelope;
  }
  checkpoint(): RunnerCheckpoint { return { ingressSeq: `u64:${this.ingress.toString(16).padStart(16, "0")}`, session: this.sessions.snapshot(), actions: this.actions.snapshot(), completions: this.completions.snapshot(), sources: this.sources.snapshot(), faults: this.faults.snapshot(), fence: this.fence.snapshot(), events: this.events.snapshot(), transitionTrace: this.transitionValues.map((value) => ({ ...value })), firstDivergence: this.violations.first() }; }
  finalize(location = "finalize"): void {
    if (this.actions.unresolved().length || this.completions.unresolved().length || this.sources.unresolved().length || this.faults.unresolved().length) throw this.violations.record({ category: "UNRESOLVED_REGISTRY", location });
    const current = this.sessions.requireCurrentOpen(location); this.sessions.close(current.sessionId, current.sessionEpoch, location);
  }
}
