import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {chromium}from '../../node_modules/playwright/index.mjs';
import {validateChunk}from './ur2d_selection_up_contract.mjs';
import {entropyStream,installEntropy}from './exact_fixed_scene_contract.mjs';
export function installWorkerWitness(){
 const Original=globalThis.Worker;const audit={jobs:[],terminals:[],errors:[],held:[],hold:false,failNext:false,workers:[],inputs:[],results:[],delayedReady:0};
 const stamp=buffer=>{const v=new DataView(buffer);return Array.from({length:8},(_,i)=>v.getBigUint64(32+i*8,true).toString());};
 globalThis.Worker=class extends Original{
  constructor(...args){super(...args);audit.workers.push(this);this.addEventListener('message',event=>{
   const d=event.data;if(d?.type==='terminal')audit.terminals.push({key:d.key,status:d.status,at:performance.now(),computeMs:d.computeMs,steps:d.steps,yieldMs:d.yieldMs,bytes:d.bytes?.byteLength??0});
  });}
  postMessage(message,transfer){if(message?.type==='run'){audit.inputs.push({key:message.key,bytes:message.bytes.slice(0)});audit.jobs.push({key:message.key,token:stamp(message.bytes),bytes:message.bytes.byteLength,at:performance.now()});}return super.postMessage(message,transfer);}
  set onmessage(fn){super.onmessage=event=>{
   if(event.data?.type==='terminal'&&event.data.bytes)audit.results.push({key:event.data.key,bytes:event.data.bytes.slice(0)});
   if(event.data?.type==='ready'&&audit.delayedReady){setTimeout(()=>fn(event),audit.delayedReady);return;}
   if(event.data?.type==='terminal'&&audit.failNext){audit.failNext=false;audit.errors.push({injected:'WorkerFailure',key:event.data.key,at:performance.now()});event={data:{type:'terminal',key:event.data.key,status:'Failed',reason:'QUALIFICATION_INJECTED_FAILURE'}};}
   if(event.data?.type==='terminal'&&audit.hold){audit.held.push(()=>fn(event));return;}fn(event);
  };}
 };
 globalThis.cpuPendingWitness={audit,fatal(){const w=audit.workers.at(-1);audit.errors.push({injected:'WorkerFatal',at:performance.now()});w.onerror?.(new ErrorEvent('error',{message:'QUALIFICATION_WORKER_FATAL'}));},release(){audit.hold=false;for(const fn of audit.held.splice(0))fn();}};
}
export async function consumer(c){
 const {createRuntime}=await import('/sdk/axiom.mjs');const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const {emptyViewPoint}=await import('/g5/exact_fixed_scene_contract.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={id:c.id,status:'STARTED',events:[],operations:[],errors:[],frames:[],observations:[],snapshots:{}};let sdk,clock=90000000000;const operationBuffers=[];
 const pause=ms=>new Promise(r=>setTimeout(r,ms));const check=(x,m)=>{if(!x)throw Error(m);};
 const witness=window.cpuPendingWitness;
 const sparseWorker=()=>witness?{jobs:witness.audit.jobs,terminals:witness.audit.terminals,errors:witness.audit.errors,workers:witness.audit.workers.length,held:witness.audit.held.length}:null;
 const save=()=>{row.worker=sparseWorker();return window.cwMetadata(row);};
 const persistedOps=new Set(),persistedInputs=new Set(),persistedResults=new Set();
 const flushOwned=async()=>{
  for(let i=0;i<operationBuffers.length;++i)if(!persistedOps.has(i)){await sendBinaryChunks('operation-'+i+'.axsnap',operationBuffers[i],window.cwChunk);persistedOps.add(i);}
  if(witness){for(let i=0;i<witness.audit.inputs.length;++i)if(!persistedInputs.has(i)){await sendBinaryChunks('job-input-'+i+'.axsnap',new Uint8Array(witness.audit.inputs[i].bytes),window.cwChunk);persistedInputs.add(i);}
   for(let i=0;i<witness.audit.results.length;++i)if(!persistedResults.has(i)){await sendBinaryChunks('job-result-'+i+'.axsnap',new Uint8Array(witness.audit.results[i].bytes),window.cwChunk);persistedResults.add(i);}}
 };const state=()=>sdk.readState();
 const privateCall=(action,values={})=>{const m=window.cpuPendingModule;if(!m)throw Error('qualification module absent');const p=m.stringToNewUTF8(JSON.stringify({command:'cpuPending',action,...values}));try{const value=JSON.parse(m.UTF8ToString(m._axiom_js_call(p,0,0)));if(!value.ok)throw Error(value.code);return value;}finally{m._free(p);}};
 const observe=label=>{const v={label,at:performance.now(),state:state(),...(window.cpuPendingModule?{pending:privateCall('facts')}:{})};row.observations.push(v);return v;};
 const pointer=(phase,x,y)=>{clock+=8000000;const r=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+x,clientY:r.top+y,button:phase==='move'?-1:0,buttons:phase==='up'||phase==='cancel'?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const at=performance.now();sdk.canvas.dispatchEvent(e);const elapsedMs=performance.now()-at;if(phase==='move')return {phase,at,elapsedMs};const s=state();const v={phase,x,y,at,elapsedMs,contacts:s.activeContacts,generation:s.generation};row.events.push(v);return v;};
 const stable=async(expected=null)=>{const began=performance.now();let quiet=0,prior='';for(;;){const s=state();const pending=window.cpuPendingModule?privateCall('facts').pending:0;if(window.cpuPendingModule){row.progress??=[];if(row.progress.length<256&&(!row.progress.length||performance.now()-row.progress.at(-1).at>250))row.progress.push({at:performance.now(),pending:privateCall('facts'),generation:s.generation,presented:s.lastCommit.presentedGeneration});}const key=s.generation+'/'+s.lastCommit.presentedGeneration;if(!pending&&!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&(!expected||row.operations.length>=expected) ){if(key===prior)++quiet;else quiet=0;if(quiet>=3)return;}else quiet=0;prior=key;check(performance.now()-began<60000,'canonical convergence60s');await pause(16);}};
 const snapshot=async(label,pixels=true)=>{const s=state();const snap=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks(label+'.axsnap',snap.bytes,window.cwChunk);row.snapshots[label]={state:s,bytes:snap.bytes.length};if(pixels){const p=readWebGLTarget(sdk.canvas,{role:'canonical'});row.snapshots[label].readback=p.metadata;check(p.metadata.status==='VALID','cold readback valid');await sendBinaryChunks(label+'.rgba',p.bytes,window.cwChunk);}await save();};
 const ready=async()=>{const began=performance.now();for(;;){const s=state();if(s.generation===s.lastCommit.presentedGeneration&&s.surface.available)return;check(performance.now()-began<30000,'initialrender30s');await pause(16);}};
 const wheel=()=>{const r=sdk.canvas.getBoundingClientRect();for(const [ctrl,dx,dy]of[[false,50,25],[true,0,-8]])sdk.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:r.left+500,clientY:r.top+300,deltaX:dx,deltaY:dy,ctrlKey:ctrl,bubbles:true,cancelable:true}));};
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,text:String(e)}),onOperation:b=>{operationBuffers.push(b);row.operations.push({at:performance.now(),bytes:b.length});},onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await ready();check(state().objects===40,'fixed40');
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await pause(32);row.before=state();await save();
  if(c.qualification){check(privateCall('facts').enabled,'candidateactuallyenabled');if(c.mode==='feedback')witness.audit.hold=true;if(c.mode==='failure'){witness.audit.hold=true;sdk.data.attach({capacity:16});}}
  const moves=c.moves??1600;pointer('down',420,160);for(let i=1;i<=moves;++i){pointer('move',420+7*Math.sin(i*.17),160+170*i/moves);if(i%8===0)await pause(0);}
  row.AUp=pointer('up',999,999);row.afterAUp=observe('afterAUp');check(row.AUp.contacts===0,'Acontactretired');
  if(c.candidate)check(row.afterAUp.state.generation===row.before.generation,'pendingUpmustnotcommit');
  // Deliberately no wait/settle between AUp and BDown.
  row.BDown=pointer('down',300,230);for(let i=1;i<=8;++i)pointer('move',300,230+i*4);row.whileB=observe('Bactive');check(row.BDown.contacts===1,'immediateBcontactaccepted');
  if(c.mode==='feedback'){
   row.cameraBefore=state().camera;wheel();row.cameraAfter=state().camera;check(JSON.stringify(row.cameraBefore)!==JSON.stringify(row.cameraAfter),'wheelnewpose');
   let busy=null;try{await sdk.undo();}catch(e){busy=e.code;}row.competingUndo=busy;check(busy==='BUSY'||busy==='COMMAND_REJECTED','pendingUndoexplicitBusy');
   const until=performance.now()+500;while(performance.now()<until){check(state().generation===row.before.generation,'heldAdidnotcommit');await pause(16);}
   row.held=observe('heldAactiveB');const px=readWebGLTarget(sdk.canvas,{role:'canonical'});row.heldReadback=px.metadata;check(px.metadata.status==='VALID','heldfeedbackreadback');await sendBinaryChunks('held-feedback.rgba',px.bytes,window.cwChunk);witness.release();
  }
  row.BUp=pointer('up',999,999);row.sequenceEndAt=performance.now();await save();
  if(c.mode==='failure'){
   const start=performance.now();while(!witness.audit.jobs.length){check(performance.now()-start<5000,'actualWorkerrequestbeforefatal');await pause(1);}
   witness.audit.delayedReady=250;row.fatalBefore=observe('beforeFatal');witness.fatal();witness.release();
  }
  if(c.mode==='failure'){
   let busy=null;try{await sdk.close();}catch(e){busy=e.code;}row.pendingClose=busy;check(busy==='BUSY','pendingcloseBusy');
  }
  if(c.mode==='failure'){
   const gl=sdk.canvas.getContext('webgl2'),ext=gl.getExtension('WEBGL_lose_context');check(ext,'contextloss extension');
   row.beforeLoss=observe('beforeContextLoss');ext.loseContext();await pause(50);row.whileLost=observe('contextLost');ext.restoreContext();await pause(100);row.afterRestore=observe('contextRestored');
  }
  await stable(c.mode==='failure'?1:2);row.settledAt=performance.now();row.settled=observe('settled');row.firstCorrectSettledFrame=row.frames.find(f=>f.at>=row.sequenceEndAt)??null;
  if(c.qualification){row.worker=sparseWorker();check(privateCall('facts').pending===0,'allpendingterminal');}
  if(c.mode==='failure')check(row.errors.some(e=>e.code==='CPU_PENDING_FAILED'),'explicitfailureerror');else check(row.errors.length===0,'noinputorworkererrors');
  // Primary timing stops before any opaque bytes are encoded/persisted.
  await flushOwned();
  // Primary timing stops before cold snapshot/readback/UndoRedo.
  await sdk.selectTool('select');const clearPoint=emptyViewPoint(state().camera);pointer('down',...clearPoint.view);pointer('up',...clearPoint.view);await pause(32);await stable();check(sdk.readSelection().count===0,'coldpixelsselectionclear');
  await snapshot('final');await sdk.undo();await stable();await snapshot('undo');await sdk.redo();await stable();await snapshot('redo');
  if(c.mode==='failure'){
   const packets=[];let cursor=null;for(let i=0;i<16;++i){const p=sdk.data.readNext(cursor);if(!p)break;packets.push(p);cursor=p;}
   check(packets.length>0,'actualcanonicalpublicationretained');row.publicationPackets=packets.map(({bytes,...p})=>({...p,bytes:bytes.length}));
   let pending=null;try{await sdk.close();}catch(e){pending=e.code;}row.publicationClose=pending;check(pending==='PUBLICATION_PENDING','realpublicationclosebarrier');
   sdk.data.release(packets.at(-1));row.publicationReleased=sdk.data.status();check(row.publicationReleased.pending===0,'exactpublicationprefixreleased');await sdk.close();row.retryClose=true;sdk=null;
  }
  row.entropy=globalThis.exactFixtureEntropy?.transcript();row.finalState=sdk?state():row.snapshots.redo.state;row.status='OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failureState=state();if(window.cpuPendingModule)row.failurePending=privateCall('facts');const p=readWebGLTarget(sdk.canvas,{role:'canonical'});row.failureReadback=p.metadata;if(p.bytes)await sendBinaryChunks('failure.rgba',p.bytes,window.cwChunk);}catch(e){row.failureCaptureError=String(e);}await save();}
 finally{try{await flushOwned();}catch(error){row.bufferCaptureError=String(error);}try{window.cpuPendingWitness?.release();if(sdk){if(state().activeContacts)pointer('cancel',0,0);await sdk.close();row.closed=true;}}catch(e){row.cleanupError=String(e.stack??e);}try{await flushOwned();}catch(error){row.lateBufferCaptureError=String(error);}await save();}
 return row;
}
export function createServer(request,active){const root=process.cwd();return http.createServer(async(req,res)=>{try{const name=new URL(req.url,'http://local').pathname;let file;
 if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{consumer}from"/driver.mjs";window.run=consumer;</script>');return;}
 if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
 if(name==='/sdk/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end("import factory from '/engine/internal/axiom.mjs';export default async o=>{const m=await factory(o);window.cpuPendingModule=m;return m;};");return;}
 if(name==='/fixture.axsnap')file=path.resolve(root,request.fixture.path);else if(name.startsWith('/sdk/'))file=path.join(active.sdk,name.slice(5));else if(name.startsWith('/engine/'))file=path.join(active.sdk,name.slice(8));else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));if(!file)throw Error('unknown');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});}
export async function run(request,out){await fs.mkdir(out,{recursive:true});const result={id:request.id,status:'STARTED',rows:[],errors:[]};const active={};let server,browser,page;
 try{server=createServer(request,active);await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
 for(const spec of request.cases){active.sdk=spec.sdk;active.qualification=!!spec.qualification;const dir=path.join(out,spec.id);await fs.mkdir(dir);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});const chunks=new Map(),consoleLines=[];await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});await page.addInitScript(installWorkerWitness);
 page.on('console',m=>consoleLines.push({type:m.type(),text:m.text()}));page.on('pageerror',e=>result.errors.push(String(e)));await page.exposeFunction('cwMetadata',v=>fs.writeFile(path.join(dir,'raw.json'),JSON.stringify(v,null,2)+'\n'));await page.exposeFunction('cwChunk',async c=>{const previous=chunks.get(c.name);if(previous?.complete)throw Error('duplicatebuffer');const next=validateChunk(c,previous),bytes=Buffer.from(c.base64,'base64'),file=path.join(dir,c.name+'.partial'),h=await fs.open(file,c.offset?'r+':'wx');try{const w=await h.write(bytes,0,bytes.length,c.offset);if(w.bytesWritten!==bytes.length)throw Error('shortwrite');}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(dir,c.name));chunks.set(c.name,next);return{nextOffset:next.nextOffset};});
 console.log(JSON.stringify({stage:'case-start',id:spec.id}));await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);const row=await page.evaluate(c=>window.run(c),{...spec,fixture:request.fixture});result.rows.push({id:spec.id,status:row.status,error:row.error,cleanupError:row.cleanupError});await fs.writeFile(path.join(dir,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');await page.close();page=null;console.log(JSON.stringify({stage:'case-end',...result.rows.at(-1)}));if(row.status!=='OBSERVED'||row.cleanupError)throw Error('frozen case failure stop');}
 if(result.errors.length)throw Error('pageerrors recorded');result.status='OBSERVED';}catch(e){result.status='FAILED';result.error=String(e.stack??e);}finally{if(page)await page.close();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status}));}return result;}
if(process.argv[1]===new URL(import.meta.url).pathname){const q=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(q,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
