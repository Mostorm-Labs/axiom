import { mkdir, readFile, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { asActionId, asCompletionTokenId, asSourceId, asTaggedU64 } from "../packages/platform-harness-protocol/dist/index.js";
import { ReferencePlatformRunner } from "../packages/platform-harness-runner/dist/index.js";
import { InProcessTransport, SerializedLoopbackTransport } from "../packages/platform-harness-transport/dist/index.js";
import { ScriptedAdapter, MalformedScriptCatalog } from "../packages/platform-harness-scripted-adapter/dist/index.js";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const suite = JSON.parse(await readFile(resolve(root, "platform/protocol/v1/suites/protocol-seed-v0.1.json"), "utf8"));
const evidenceRoot = process.env.AXIOM_PROTOCOL_EVIDENCE_ROOT ? resolve(process.env.AXIOM_PROTOCOL_EVIDENCE_ROOT) : resolve(root, "evidence/g0/gt-g0-06");
const outputRoot = resolve(evidenceRoot, "protocol-meta-results");
const mutation = process.env.AXIOM_PROTOCOL_MUTATION ?? "";
const evidenceMutation = process.env.AXIOM_PROTOCOL_EVIDENCE_MUTATION ?? "";
const boundaries = (process.env.AXIOM_PROTOCOL_BOUNDARIES ?? "IN_PROCESS,OUT_OF_PROCESS").split(",").filter(Boolean);
if (boundaries.some((value) => !["IN_PROCESS","OUT_OF_PROCESS"].includes(value))) throw new Error("unsupported boundary mode");
const source = asSourceId("source:touch");
const error = (fn) => { try { fn(); return null; } catch (value) { return value; } };

function setup(boundary) {
  const runner = new ReferencePlatformRunner();
  const transport = boundary === "IN_PROCESS" ? new InProcessTransport() : new SerializedLoopbackTransport();
  const adapter = new ScriptedAdapter(runner, transport); adapter.connect();
  adapter.run({ steps: [{ kind: "SEND", location: "vector.hello", envelope: ScriptedAdapter.hello(boundary) }] });
  runner.openSession("session:001", "u64:0000000000000001", "adapter:scripted:001");
  return { runner, adapter };
}

function execute(vector, boundary) {
  const action = vector.input.focusAreas.at(-1); const { runner, adapter } = setup(boundary);
  let observed = "PASS"; let firstDivergence = null;
  const reject = (fn) => { const thrown = error(fn); if (!thrown) { observed = "FAIL"; } else { observed = "REJECTED"; firstDivergence = runner.checkpoint().firstDivergence; } };
  const request = (id="action:001") => ({ actionId:asActionId(id), stepId:"step:001", commandSeq:asTaggedU64("u64:0000000000000000"), completionMode:"WAIT_FOR_ACTION_COMPLETION", action:"render", parameters:{} });
  const completion = (overrides={}) => ({ tokenId:asCompletionTokenId("completion:001"), actionId:asActionId("action:001"), outcome:"SUCCEEDED", artifacts:[], ...overrides });
  const prepareCompletion = () => { const d=runner.dispatchAction(request()); runner.receiveReceipt({ actionId:asActionId("action:001"), commandSeq:d.commandSeq, receiptStatus:"DISPATCHED", tokenId:asCompletionTokenId("completion:001"), terminalOutcome:null }); return d; };
  if (action === "hello-version-reject") { const fresh = new ReferencePlatformRunner(); fresh.connect(boundary); reject(() => fresh.acceptHello({ ...ScriptedAdapter.hello(boundary).payload, supportedProtocolVersions: [2] }, "vector.hello.version")); }
  else if (action.includes("epoch-zero")) reject(() => runner.openSession("session:002", "u64:0000000000000000", "adapter:scripted:001"));
  else if (action.includes("epoch-reuse")) { runner.finalize(); reject(() => runner.openSession("session:002", "u64:0000000000000001", "adapter:scripted:001")); }
  else if (action.includes("stale-epoch")) reject(() => runner.receiveEnvelope(new TextEncoder().encode(JSON.stringify({ protocol:"axiom-platform-harness-exec-v1", protocolVersion:1, messageId:"msg:00010", messageType:"SOURCE_ATTEMPT", sessionId:"session:001", sessionEpoch:"u64:0000000000000000", payload:{ sourceId:"source:touch" } }))));
  else if (action.includes("malformed-json")) reject(() => runner.receiveEnvelope(new TextEncoder().encode("not-json"), "vector.malformed"));
  else if (action.includes("unknown-message")) reject(() => runner.receiveEnvelope(new TextEncoder().encode(JSON.stringify({ protocol:"axiom-platform-harness-exec-v1", protocolVersion:1, messageId:"msg:00010", messageType:"UNKNOWN", sessionId:"session:001", sessionEpoch:"u64:0000000000000001", payload:{} })), "vector.unknown-message"));
  else if (action.includes("duplicate-completion")) { prepareCompletion(); runner.receiveCompletion(completion()); reject(() => runner.receiveCompletion(completion())); }
  else if (action.includes("command-seq")) { runner.dispatchAction(request()); reject(() => runner.receiveReceipt({ actionId:asActionId("action:001"), commandSeq:asTaggedU64("u64:0000000000000009"), receiptStatus:"DISPATCHED", tokenId:asCompletionTokenId("completion:001"), terminalOutcome:null })); }
  else if (action === "sync-terminal-reject") { const d=runner.dispatchAction(request()); reject(() => runner.receiveReceipt({ actionId:asActionId("action:001"), commandSeq:d.commandSeq, receiptStatus:"COMPLETED_SYNC", tokenId:null, terminalOutcome:null })); }
  else if (action === "completed-sync-terminal") { const d=runner.dispatchAction(request()); runner.receiveReceipt({ actionId:asActionId("action:001"), commandSeq:d.commandSeq, receiptStatus:"COMPLETED_SYNC", tokenId:null, terminalOutcome:"SUCCEEDED" }); runner.finalize(); }
  else if (action.includes("unknown-token")) reject(() => runner.receiveCompletion(completion()));
  else if (action.includes("duplicate-token")) { prepareCompletion(); runner.receiveCompletion(completion()); reject(() => runner.receiveCompletion(completion())); }
  else if (action.includes("action-mismatch")) { prepareCompletion(); reject(() => runner.receiveCompletion(completion({actionId:asActionId("action:002")}))); }
  else if (action.includes("duplicate-action")) { runner.dispatchAction({ actionId:asActionId("action:001"), stepId:"step:001", commandSeq:asTaggedU64("u64:0000000000000000"), completionMode:"DISPATCH_ONLY", action:"render", parameters:{} }); reject(() => runner.dispatchAction({ actionId:asActionId("action:001"), stepId:"step:001", commandSeq:asTaggedU64("u64:0000000000000000"), completionMode:"DISPATCH_ONLY", action:"render", parameters:{} })); }
  else if (action.includes("missing-token")) { const d=runner.dispatchAction({ actionId:asActionId("action:001"), stepId:"step:001", commandSeq:asTaggedU64("u64:0000000000000000"), completionMode:"WAIT_FOR_ACTION_COMPLETION", action:"render", parameters:{} }); reject(() => runner.receiveReceipt({ actionId:asActionId("action:001"), commandSeq:d.commandSeq, receiptStatus:"DISPATCHED", tokenId:null, terminalOutcome:null })); }
  else if (action.includes("source") || action.includes("lease") || action.includes("event") || action.includes("attempt")) {
    if (action.includes("duplicate")) { runner.openSource(source); reject(() => runner.openSource(source)); }
    else if (action.includes("unknown")) reject(() => runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event:"POINTER", data:{} }));
    else if (action.includes("closed")) { runner.openSource(source); runner.closeSource(source); reject(() => runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event:"POINTER", data:{} })); }
    else { runner.openSource(source); if (action.includes("leak")) reject(() => runner.finalize()); else { runner.advanceFence(asTaggedU64("u64:0000000000000001")); runner.receiveEvent(source, asTaggedU64("u64:0000000000000001"), { event:"POINTER", data:{x:1} }); runner.closeSource(source); runner.finalize(); } }
  } else if (action.includes("fault")) {
    if (action.includes("unknown") || action.includes("mismatch")) reject(() => runner.setFault("SURFACE_LOST", false));
    else { runner.setFault("SURFACE_LOST", true); if (action.includes("active") || action.includes("reject")) reject(() => runner.finalize()); else { runner.setFault("SURFACE_LOST", false); runner.finalize(); } }
  } else if (action.includes("stale-generation")) { runner.openSource(source); runner.advanceFence(asTaggedU64("u64:0000000000000001")); reject(() => runner.receiveEvent(source, asTaggedU64("u64:0000000000000000"), { event:"POINTER", data:{} })); }
  else if (action.includes("generation-rewind")) { runner.advanceFence(asTaggedU64("u64:0000000000000002")); reject(() => runner.advanceFence(asTaggedU64("u64:0000000000000001"))); }
  else if (action.includes("nonportable") || action.includes("malformed-line")) { reject(() => adapter.run({ steps: [{ kind:"INJECT", location:"vector.portability", bytes:new TextEncoder().encode("{}") }] })); }
  else if (action.includes("disconnect")) { adapter.run({ steps:[{kind:"DISCONNECT"}] }); observed = runner.checkpoint().session.state === "CLOSED" ? "DISCONNECT" : "FAIL"; }
  else if (action.includes("reconnect")) { adapter.run({ steps:[{kind:"DISCONNECT"},{kind:"RECONNECT"}] }); observed = runner.checkpoint().session.state === "CLOSED" ? "RENEGOTIATE" : "FAIL"; }
  else { runner.finalize(); }
  const expectedStatus = vector.expected.terminal === "REJECTED" ? "REJECTED" : vector.expected.oracle === "DISCONNECT" ? "DISCONNECT" : vector.expected.oracle === "RENEGOTIATE" ? "RENEGOTIATE" : "PASS";
  const mutationTargets = {"duplicate-completion":"duplicate-completion-reject","stale-epoch":"stale-epoch-reject","lease-leak":"lease-leak","fault-transition":"fault-pulse-mismatch","late-fence":"stale-generation-reject","transport-drop":"transport-disconnect","boundary-equivalence":"boundary-equivalence"};
  if (mutationTargets[mutation] === action && (mutation !== "boundary-equivalence" || boundary === "OUT_OF_PROCESS")) observed = expectedStatus === "REJECTED" ? "PASS" : "REJECTED";
  const status = observed === expectedStatus ? (observed === "REJECTED" ? "REJECTED" : "PASS") : "FAIL";
  return { format:"axiom-platform-protocol-meta-result-v1", formatVersion:1, vectorId:vector.id, boundaryMode:boundary, status, checks:[{name:"contract-oracle", status:status === "FAIL" ? "FAIL" : "PASS"},{name:"first-divergence", status:firstDivergence ? "PASS" : "NOT_APPLICABLE"}], firstDivergence:firstDivergence ?? null };
}

const results=[];
for (const ref of suite.vectorRefs) { const vector=JSON.parse(await readFile(resolve(root, ref.replace(/^verification\//,"")), "utf8")); for (const boundary of vector.input.boundaryModes.filter((value)=>boundaries.includes(value))) { const result=execute(vector,boundary); results.push(result); const target=resolve(outputRoot, boundary, `${vector.id}.json`); await mkdir(dirname(target), {recursive:true}); await writeFile(target, `${JSON.stringify(result)}\n`); } }
const lines=results.map((r)=>JSON.stringify(r)).join("\n")+"\n"; const manifest={format:"axiom-platform-protocol-corpus-integrity-v1",suiteId:suite.id,resultCount:results.length,sha256:createHash("sha256").update(lines).digest("hex"),results:results.map((r)=>({vectorId:r.vectorId,boundaryMode:r.boundaryMode,status:r.status}))}; await mkdir(evidenceRoot,{recursive:true}); if(evidenceMutation !== "missing-integrity") await writeFile(resolve(evidenceRoot,"corpus-integrity.json"),`${JSON.stringify(manifest)}\n`); if(results.some((r)=>r.status === "FAIL")) process.exitCode=1; console.log(`protocol vector execution: ${results.filter((r)=>r.status !== "FAIL").length}/${results.length} passed`);
