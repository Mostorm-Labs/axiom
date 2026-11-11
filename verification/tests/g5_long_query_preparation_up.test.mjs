import test from 'node:test';import assert from 'node:assert/strict';
import vm from 'node:vm';import {webcrypto,createHash} from 'node:crypto';
import {webConsumer} from '../runners/g5/long_query_preparation_up.mjs';
import {sendBinaryChunks} from '../runners/g5/ur2d_selection_up_contract.mjs';
async function run({failUp=false,closeError=false}={}){
 let clock=0,generation=0,contacts=0,ups=0,selection=0;const events=[],buffers=[],metadata=[],traces=[];
 const state=()=>({objects:40,activeContacts:contacts,generation:String(generation),canUndo:true,camera:{scale:1,x:0,y:0},lastCommit:{presentedGeneration:String(generation)}});
 const sdk={canvas:{getBoundingClientRect:()=>({left:0,top:0})},readState:state,selectTool:async()=>{},setEraser:async()=>{},readSelection:()=>({count:selection}),captureSnapshot:async()=>{assert.equal(contacts,0);return{bytes:Uint8Array.of(1,2,3)};},undo:async()=>{++generation;},redo:async()=>{++generation;},close:async()=>{if(closeError)throw Error('close BUSY');}};
 const observer={begin:()=>{},end:()=>{},restore:()=>{},activity:()=>({pending:0}),dispatch:(_,e)=>{const phase=e.type.slice(7);events.push(phase);if(phase==='down')contacts=1;if(phase==='up'){++ups;if(failUp&&ups===1)throw Error('forced firstUp');contacts=0;++generation;}if(phase==='cancel')contacts=0;}};
 const modules={'/sdk/axiom.mjs':{createRuntime:async()=>sdk},'/g5/navigation_observer.mjs':{installNavigationObserver:()=>observer},'/g5/webgl_target_readback.mjs':{readWebGLTarget:()=>({metadata:{status:'VALID'},bytes:Uint8Array.of(255,255,255,255)})},'/g5/ur2d_selection_up_contract.mjs':{sendBinaryChunks},'/g5/exact_query_working_set_contract.mjs':{typedWitness:()=>{},qualifyWitness:()=>{},navigationWitness:()=>{}},'/g5/erase_exact_canonical_contract.mjs':{observeCorrectFeedback:()=>{}},'/g5/exact_fixed_scene_contract.mjs':{emptyViewPoint:()=>({view:[24,986]})}};
 const context=vm.createContext({Uint8Array,BigInt,crypto:webcrypto,performance:{now:()=>++clock},setTimeout:fn=>fn(),btoa:s=>Buffer.from(s,'binary').toString('base64'),fetch:async()=>({arrayBuffer:async()=>Uint8Array.of(9).buffer}),document:{getElementById:()=>({})},PointerEvent:class{constructor(type,props){this.type=type;Object.assign(this,props);}},window:{waTrace:{enable:()=>{},take:()=>[{records:[]}]},hpTraceChunk:async v=>traces.push(v),waChunk:async v=>{buffers.push(v.name);return{nextOffset:v.offset+Buffer.from(v.base64,'base64').length};},waMetadata:async v=>metadata.push(structuredClone(v))},exactFixtureEntropy:{transcript:()=>({seed:324508639})}});
 const mod=new vm.SourceTextModule('export '+webConsumer.toString(),{context,importModuleDynamically:async name=>{assert(modules[name],name);const exports=modules[name],m=new vm.SyntheticModule(Object.keys(exports),function(){for(const[k,v]of Object.entries(exports))this.setExport(k,v);},{context});await m.link(()=>{});await m.evaluate();return m;}});await mod.link(()=>{});await mod.evaluate();
 const fixture={sha256:createHash('sha256').update(Uint8Array.of(9)).digest('hex'),metadata:{documentRevision:'42',recoveryFrontier:[]}};
 const r=await mod.namespace.webConsumer({mode:'growth',diagnostic:true,fixture});return{r,events,buffers,metadata,traces};
}
test('actual normal/diagnostic consumer preserves original ten main/short waveform',async()=>{
 const {r,events,buffers,traces}=await run();assert.equal(r.status,'OBSERVED');assert.equal(r.rounds.length,10);assert.equal(r.rounds[9].events.length,160);assert.equal(events.filter(v=>v==='down').length,21);assert(buffers.includes('initial.axsnap'));assert(buffers.includes('redo.rgba'));assert(traces.length>0);
});
test('actual Up failure preserves full frame and original state before independent close error',async()=>{
 const {r,buffers,metadata}=await run({failUp:true,closeError:true});assert.equal(r.status,'FAILED');assert.match(r.error,/forced firstUp/);assert.equal(r.failureState.activeContacts,1);assert(buffers.includes('failure.rgba'));assert.equal(r.failureReadback.status,'VALID');assert.match(r.cleanupError,/BUSY/);assert.equal(metadata.length,1);
});
