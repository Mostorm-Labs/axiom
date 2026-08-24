import { asMessageId, asSessionId, asTaggedU64, type HarnessEnvelope } from "@axiom/platform-harness-protocol";
import type { ITransport } from "@axiom/platform-harness-transport";
import type { ReferencePlatformRunner } from "@axiom/platform-harness-runner";
import { ScriptCursor } from "./ScriptCursor.js"; import type { ScriptProgram } from "./ScriptProgram.js";
export class ScriptedAdapter {
  constructor(private readonly runner: ReferencePlatformRunner, private readonly transport: ITransport) {}
  connect(): void { this.runner.connect(this.transport.boundaryMode); this.transport.connect((bytes, location)=>this.runner.receiveEnvelope(bytes, location)); }
  run(program: ScriptProgram): void { const cursor=new ScriptCursor(program); let step; while((step=cursor.next())) { if(step.kind==="SEND") this.transport.send(step.envelope,step.location); else if(step.kind==="EVENT_DRAFT_CONTEXT") this.runner.receiveEvent(step.sourceId,step.generation,step.draft,step.location); else if(step.kind==="INJECT") this.transport.inject(step.bytes,step.location); else if(step.kind==="DISCONNECT") { this.runner.transportLost(); this.transport.disconnect(); } else this.connect(); } }
  static hello(mode: "IN_PROCESS"|"OUT_OF_PROCESS", messageId="msg:00001"): HarnessEnvelope { return {protocol:"axiom-platform-harness-exec-v1",protocolVersion:1,messageId:asMessageId(messageId),messageType:"HELLO",sessionId:asSessionId("session:001"),sessionEpoch:asTaggedU64("u64:0000000000000000"),payload:{adapterInstanceId:"adapter:scripted:001",supportedProtocolVersions:[1],boundaryMode:mode}}; }
}
