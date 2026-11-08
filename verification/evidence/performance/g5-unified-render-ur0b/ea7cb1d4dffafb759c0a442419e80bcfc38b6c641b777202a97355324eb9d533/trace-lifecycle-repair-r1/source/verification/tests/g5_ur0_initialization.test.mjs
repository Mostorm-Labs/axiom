import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {decodeTrace} from '../runners/g5/ur0/decode_trace.mjs';
import {compileMetrics} from '../runners/g5/ur0/metrics_compiler.mjs';
import {canonicalChainProof} from '../runners/g5/ur0/canonical_chain.mjs';
const root=path.resolve(import.meta.dirname,'../..');
const inputs=JSON.parse(fs.readFileSync(path.join(root,'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json')));
const entry=inputs.inputs.find(r=>r.run_id==='realtime-60'),directory=path.join(root,entry.input_directory);
const read=name=>fs.readFileSync(path.join(directory,name));
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
const rawBytes=read('raw-trace.jsonl'),runBytes=read('worker-result.json'),inputBytes=read('run-inputs.json');
for(const ref of entry.immutable_inputs)assert.equal(digest(fs.readFileSync(path.join(root,ref.path))),ref.sha256);
const actualRaw=rawBytes.toString().trim().split('\n').map(JSON.parse),actualRun=JSON.parse(runBytes),actualInputs=JSON.parse(inputBytes);
const actualRecording={run:actualRun,raw_trace_jsonl:rawBytes.toString(),worker_result_json:runBytes.toString(),run_inputs_json:inputBytes.toString(),input_refs:{raw_trace_sha256:digest(rawBytes),worker_result_sha256:digest(runBytes),run_inputs_sha256:digest(inputBytes)}};
const prefix=()=>structuredClone(actualRaw.filter(r=>BigInt(r.seq)<=55n));
const modelRun=()=>({init:structuredClone(actualRun.init),source_before:{source_tree_sha256:actualRun.source_before.source_tree_sha256,files:Object.fromEntries(['verification/runners/g5/ur0/consumer.mjs','verification/runners/g5/ur0/worker.mjs'].map(n=>[n,structuredClone(actualRun.source_before.files[n])]))},trace:{loss:{critical:0,statistical:0}}});
function recording(raw,run=modelRun()){
 const raw_trace_jsonl=raw.map(r=>JSON.stringify(r)+'\n').join(''),worker_result_json=JSON.stringify(run),run_inputs_json=JSON.stringify(run.mode==='correctness'?{schema:run.schema,mode:run.mode,branch:run.branch,toggle:run.toggle,source_before:run.source_before,artifacts_before:run.artifacts_before}:actualInputs);
 return {run,raw_trace_jsonl,worker_result_json,run_inputs_json,input_refs:{raw_trace_sha256:digest(raw_trace_jsonl),worker_result_sha256:digest(worker_result_json),run_inputs_sha256:digest(run_inputs_json)}};
}
function analyze(raw,context=recording(raw),document=null,loss={critical:0,statistical:0}){
 const decoded=decodeTrace(raw,loss,context);return {decoded,proof:canonicalChainProof(raw,decoded,document,context)};
}
let actualCompiled;
function compiledActual(){return actualCompiled??=compileMetrics(actualRun,actualRaw,{run_id:'realtime-60',row_id:'realtime-60',repetition:1,fixture_sha256:actualInputs.fixture_sha256,recording:actualRecording});}
test('ACTUAL-RT60: full initial native frame plus first committed stroke reconciles without a second mutation receipt',()=>{
 const compiled=compiledActual();
 const initial=compiled.decoded.frames.find(f=>f.work==='2');
 assert.equal(initial.semanticGeneration,'0');assert.equal(initial.operation,null);
 assert.equal(initial.draw_seq,'15');assert.equal(initial.provider_seq,'32');assert.equal(initial.capture_seq,'42');
 assert.equal(actualRun.init.state.objects,0);assert.equal(actualRun.init.state.target.runtimeEpoch,initial.documentEpoch);
 assert.equal(actualRun.init.state.target.attachmentEpoch,initial.attachment);
 const proof=canonicalChainProof(actualRaw,compiled.decoded,compiled.document,actualRecording);
 assert.equal(proof.status,'QUALIFIED','complete actual init+commit evidence must qualify; old all-raw committed-only equality is MISSING');
 assert.equal(proof.schema,'g5-ur0b-canonical-chain-proof-v2');
 assert.equal(proof.accepted_raw_count,2);assert.equal(proof.qualified_initialization_count,1);
 assert.equal(proof.qualified_committed_count,1);assert.equal(proof.unresolved_accepted_count,0);
 assert.equal(proof.metric_receipt_count,1);assert.equal(compiled.document.interactions.flatMap(i=>i.canonical_receipts).length,1);
});
test('INIT-ONLY and NATIVE-ONLY: initialization is a lifecycle population with no mutation receipt or measurement.before',()=>{
 const rows=prefix(),context=recording(rows);delete context.run.measurement;
 const {decoded,proof}=analyze(rows,context);
 assert.equal(proof.status,'QUALIFIED');assert.equal(proof.scope,'NATIVE_ONLY');assert.equal(proof.metric_receipt_count,null);
 assert.equal(proof.qualified_initialization_count,1);assert.equal(proof.qualified_committed_count,0);assert.equal(decoded.qualified_completions.length,0);
 assert.equal(proof.initialization_chains[0].basis.operation_id,null);
 assert.equal(proof.initialization_chains[0].operation_reason,'NOT_APPLICABLE_INITIALIZATION');
 assert.equal(decoded.commits.length,0);assert.equal(decoded.scenes.length,0);
 const withMetrics=analyze(rows,context,{interactions:[]}).proof;assert.equal(withMetrics.status,'QUALIFIED');assert.equal(withMetrics.metric_receipt_count,0);
});
test('MISSING-INIT-ANCHOR: generation zero without complete state, recording profile and input binding is unresolved',()=>{
 const changes=[c=>delete c.run.init.state,c=>delete c.run.source_before,c=>delete c.input_refs,c=>{c.input_refs.worker_result_sha256='f'.repeat(64);},c=>{c.run.init.state.documentId='a'.repeat(32);}];
 for(const change of changes){const rows=prefix(),context=recording(rows);change(context);const p=analyze(rows,context).proof;assert.equal(p.status,'MISSING');assert.equal(p.unresolved_accepted_count,1);}
});
test('RESTORED-OR-NONEMPTY and CONTRADICTORY-GENERATION: a consistently bound but inapplicable public init is rejected',()=>{
 const changes=[r=>{delete r.init.state;},r=>{delete r.source_before;},r=>{r.init.state.objects=1;},r=>{r.init.state.generation='1';},r=>{r.init.state.lastCommit.ordinal='1';},r=>{r.source_before.files['verification/runners/g5/ur0/consumer.mjs'].sha256='f'.repeat(64);},r=>{r.init.state.target.runtimeEpoch='0';},r=>{r.init.state.target.attachmentEpoch='3';},r=>{r.init.state.target.viewId='2';},r=>{r.init.state.lastCommit.operationId='1'.repeat(32);}];
 for(const change of changes){const rows=prefix(),run=modelRun();change(run);const p=analyze(rows,recording(rows,run)).proof;assert.equal(p.status,'MISSING');assert.equal(p.qualified_initialization_count,0);}
});
test('CONTRADICTORY-GENERATION: a real matching-owner commit or Scene at/before capture defeats initialization',()=>{
 for(const kind of ['commit','scene']){
  const rows=prefix(),native=structuredClone(actualRaw.find(r=>r.kind===(kind==='commit'?6:7)&&r.tag===(kind==='commit'?27:9)));
  const owner=structuredClone(actualRaw.find(r=>r.kind===5&&r.tag===1&&r.job===native.job&&r.basis===native.basis));
  owner.seq='100001';native.seq='100002';native.at_ns='335200000';rows.push(owner,native);
  const p=analyze(rows).proof;assert.equal(p.status,'MISSING');assert.equal(p.qualified_initialization_count,0);
 }
});
test('LOSS-OR-AMBIGUITY: an unknown commit time or duplicated native provenance cannot prove initial emptiness',()=>{
 const rows=prefix(),commit=structuredClone(actualRaw.find(r=>r.kind===6&&r.tag===27)),owner=structuredClone(actualRaw.find(r=>r.kind===5&&r.tag===1&&r.job===commit.job&&r.basis===commit.basis));
 commit.at_ns='unknown';rows.push(owner,commit);assert.equal(analyze(rows).proof.status,'MISSING');
 const duplicated=prefix();duplicated.push(structuredClone(duplicated.find(r=>r.seq==='39')));assert.equal(analyze(duplicated).proof.status,'MISSING');
});
test('FOREIGN-OWNER-OR-TARGET: every independent native owner/frame stamp and successful job must match',()=>{
 const edits=[...[1,2,3,4,5,6,8,9,10].map(i=>['41','values',i]),...[1,2,3,4,5,6].map(i=>['42','values',i]),...[2,3,4,5,6,7,8,9,10].map(i=>['47','values',i]),['15','job'],['32','basis']];
 for(const [seq,key,index] of edits){const rows=prefix(),r=rows.find(r=>r.seq===seq);if(key==='values')r.values[index]='999';else r[key]='999';const p=analyze(rows).proof;assert.equal(p.status,'MISSING',`${seq}/${key}/${index}`);assert.equal(p.qualified_initialization_count,0);}
});
test('DUPLICATE-OR-OMITTED: repeated accepted work/sequence or omitted decoded member cannot improve the denominator',()=>{
 for(const duplicateSeq of [true,false]){const rows=prefix(),copy=structuredClone(rows.find(r=>r.seq==='47'));if(!duplicateSeq)copy.seq='100000';rows.push(copy);const p=analyze(rows).proof;assert.equal(p.status,'MISSING');assert.equal(p.accepted_raw_count,2);assert.ok(p.unresolved_accepted_count>0);}
 const rows=prefix(),context=recording(rows),d=decodeTrace(rows,{critical:0,statistical:0},context);d.initialization_completions=[];
 const p=canonicalChainProof(rows,d,null,context);assert.equal(p.status,'MISSING');assert.equal(p.accepted_raw_count,1);assert.equal(p.unresolved_accepted_count,1);
});
test('NONZERO-WITHOUT-SCENE: initial state never repairs an unproved nonzero canonical frame',()=>{
 const rows=prefix();for(const r of rows){if(r.seq==='15'){r.values[2]='1';r.values[3]='1';}if(r.seq==='32'){r.values[2]='1';r.values[3]='1';}if(r.seq==='41'){r.values[7]='1';r.values[9]='1';}if(r.seq==='42'){r.values[3]='1';r.values[4]='1';}if(['46','47'].includes(r.seq)){r.values[7]='1';r.values[8]='1';}}
 const p=analyze(rows).proof;assert.equal(p.status,'MISSING');assert.equal(p.qualified_initialization_count,0);assert.equal(p.qualified_committed_count,0);
});
test('MISSING-MUTATION-RECEIPT: an initialization chain cannot replace a missing or foreign committed receipt',()=>{
 const compiled=compiledActual();
 for(const foreign of [false,true]){const doc=structuredClone(compiled.document);for(const i of doc.interactions){if(foreign){for(const r of i.canonical_receipts)r.basis.operation_id='f'.repeat(32);}else i.canonical_receipts=[];}
  const p=canonicalChainProof(actualRaw,compiled.decoded,doc,actualRecording);assert.equal(p.status,'MISSING');assert.equal(p.qualified_initialization_count,1);assert.equal(p.unresolved_accepted_count,1);
 }
});
test('LOSS-OR-AMBIGUITY: loss, repeated provider or ambiguous draw remains a blocker',()=>{
 const rows=prefix();assert.equal(analyze(rows,recording(rows),null,{critical:1,statistical:0}).proof.status,'MISSING');
 for(const seq of ['15','32']){const raw=prefix(),r=structuredClone(raw.find(r=>r.seq===seq));r.seq='100000';raw.splice(raw.findIndex(x=>x.seq===seq),0,r);assert.equal(analyze(raw).proof.status,'MISSING');}
});
test('NEXT-INTERACTION: delayed initial completion cannot confirm a later mutation or interaction receipt',()=>{
 const rows=prefix(),accepted=rows.find(r=>r.seq==='47');accepted.at_ns='31000000000';accepted.seq='100003';
 const commit=structuredClone(actualRaw.find(r=>r.kind===6&&r.tag===27)),owner=structuredClone(actualRaw.find(r=>r.kind===5&&r.tag===1&&r.job===commit.job&&r.basis===commit.basis));
 rows.push(owner,commit);const {proof,decoded}=analyze(rows);
 assert.equal(proof.status,'QUALIFIED');assert.equal(proof.qualified_initialization_count,1);assert.equal(proof.qualified_committed_count,0);assert.equal(decoded.qualified_completions.length,0);assert.equal(proof.metric_receipt_count,null);
});
test('OFF-NO-OBSERVATIONS: initial public state cannot invent any native observation',()=>{
 const {proof}=analyze([],recording([]),{interactions:[]});assert.equal(proof.status,'NOT_OBSERVED');assert.equal(proof.accepted_raw_count,0);assert.equal(proof.qualified_initialization_count,0);assert.equal(proof.qualified_committed_count,0);
});
const repairedFiles=['consumer.mjs','worker.mjs','correctness_driver.mjs','pixel_oracle.mjs','stream_transfer.mjs','correctness_transfer.mjs','correctness_contract.mjs'].map(n=>'verification/runners/g5/ur0/'+n);
function repairedRun(){
 const run=modelRun();Object.assign(run,{schema:'g5-ur0b-worker-v1',mode:'correctness',branch:'lower_z_translucent_drag',toggle:'ON',status:'OBSERVED',source_stable:true,artifacts_stable:true,close:{closed:true}});
 run.source_before.files=Object.fromEntries(repairedFiles.map(n=>[n,{sha256:digest(fs.readFileSync(path.join(root,n)))}]));run.source_after=structuredClone(run.source_before);
 run.artifacts_before={'axiom.wasm':'a'.repeat(64)};run.artifacts_after=structuredClone(run.artifacts_before);
 run.init.environment={timeOrigin:1000};run.paired_init=structuredClone(run.init);run.paired_init.environment.timeOrigin=2000;
 run.trace.records=prefix().length;
 run.correctness=['OFF','ON'].flatMap(toggle=>[0,1].map(step=>({case_id:run.branch,toggle,step,source_sha256:'a'.repeat(64),candidate_sha256:'a'.repeat(64),reference_snapshot_sha256:'b'.repeat(64),comparison:{status:'QUALIFIED'}})));
 return run;
}
test('CREATION-PROFILES: exactly pinned repaired correctness worker qualifies, unknown consumer/worker/helper never does',()=>{
 const rows=prefix(),run=repairedRun();assert.equal(analyze(rows,recording(rows,run)).proof.status,'QUALIFIED','approved repaired worker profile missing');
 for(const n of repairedFiles){for(const change of [r=>r.source_before.files[n].sha256='f'.repeat(64),r=>delete r.source_before.files[n]]){const bad=repairedRun();change(bad);assert.equal(analyze(rows,recording(rows,bad)).proof.status,'MISSING',n);}}
 const bad=repairedRun();bad.mode='unsupported';assert.equal(analyze(rows,recording(rows,bad)).proof.status,'MISSING');
});
test('CREATION-PROFILES: unchanged primary modes keep init-only admission under the exact repaired worker hash',()=>{
 for(const mode of ['fixture','main','realtime','legacy','setup']){
  const rows=prefix(),run=repairedRun();run.mode=mode;assert.equal(analyze(rows,recording(rows,run)).proof.status,'QUALIFIED',mode);
  run.paired_init=structuredClone(run.init);run.init.state.target.viewId='99';assert.equal(analyze(rows,recording(rows,run)).proof.status,'MISSING','primary mode must not use paired_init');
 }
});
test('PAIRED-INIT-OWNERS: repaired reload selects one fully matching state and rejects ambiguity/restored/nonempty/foreign state',()=>{
 const rows=prefix(),run=repairedRun();run.init.state.target.viewId='99';
 const good=analyze(rows,recording(rows,run)).proof;assert.equal(good.status,'QUALIFIED');assert.equal(good.initialization_chains[0].initialization_basis.state_path,'worker-result.json/paired_init/state');
 for(const change of [r=>r.init.environment.timeOrigin=r.paired_init.environment.timeOrigin,r=>delete r.paired_init,r=>r.paired_init.state.objects=1,r=>r.paired_init.state.generation='1',r=>r.paired_init.state.lastCommit.operationId='f'.repeat(32),r=>r.paired_init.state.target.runtimeEpoch='99',r=>r.paired_init.state.documentId='b'.repeat(32)]){
  const bad=structuredClone(run);change(bad);const p=analyze(rows,recording(rows,bad)).proof;assert.equal(p.status,'MISSING');assert.equal(p.unresolved_accepted_count,1);
 }
});
const lifecycleInputs=JSON.parse(fs.readFileSync(path.join(root,'.aegis/verification/G5-UR0b-trace-lifecycle-v1/evidence-inputs.json')));
const actualR5Directory=path.dirname(path.join(root,lifecycleInputs.r5_worker_inputs[0].path));
let r5Context;
function actualR5(){
 if(r5Context)return r5Context;
 const raw_trace_jsonl=fs.readFileSync(path.join(actualR5Directory,'raw-trace.jsonl'),'utf8'),worker_result_json=fs.readFileSync(path.join(actualR5Directory,'worker-result.json'),'utf8'),run_inputs_json=fs.readFileSync(path.join(actualR5Directory,'run-inputs.json'),'utf8');
 r5Context={run:JSON.parse(worker_result_json),raw_trace_jsonl,worker_result_json,run_inputs_json,input_refs:{raw_trace_sha256:digest(raw_trace_jsonl),worker_result_sha256:digest(worker_result_json),run_inputs_sha256:digest(run_inputs_json)}};return r5Context;
}
test('LIFECYCLE ACTUAL-SCOPED-POSITIVE: immutable actual r5 reconciles6=init1+committed5 with native-only scope',()=>{
 const context=actualR5(),raw=context.raw_trace_jsonl.trim().split('\n').map(JSON.parse),{proof,decoded}=analyze(raw,context);
 assert.equal(proof.status,'QUALIFIED');assert.equal(proof.accepted_raw_count,6);assert.equal(proof.qualified_initialization_count,1);assert.equal(proof.qualified_committed_count,5);assert.equal(proof.unresolved_accepted_count,0);assert.equal(proof.scope,'NATIVE_ONLY');assert.equal(proof.metric_receipt_count,null);assert.deepEqual(decoded.missing,[]);
 const old=JSON.parse(fs.readFileSync(path.join(actualR5Directory,'canonical-chain.json')));assert.deepEqual(proof.chains,old.chains);
 assert.deepEqual(context.run.init.state,context.run.paired_init.state);
 assert.deepEqual(proof.initialization_chains[0].initialization_basis.trace_scope,{scheme:'CORRECTNESS_FINAL_ON_PAGE_V1',qualifying_state_path:'worker-result.json/paired_init/state',off_page_time_origin_ms:lifecycleInputs.actual_off_time_origin_ms,trace_page_time_origin_ms:lifecycleInputs.actual_on_time_origin_ms,verification_spec_ref:{path:'.aegis/verification/G5-UR0b-trace-lifecycle-v1/verification-spec.json',sha256:'bd6e1d2f36f7e20317a326fc64ff9856523927cb054acc2ec443cbc18052ed5a'}});
});
test('LIFECYCLE CROSS-PAGE-ALIAS: equal state values remain separate page provenance entries',()=>{
 const run=repairedRun(),rows=prefix(),p=analyze(rows,recording(rows,run)).proof;assert.equal(p.status,'QUALIFIED');
 const o=p.initialization_chains[0].initialization_basis;assert.equal(o.state_path,'worker-result.json/paired_init/state');assert.equal(o.recorded_states.length,2);assert.deepEqual(o.recorded_states[0].public_state,o.recorded_states[1].public_state);assert.notEqual(o.recorded_states[0].page_time_origin_ms,o.recorded_states[1].page_time_origin_ms);
});
test('LIFECYCLE UNKNOWN-SCOPE: incomplete protocol/profile/origins/pair/input or trace never selects ON by convenience',()=>{
 const changes=[r=>delete r.paired_init,r=>delete r.paired_init.environment,r=>delete r.init.environment,r=>r.paired_init.environment.timeOrigin=1000,r=>r.paired_init.environment.timeOrigin=500,r=>r.paired_init.environment.timeOrigin=Infinity,r=>r.paired_init.environment.timeOrigin='2000',r=>r.toggle='OFF',r=>r.branch='foreign',r=>r.status='FAILED',r=>r.source_stable=false,r=>r.artifacts_stable=false,r=>r.source_after.source_tree_sha256='f'.repeat(64),r=>r.artifacts_after['axiom.wasm']='f'.repeat(64),r=>r.trace.records++,r=>r.trace.loss.critical=1,r=>r.correctness.pop(),r=>r.correctness.push(structuredClone(r.correctness[0])),r=>r.correctness[3].step=0,r=>r.correctness[0].case_id='foreign',r=>r.correctness[0].comparison.status='unknown',r=>delete r.correctness[0].candidate_sha256,r=>r.close.closed=false];
 for(const change of changes){const run=repairedRun();change(run);assert.equal(analyze(prefix(),recording(prefix(),run)).proof.status,'MISSING',String(change));}
 const context=recording(prefix(),repairedRun()),inputs=JSON.parse(context.run_inputs_json);inputs.toggle='OFF';context.run_inputs_json=JSON.stringify(inputs);context.input_refs.run_inputs_sha256=digest(context.run_inputs_json);assert.equal(analyze(prefix(),context).proof.status,'MISSING');
});
test('LIFECYCLE WITHIN-SCOPE-IDENTITY: matching OFF never substitutes for foreign/nonempty/nonzero ON',()=>{
 for(const change of [r=>r.paired_init.state.documentId='b'.repeat(32),r=>r.paired_init.state.target.runtimeEpoch='99',r=>r.paired_init.state.target.attachmentEpoch='99',r=>r.paired_init.state.target.viewId='99',r=>r.paired_init.state.objects=1,r=>r.paired_init.state.generation='1',r=>r.paired_init.state.lastCommit.ordinal='1',r=>r.paired_init.state.lastCommit.operationId='f'.repeat(32)]){
  const run=repairedRun();change(run);const p=analyze(prefix(),recording(prefix(),run)).proof;assert.equal(p.status,'MISSING');assert.equal(p.qualified_initialization_count,0);
 }
});
test('LIFECYCLE OUTCOME-INDEPENDENCE: complete FAILED/MISSING pixels can resolve provenance but never change their outcomes',()=>{
 for(const status of ['FAILED','MISSING']){const run=repairedRun();for(const e of run.correctness)e.comparison.status=status;const context=recording(prefix(),run);const p=analyze(prefix(),context).proof;assert.equal(p.status,'QUALIFIED');assert.equal(p.qualified_initialization_count,1);assert.ok(context.run.correctness.every(e=>e.comparison.status===status));}
});
