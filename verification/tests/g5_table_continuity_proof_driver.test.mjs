import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import vm from 'node:vm';
import {createHash,webcrypto} from 'node:crypto';
import {webConsumer,createServer} from '../runners/g5/table_continuity_proof_web.mjs';
import {probeUntilContent,compareExpectedROI} from '../runners/g5/table_continuity_proof_contract.mjs';
import {sendBinaryChunks} from '../runners/g5/ur2d_selection_up_contract.mjs';
import {typedWitness,qualifyWitness} from '../runners/g5/exact_query_working_set_contract.mjs';
const root=new URL('../../',import.meta.url);
const reference=JSON.parse(await fs.readFile(new URL('out/G5-table-continuity-proof-r1/witness.json',root),'utf8'));

async function branch({readMode='white',cancelError=false,closeError=false,deferEvidence=false}={}){
 let time=0,selected=null,shape=null,tool='select',generation=0,contacts=0,down=null,moved=false,objects=40;
 const records=new Map(),buffers=[],calls=[],feedbackSaved=[];
 const shapes={rectangle:{id:'rect',xy:[850,500],w:120,h:80},sticky:{id:'sticky',xy:[1050,500],w:200,h:200},table:{id:reference.objectId,xy:[850,750],w:240,h:96},image:{id:'image',xy:[1150,750],w:80,h:80}};
 const kind=()=>Object.keys(shapes).find(k=>shapes[k].id===selected);
 const state=()=>({activeContacts:contacts,generation:String(generation),objects,camera:{scale:1,x:0,y:0},lastCommit:{presentedGeneration:String(generation)},canUndo:true});
 const selection=()=>{const s=shapes[kind()];return s?{primary:s.id,count:1,generation:String(generation),transform:[1,0,0,1,...s.xy],bounds:[s.xy[0],s.xy[1],s.xy[0]+s.w,s.xy[1]+s.h],appearance:{fill:[1,1,128/255,1]}}:{primary:'',count:0};};
 const grid=reference.grid,rows=reference.rows,columns=reference.columns;
 const table=()=>({available:kind()==='table',objectId:reference.objectId,generation:String(generation),rows,columns,grid});
 const sdk={canvas:{getBoundingClientRect:()=>({left:0,top:0})},readState:state,readSelection:selection,
  appearance:{setDefaults:async()=>{},readDefaults:()=>({enabled:false}),read:()=>({properties:[],stamp:{owner:'runtime'}})},
  selectShape:async v=>{shape=v;},setAppearance:async()=>{},selectTool:async v=>{tool=v;shape=null;},setSelectionMode:async()=>{},setEraser:async()=>{},
  importImage:async()=>{++objects;selected='image';return{objectId:'image'};},resizeImage:async()=>{},transformSelection:async()=>{},
  readSelectedSticky:()=>({available:true,objectId:'sticky',width:200,height:200}),table:{read:table},
  readSelectedImage:()=>({available:true,objectId:'image',content:{width:80,height:80,resourceId:'red'}}),resources:{token:()=>({}),readBytes:()=>imageBytes},
  captureSnapshot:async()=>{assert.equal(contacts,0);calls.push('snapshot:'+String(generation));return{bytes:Uint8Array.of(1,2,3),throughCommit:{generation:String(generation)}};},
  undo:async()=>{++generation;},redo:async()=>{++generation;},close:async()=>{calls.push('close');if(closeError)throw Error('forced close BUSY');}}
 const imageBytes=Uint8Array.from([0x89,0x50,0x4e,0x47,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130]);
 const observer={begin:()=>{},end:()=>{},restore:()=>{},activity:()=>({pending:0}),dispatch:(_,e)=>{
  const phase=e.type.slice(7),x=e.clientX,y=e.clientY;calls.push(phase);
  if(phase==='down'){contacts=1;down=[x,y];moved=false;if(!shape&&tool==='select'){selected=x<100?'':x<1000?shapes.table.id:x<1150?'sticky':'image';}}
  if(phase==='move'&&contacts)moved=true;
  if(phase==='cancel'){if(cancelError)throw Error('forced cancel failed');contacts=0;}
  if(phase==='up'){
   contacts=0;if(shape){++objects;selected=shapes[shape].id;shape=null;}
   else if(moved&&selected&&tool==='select'){const s=shapes[kind()];s.xy=[s.xy[0]+x-down[0],s.xy[1]+y-down[1]];}
   ++generation;
  }
 }};
 let tableProbeCount=0;
 const read=(_,opts)=>{
  let pixels=new Uint8Array((opts.rect?.width??4)*(opts.rect?.height??4)*4).fill(255);
  const active=contacts===1&&kind()==='table';
  if(active&&opts.rect?.x===reference.rect.x&&opts.rect?.y===reference.rect.y){
   ++tableProbeCount;
   if(readMode==='error'&&tableProbeCount>1)throw Error('forced read exception');
   if(readMode==='positive')pixels=Uint8Array.from(reference.rgba);
  }else if(contacts===1&&kind()==='sticky')for(let i=0;i<pixels.length;i+=4)pixels.set([255,255,128,255],i);
  else if(contacts===1&&kind()==='image')for(let i=0;i<pixels.length;i+=4)pixels.set([255,0,0,255],i);
  return{metadata:{status:active&&readMode==='invalid'?'INVALID':'VALID',reason:active&&readMode==='invalid'?'forced invalid FBO':null},bytes:active&&readMode==='invalid'?null:pixels};
 };
 const context=vm.createContext({Uint8Array,ArrayBuffer,BigInt,structuredClone,crypto:webcrypto,
  performance:{now:()=>time+=20},setTimeout:fn=>{time+=1000;fn();},btoa:s=>Buffer.from(s,'binary').toString('base64'),
  fetch:async()=>({arrayBuffer:async()=>Uint8Array.of(9).buffer}),document:{getElementById:()=>({})},
  PointerEvent:class{constructor(type,props){this.type=type;Object.assign(this,props);}},
  window:{tpRecord:async({name,value})=>{const v=structuredClone(value);records.set(name,v);if(name==='table-feedback')feedbackSaved.push(v);},tpChunk:async v=>{buffers.push(v.name);return{nextOffset:v.offset+Buffer.from(v.base64,'base64').length};},tpMetadata:async v=>records.set('raw',structuredClone(v))},exactFixtureEntropy:{transcript:()=>({seed:324508639})}});
 const modules={
  '/sdk/axiom.mjs':{createRuntime:async()=>sdk},'/g5/navigation_observer.mjs':{installNavigationObserver:()=>observer},
  '/g5/webgl_target_readback.mjs':{readWebGLTarget:read},'/g5/ur2d_selection_up_contract.mjs':{sendBinaryChunks},
  '/g5/exact_query_working_set_contract.mjs':{typedWitness,qualifyWitness},
  '/g5/table_continuity_proof_contract.mjs':{probeUntilContent,compareExpectedROI},
  '/g5/exact_fixed_scene_contract.mjs':{emptyViewPoint:()=>({view:[24,986]})}
 };
 const mod=new vm.SourceTextModule('export '+webConsumer.toString(),{context,importModuleDynamically:async name=>{
  assert(modules[name],name);const exports=modules[name],m=new vm.SyntheticModule(Object.keys(exports),function(){for(const[k,v]of Object.entries(exports))this.setExport(k,v);},{context});await m.link(()=>{});await m.evaluate();return m;
 }});
 await mod.link(()=>{});await mod.evaluate();
 const fixture={sha256:createHash('sha256').update(Uint8Array.of(9)).digest('hex'),metadata:{documentRevision:'42',recoveryFrontier:[]},documentId:'9'.repeat(32)};
 const result=await mod.namespace.webConsumer({id:'forced-branch',fixture,tableReference:reference,deferEvidence});
 return{result,records,buffers,calls,feedbackSaved};
}
test('actual driver VM preserves timeout, preDown committed basis and continues independent image after Cancel',async()=>{
 const r=await branch();const table=r.records.get('table-stage'),raw=r.records.get('raw');
 assert.equal(table.status,'MISSING');assert.equal(table.feedback.last.metadata.status,'VALID');assert.equal(table.feedback.last.state.activeContacts,1);
 assert.equal(table.feedback.last.rgba.length,64);assert.equal(table.feedback.recent.length,8);
 assert.equal(table.basis.typed.objectId,reference.objectId);assert.equal(table.basis.latestCommittedSnapshot.before.activeContacts,0);
 assert(r.buffers.includes('table-preDown.axsnap'));assert(r.buffers.includes('table-active-missing.rgba'));
 assert.equal(raw.feedbacks.find(v=>v.kind==='image').status,'QUALIFIED');assert.equal(raw.feedbacks.find(v=>v.kind==='image').pacing,'MODIFIED_RECOVERY_AFTER_EXPLICIT_CANCEL');assert.equal(raw.closed,true);
 assert(r.feedbackSaved.length>1);
});
test('actual driver VM separates invalid readback from white and retains Cancel/close errors',async()=>{
 const r=await branch({readMode:'invalid',cancelError:true,closeError:true});const table=r.records.get('table-stage'),raw=r.records.get('raw');
 assert.equal(table.feedback.last.metadata.status,'INVALID');assert.equal(table.feedback.last.rgba,null);assert.equal(table.feedback.last.readbackClassification,'INVALID');
 assert.match(table.errors.join(' '),/forced cancel failed/);assert.match(table.cancelError,/forced cancel failed/);
 assert(raw.cleanupErrors.some(v=>v.stage==='close'&&/BUSY/.test(v.error)));
 assert.equal(raw.feedbacks.length,2);assert.equal(raw.feedbacks.find(v=>v.kind==='image'),undefined);
});
test('actual driver VM keeps last valid table pixels when next read throws',async()=>{
 const r=await branch({readMode:'error'}),table=r.records.get('table-stage');
 assert.equal(table.feedback.status,'ERROR');assert.match(table.feedback.error,/forced read exception/);assert.equal(table.feedback.last.rgba.length,64);
 assert(r.records.get('raw'));
});
test('frozen deferred branch retains failed proof without preDown host snapshot and preserves actual sticky commit',async()=>{
 const r=await branch({deferEvidence:true}),table=r.records.get('table-stage');
 assert.equal(table.feedback.status,'MISSING');assert.equal(table.preDownSnapshot,undefined);
 assert.equal(table.basis.latestCommittedBasis.method,'initialcanonical+legitimatepublicUp+operationandgenerationstamps;offlinecloneanddecode');
 assert.deepEqual([...table.basis.latestCommittedBasis.completedSticky.transform],[1,0,0,1,1130,530]);
 assert.equal(table.feedback.last.selection.primary,reference.objectId);
 assert.equal(table.feedback.last.typedTable.rows[0].size,48);
 assert(r.buffers.includes('table-active-missing.rgba'));
});
test('actual HTTP entry serves browser-only consumer and exact public SDK entry',async()=>{
 const sdkRoot=new URL('../../.aegis/results/G5-exact-history-path-residency-r1/immutable-sdk/normal/',import.meta.url).pathname;
 const server=createServer({fixture:{path:new URL('../../out/G5-table-continuity-proof-r1/new-request.json',import.meta.url).pathname}},{sdk:sdkRoot});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));try{
  const base='http://127.0.0.1:'+server.address().port;
  const html=await(await fetch(base+'/')).text();assert.match(html,/1431px/);assert.match(html,/window.run=webConsumer/);
  const source=await(await fetch(base+'/driver.mjs')).text();assert(!source.includes('node:'));assert(source.startsWith('export async function webConsumer'));
  const wrapper=await(await fetch(base+'/sdk/axiom.mjs')).text();assert.match(wrapper,/createRuntime/);
  const helper=await(await fetch(base+'/g5/table_continuity_proof_contract.mjs')).text();assert.match(helper,/probeUntilContent/);
 }finally{await new Promise(r=>server.close(r));}
});
