import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {fileURLToPath}from 'node:url';import {createHash}from 'node:crypto';import {execFileSync}from 'node:child_process';
import {CASES,qualifyCold,contentFeedback,qualifyDisplayJoins}from './ur2a_handoff_contract.mjs';
import {createReferenceQualifier}from './prepared_body_index_reverify_reference.mjs';
import {comparePixels}from './ur0/pixel_oracle.mjs';
const HERE=path.dirname(fileURLToPath(import.meta.url)),ROOT=path.resolve(HERE,'../../..');
const sha=b=>createHash('sha256').update(b).digest('hex');
import {handoffConsumer}from './ur2a_handoff.mjs';
import {CONTINUOUS_CASES,qualifyContinuous}from './transient_display_retry_contract.mjs';
import {persistPageFailures,classifyFailureCaptures}from './webgl_target_readback_host.mjs';
export async function continuousConsumer(){
 const {SENTINEL,validateTrajectoryPlan,requireTrajectoryCamera,qualifySentinel}=await import('/g5/transient_display_retry_contract.mjs');
 const {boundedDisplayOpportunity}=await import('/g5/prepared_body_index_reverify_pacing.mjs');
 const {captureColdStage}=await import('/g5/ur2a_cold_capture.mjs');
 const {readWebGLTarget,captureFailureNow}=await import('/g5/webgl_target_readback.mjs');
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 let sdk,fixture,tracePlan,trajectoryPlanSha,initial,clock=31004000000n,reference,referenceCamera,liveIndex=0;
 const row={schedule:[],live:[],frames:[],operations:[],errors:[],cold:[]};const observer=installNavigationObserver(()=>sdk?.canvas);
 const state=()=>sdk.readState(),digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 const camera=()=>JSON.stringify(state().camera),check=(v,m)=>{if(!v)throw Error('PRODUCT_CHECK: '+m);};
 const persist=async(key,b)=>{for(let off=0;off<b.length;off+=262144){const part=b.subarray(off,off+262144);let text='';for(let i=0;i<part.length;i+=8192)text+=String.fromCharCode(...part.subarray(i,i+8192));await window.transientPersist(key,off,btoa(text),b.length);}return{sha256:await digest(b),length:b.length};};
 const settle=async()=>{const until=performance.now()+20000;for(;;){await boundedDisplayOpportunity({timeoutMs:5000});const s=state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;check(performance.now()<until,'20s exact safety bound');}};
 const failureNow=(reason,trigger)=>captureFailureNow({targets:{canonical:sdk.canvas,preview:()=>document.querySelector('#mount canvas[id$="-preview"]')},state:()=>({state:state(),camera:state().camera,selection:sdk.readSelection(),resource_binding:sdk.resources.list().binding,stage:reason}),reason,trigger});
 const qualified=(role,rect,stage)=>{const c=role==='canonical'?sdk.canvas:document.querySelector('#mount canvas[id$="-preview"]');const r=readWebGLTarget(c,{role,rect});if(r.metadata.status!=='VALID'){row.failure=failureNow(stage+': '+r.metadata.reason,r);throw Error('READ_TARGET_ERROR_OR_STATE_RESTORATION_FAILURE: '+r.metadata.reason);}return r;};
 const surface=role=>{const r=qualified(role,undefined,'full-'+role),c=role==='canonical'?sdk.canvas:document.querySelector('#mount canvas[id$="-preview"]');let nonzeroAlpha=0;for(let i=3;i<r.bytes.length;i+=4)if(r.bytes[i])++nonzeroAlpha;const css=getComputedStyle(c);return{...r,metadata:undefined,readback:r.metadata,nonzeroAlpha,visible:css.display!=='none'&&css.visibility!=='hidden'&&Number(css.opacity)>0};};
 const readROI=stage=>{const c=sdk.canvas,sx=c.width/800,sy=c.height/500,x=Math.round(SENTINEL.left*sx),y=Math.round(SENTINEL.top*sy),w=Math.round((SENTINEL.right-SENTINEL.left)*sx),h=Math.round((SENTINEL.bottom-SENTINEL.top)*sy);const r=qualified('canonical',{x,y,width:w,height:h},stage);return{...r,metadata:undefined,readback:r.metadata,device:{x,y,w,h}};};
 const live=async stage=>{
  const r=readROI(stage),key='live-'+liveIndex++,v={stage,at:performance.now(),state:state(),camera:camera(),...r,bytes:undefined,key};
  v.qualifier=referenceCamera===v.camera?qualifySentinel(reference,r.bytes):{status:'INVALID_CAMERA'};row.live.push(v);
  if(v.qualifier.status!=='PRESERVED'){
   // No await, RAF, continued input or cleanup can precede these copies.
   row.failure=failureNow('live-'+stage,{...r,metadata:r.readback});
   throw Error(v.qualifier.status==='INVALID_CAMERA'?'READ_TARGET_CAMERA_MISMATCH':'PRODUCT_PIXEL_FAILURE: unrelated ink disappeared at '+stage);
  }
  Object.assign(v,await persist(key,r.bytes));await window.transientMetadata(row);
 };
 const resetROI=async stage=>{await settle();const r=readROI('baseline-'+stage);reference=r.bytes;referenceCamera=camera();row.roi={fixture:fixture.sha256,definition:SENTINEL,world_basis:state().camera,readback:r.readback};const q=qualifySentinel(reference,reference);if(q.status!=='PRESERVED'){row.failure=failureNow('baseline-'+stage,{...r,metadata:r.readback});throw Error('PRODUCT_PIXEL_FAILURE: fixture-defined ROI has no valid opaque interior');}await persist('baseline-'+liveIndex,reference);row.schedule.push({kind:'ROI-basis',stage,camera:state().camera,qualifier:q});};
 const pointer=(phase,x,y)=>{clock+=4000000n;const rect=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});const started=performance.now();observer.dispatch(sdk.canvas,e);const r={kind:'pointer',phase,x,y,started,ended:performance.now(),activeContacts:state().activeContacts,generation:state().generation};row.schedule.push(r);return r;};
 const point=(index,i)=>tracePlan.traces[index].view_points[i];
 const bindTrace=(index,cancel=false)=>{const trace=tracePlan.traces[index];requireTrajectoryCamera(state().camera,trace);check(BigInt(state().generation)===BigInt(initial)+BigInt(index),'frozen history prefix generation');row.schedule.push({kind:'frozen-trajectory',index,cancel,trajectory_plan_sha256:trajectoryPlanSha,camera:state().camera,generation:state().generation,view_points:trace.view_points,world_points:trace.world_points,expected_target_ids:trace.expected_target_ids,proof:trace.proof[String(devicePixelRatio)]});};
 const erase=async(index,{primed=false,primeNext=false,cancel=false}={})=>{bindTrace(index,cancel);if(!primed)pointer('down',...point(index,0));check(state().activeContacts===1,'Down not admitted');await boundedDisplayOpportunity({timeoutMs:5000});await live('erase'+index+'-down');const start=performance.now();for(let i=1;i<=8;++i){const planned=start+i*16.666667;const remaining=planned-performance.now();if(remaining>0)await new Promise(r=>setTimeout(r,remaining));pointer('move',...point(index,i));row.schedule.push({kind:'planned-move',i,planned,actual:performance.now()});await boundedDisplayOpportunity({timeoutMs:5000});if(i===4||i===8)await live('erase'+index+'-move'+i);}pointer(cancel?'cancel':'up',...point(index,8));if(primeNext){requireTrajectoryCamera(state().camera,tracePlan.traces[index+1]);const next=pointer('down',...point(index+1,0));next.same_dom_task_after_up=true;}await window.transientMetadata(row);};
 const navigation=async label=>{for(const ctrl of[false,true]){const start=performance.now();for(let i=0;i<12;++i){const planned=start+i*12,remaining=planned-performance.now();if(remaining>0)await new Promise(r=>setTimeout(r,remaining));clock+=4000000n;const rect=sdk.canvas.getBoundingClientRect(),e=new WheelEvent('wheel',{clientX:rect.left+400,clientY:rect.top+250,ctrlKey:ctrl,deltaX:ctrl?0:4,deltaY:ctrl?-4:2,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});observer.dispatch(sdk.canvas,e);row.schedule.push({kind:'wheel',label,ctrl,i,planned,actual:performance.now(),camera:state().camera});}}await resetROI(label);};
 const init=async f=>{fixture=f;const planBytes=new Uint8Array(await(await fetch('/trajectory-plan.json')).arrayBuffer());trajectoryPlanSha=await digest(planBytes);check(trajectoryPlanSha===f.trajectory_plan_sha256,'trajectory plan identity');tracePlan=validateTrajectoryPlan(JSON.parse(new TextDecoder().decode(planBytes)),f.name,f.sha256,devicePixelRatio);const bytes=new Uint8Array(await(await fetch('/fixtures/'+f.name+'.axsnap')).arrayBuffer());check(await digest(bytes)===f.sha256,'fixture hash');const{createRuntime}=await import('/axiom.mjs');sdk=await createRuntime({mount:document.getElementById('mount'),documentId:f.state.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes,metadata:f.metadata},onError:e=>row.errors.push({message:String(e.message??e),code:e.code}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:v=>row.frames.push({at:performance.now(),value:v,state:sdk?.readState()})});sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();initial=state().generation;await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');await resetROI('initial');row.initial=state();check(row.initial.objects===16,'fixture object count');return{state:state(),build:sdk.readBuildInfo(),fixture_sha256:f.sha256,dpr:devicePixelRatio,resource_binding:sdk.resources.list().binding};};
 const capture=async stage=>{await settle();const v=await captureColdStage(stage,{readState:()=>({state:state(),camera:state().camera,selection:sdk.readSelection(),resource_binding:sdk.resources.list().binding,activity:observer.activity()}),readSurface:surface,persist,snapshot:async r=>{const request={generation:r.state.generation,documentRevision:(BigInt(fixture.metadata.documentRevision)+BigInt(r.state.generation)-BigInt(initial)).toString(),recoveryFrontier:fixture.metadata.recoveryFrontier};const s=await sdk.captureSnapshot(request);return{bytes:s.bytes,request,checkpointDigest:s.checkpointDigest};}});row.cold.push(v);return v;};
 const run=async()=>{observer.begin(row);try{await erase(0,{primeNext:true});await erase(1,{primed:true});await navigation('after-second');for(let i=2;i<6;++i)await erase(i);await navigation('after-sixth');await new Promise(r=>setTimeout(r,1000));await erase(6);await settle();await erase(7,{cancel:true});await settle();await erase(7);row.afterInput=state();row.status='INPUT_COMPLETED';}catch(e){row.error=String(e.stack??e);row.status='FAILED';throw e;}finally{observer.end(row);await window.transientMetadata(row);}return row;};
 window.transientDisplay={init,capture,run,undo:()=>sdk.undo(),redo:()=>sdk.redo(),dump:()=>({...row,final:{state:state()},activity:observer.activity()}),async close(){await sdk.close();observer.restore();return{closed:!document.querySelector('#mount canvas')};}};
}

async function runContinuousCase({browser,url,manifest,out,qualify,oracle,spec,result,trajectoryPlanSha}){
 const {id,fixture,dpr}=spec,row={id,status:'STARTED',cold:[],failures:[],normal_only:true};const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:dpr});
 page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
 const files=new Map();const fileFor=key=>path.join(out,id+'-'+(key.endsWith('-canonical')||key.endsWith('-preview')?key.replace(/-(canonical|preview)$/,
  (_,role)=>'.'+role+'.rgba'):key.endsWith('-snapshot')?key.replace(/-snapshot$/,'.axsnap'):key+'.roi.rgba'));
 const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');
 try{
  await page.exposeFunction('transientPersist',async(key,off,text,total)=>{if(!/^[a-zA-Z0-9_-]+$/.test(key)||off<0||off>=total)throw Error('raw stream shape');let h=files.get(key);if(off===0){if(h)throw Error('raw stream replay');h={handle:await fs.open(fileFor(key),'w'),offset:0,total};files.set(key,h);}if(!h||h.offset!==off||h.total!==total)throw Error('raw stream continuity');const bytes=Buffer.from(text,'base64');if(off+bytes.length>total)throw Error('raw stream overflow');await h.handle.write(bytes);h.offset+=bytes.length;if(h.offset===total){await h.handle.close();h.handle=null;}});
  await page.exposeFunction('transientMetadata',r=>write(id+'-continuous-prefix.json',r));
  await page.goto(url+'/continuous.html');await page.waitForFunction(()=>!!window.transientDisplay);row.init=await page.evaluate(f=>transientDisplay.init(f),{...manifest[fixture],trajectory_plan_sha256:trajectoryPlanSha});await write(id+'-init.json',row.init);
  const save=async stage=>{const value=await page.evaluate(s=>transientDisplay.capture(s),stage);row.cold.push(value);await write(id+'-cold.json',row.cold);
   const captured=await persistPageFailures(page,out,id);if(captured.length){row.failure_captures=captured;throw Error(classifyFailureCaptures(captured));}
   for(const [v,key]of [...Object.entries(value.surfaces).map(([r,v])=>[v,stage+'-'+r]),...(value.snapshot?[[value.snapshot,stage+'-snapshot']]:[])])if(sha(await fs.readFile(fileFor(key)))!==v.sha256)throw Error('cold byte stream identity');
   return value;};
  await save('before');row.input=await page.evaluate(()=>transientDisplay.run());await save('after');for(const stage of['undo','redo']){await page.evaluate(s=>transientDisplay[s](),stage);await save(stage);}const final=await page.evaluate(()=>transientDisplay.dump());Object.assign(row,final);row.cold=final.cold;await write(id+'-final.json',row);
  // Persist every live target and request before any oracle/qualification verdict.
  for(const live of row.live){const file=fileFor(live.key);if(sha(await fs.readFile(file))!==live.sha256)throw Error('live byte stream identity');}
  for(const stage of row.cold)if(stage.snapshot&&stage.surfaces.canonical){const image=stage.surfaces.canonical;stage.oracle=await qualify({oracle,snapshot:fileFor(stage.stage+'-snapshot'),actual:await fs.readFile(fileFor(stage.stage+'-canonical')),request:{width:image.width,height:image.height,logical_width:800,logical_height:500,camera:stage.camera,hud_bounds:[]},prefix:path.join(out,id+'-'+stage.stage+'-reference')});}
  row.cold_qualification=qualifyCold(row.cold);row.failures.push(...row.cold_qualification.failures);
  row.roundtrip=comparePixels(await fs.readFile(fileFor('after-canonical')),await fs.readFile(fileFor('redo-canonical')),await fs.readFile(path.join(out,id+'-after-reference.edge')));row.qualification=qualifyContinuous(row);row.failures.push(...row.qualification.failures);row.close=await page.evaluate(()=>transientDisplay.close());if(!row.close.closed)row.failures.push('owner cleanup');row.status=row.failures.length?'FAILED':'COMPLETED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failure_captures=await persistPageFailures(page,out,id);row.blocking_class=classifyFailureCaptures(row.failure_captures);}catch(x){row.failure_capture_missing=String(x);row.blocking_class='FAILURE_ARTIFACT_CAPTURE_MISSING';}try{row.partial=await page.evaluate(()=>window.transientDisplay?.dump());await write(id+'-partial.json',row.partial);}catch(x){row.partial_error=String(x);}}
 finally{for(const h of files.values())if(h.handle)await h.handle.close();if(row.status==='FAILED')try{row.failure_cleanup=await page.evaluate(()=>transientDisplay.close());}catch(x){row.failure_cleanup_missing=String(x);}await write(id+'-result.json',row);await page.close();}
 console.log(JSON.stringify({stage:'continuous-closed',id,status:row.status,error:row.error??null,failures:row.failures}));return row;
}
const qualificationFactory=`import factory from '/_engine/internal/axiom.mjs';export default async function(args){const module=await factory(args);if(module._axiom_g5_trace_control(2,101)!==0||module._axiom_g5_trace_control(1,0)!==0)throw Error('trace init');const drain=()=>{const chunks=[];for(let i=0;i<256;++i){if(module._axiom_g5_trace_control(4,0)!==0)throw Error('trace drain');const p=module._axiom_g5_trace_bytes(),n=module._axiom_g5_trace_size(),v=JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(p,p+n)));chunks.push(v);if(v.finished)return chunks;}throw Error('trace guard');};let collected=[];const timer=setInterval(()=>collected.push(...drain()),100);window.ur2aTrace=()=>{const rows=[...collected,...drain()];collected=[];return rows;};window.ur2aTraceClose=()=>{clearInterval(timer);return window.ur2aTrace();};window.ur2aClock=()=>{const before=performance.now();module._axiom_g5_trace_control(3,0);const p=module._axiom_g5_trace_bytes(),n=module._axiom_g5_trace_size();return{before,...JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(p,p+n))),after:performance.now()};};return module;}`;
export async function worker({sdkArg,outArg,variant,oracle,referenceCatalog,trajectoryPlanPath}){
 const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
 const trajectoryPlanBytes=await fs.readFile(trajectoryPlanPath),trajectoryPlanSha=sha(trajectoryPlanBytes);
 const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
 const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,'ur2a_handoff_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
 const artifacts=async()=>{const rows={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())rows[n]=sha(await fs.readFile(p));}return rows;};
 const result={schema:'g5-qualified-target-normal-web-worker-v1',status:'STARTED',variant,compiled_subject_source_tree_sha256:'d9bd0a26585a4419ea1a25701f5c42132cbb30e6fb149398a6fe117c1a4154dc',SDK_rebuilt:false,harness_source_before:source(),artifacts_before:await artifacts(),cases:[],page_errors:[]};
 const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');let browser,server,page;
 const tagsText=await fs.readFile(path.join(ROOT,'runtime/foundation/include/canvas/foundation/internal/g5_observation.hpp'),'utf8');const tags=Object.fromEntries(tagsText.match(/enum class Tag[^]*?\{([^]*?)\};/)[1].split(',').map((v,i)=>[v.trim().split('=')[0].trim(),i+1]));
 try{
  const qualify=await createReferenceQualifier(JSON.parse(await fs.readFile(referenceCatalog)),oracle);
  server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://127.0.0.1');if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">await ('+handoffConsumer.toString()+')();</script>');return;}
   if(u.pathname==='/trajectory-plan.json'){res.setHeader('Content-Type','application/json');res.end(trajectoryPlanBytes);return;}
   if(u.pathname==='/continuous.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">await ('+continuousConsumer.toString()+')();</script>');return;}
   if(variant==='qualification'&&u.pathname==='/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(qualificationFactory);return;}
   const [base,suffix]=u.pathname.startsWith('/_engine/')?[sdk,u.pathname.slice(9)]:u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];const p=path.resolve(base,suffix);if(!p.startsWith(base+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(p));}catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));const {chromium}=await import('../../node_modules/playwright/index.mjs');browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
  for(const id of CASES){
   const row={id,status:'STARTED',cold:[],failures:[],traces:[]};result.cases.push(row);page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
   await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur2a);
   const save=async stage=>{
    const value=await page.evaluate(s=>ur2a.capture(s),stage);row.cold.push(value);await write(id+'-cold.json',row.cold);
   const captured=await persistPageFailures(page,out,id);if(captured.length){row.failure_captures=captured;throw Error(classifyFailureCaptures(captured));}
    const members=[...Object.entries(value.surfaces).map(([role,v])=>[v,role+'.rgba']),...(value.snapshot?[[value.snapshot,'axsnap']]:[])];
    for(const [v,ext]of members){const filename=path.join(out,id+'-'+stage+'.'+ext),file=await fs.open(filename,'w');try{for(let off=0;off<v.length;off+=262144)await file.writeFile(Buffer.from(await page.evaluate(({key,off})=>ur2a.pull(key,off,262144),{key:v.key,off}),'base64'));}finally{await file.close();}
     if(sha(await fs.readFile(filename))!==v.sha256)throw Error('cold transfer identity');await page.evaluate(key=>ur2a.release(key),v.key);
    }
    if(variant==='qualification')row.traces.push({stage,chunks:await page.evaluate(()=>ur2aTrace())});await write(id+'-trace.json',row.traces);
   };
   row.init=await page.evaluate(f=>ur2a.init(f),manifest[id.slice(0,2)]);await write(id+'-init.json',row.init);
   await save('before');
   try{row.input=await page.evaluate(id=>ur2a.run(id),id);}catch(e){row.failures.push(String(e));}
   await save('after');
   for(const stage of ['undo','redo']){try{await page.evaluate(stage=>ur2a[stage](),stage);}catch(e){row.failures.push(stage+':'+String(e));}await save(stage);}
   row.final=await page.evaluate(()=>ur2a.dump());await write(id+'-final.json',row.final);
   // All four dual-surface buffers and snapshot prefixes are durable now.
   for(const s of row.cold)if(s.snapshot&&s.surfaces.canonical){const image=s.surfaces.canonical;try{s.oracle=await qualify({oracle,snapshot:path.join(out,id+'-'+s.stage+'.axsnap'),actual:await fs.readFile(path.join(out,id+'-'+s.stage+'.canonical.rgba')),request:{width:image.width,height:image.height,logical_width:800,logical_height:500,camera:s.camera,hud_bounds:[]},prefix:path.join(out,id+'-'+s.stage+'-reference')});}catch(e){s.oracle={status:'FAILED',error:String(e.stack??e)};}}
   row.cold_qualification=qualifyCold(row.cold);row.failures.push(...row.cold_qualification.failures);
   const after=row.cold.find(r=>r.stage==='after'),redo=row.cold.find(r=>r.stage==='redo');
   try{if(JSON.stringify(after.camera)!==JSON.stringify(redo.camera))throw Error('camera changed during History');row.roundtrip=comparePixels(await fs.readFile(path.join(out,id+'-redo.canonical.rgba')),await fs.readFile(path.join(out,id+'-after.canonical.rgba')),await fs.readFile(path.join(out,id+'-after-reference.edge')));if(row.roundtrip.status!=='QUALIFIED')row.failures.push('after/redo independent band roundtrip');}catch(e){row.failures.push('roundtrip:'+String(e));}
   if(row.input?.status!=='INPUT_COMPLETED'||row.final.errors.length||row.final.state.activeContacts!==0)row.failures.push('input/error/contact state');
   const expected=id.endsWith('E-reerase')||id.endsWith('H-undo-before-present')?3:id.endsWith('B-brush-erase')?2:1;
   if(!row.input||BigInt(row.input.after.generation)!==BigInt(row.input.before.generation)+BigInt(expected))row.failures.push('non-noop exactly-once commits');
   if(row.input&&(BigInt(row.input.after.generation)>BigInt(row.input.before.generation)+BigInt(expected)||BigInt(row.input.after.generation)<BigInt(row.input.before.generation))){await write(id+'-unsafe-semantic-state.json',row);throw Error('UNSAFE: unexpected canonical mutation count');}
   if(id.endsWith('P-pan')||id.endsWith('Z-zoom-reentry')){
    row.feedback=contentFeedback(row.input??{});if(row.feedback.status!=='OBSERVED')row.failures.push('no content writes during camera input');
    const generation=row.input?.afterUp?.state.generation;const exact=row.final.frames.find(f=>f.at>=row.input.afterUp.at&&f.state?.lastCommit.presentedGeneration===generation);
    row.content_before_exact=(row.input.colorOps??[]).some(w=>w.kind==='content'&&w.started>=row.input.afterUp.at&&w.ended< (exact?.at??0)&&w.ended<=row.input.inputEnded);
    if(!row.content_before_exact)row.failures.push('camera content not observed before exact');
   }
   row.close=await page.evaluate(()=>ur2a.close());if(!row.close.closed)throw Error('owner cleanup');
   if(variant==='qualification'){
    row.traces.push({stage:'close',chunks:await page.evaluate(()=>ur2aTraceClose())});const chunks=row.traces.flatMap(t=>t.chunks);if(chunks.some(c=>Number(c.loss?.critical)>0))row.failures.push('critical trace loss');
    row.display_joins=qualifyDisplayJoins(chunks.flatMap(c=>c.records??[]),tags);if(row.display_joins.status==='INVALID'){await write(id+'-invalid-display-join.json',row.display_joins);throw Error('UNSAFE: unqualified or foreign display completion');}if(row.display_joins.status!=='OBSERVED')row.failures.push('exact display/work joins unavailable');
   }else if(await page.evaluate(()=>typeof window.ur2aTrace!=='undefined'))row.failures.push('normal diagnostic leak');
   row.status=row.failures.length?'FAILED':'COMPLETED';await write(id+'-result.json',row);await write(id+'-cold.json',row.cold);await write(id+'-trace.json',row.traces);await page.close();page=null;console.log(JSON.stringify({stage:'case-closed',id,status:row.status,failures:row.failures}));if(row.status!=='COMPLETED')throw Error('PRODUCT_CHECK: original case failed '+id);
  }
  if(variant==='normal')for(const spec of CONTINUOUS_CASES){
    const row=await runContinuousCase({browser,url:'http://127.0.0.1:'+server.address().port,manifest,out,qualify,oracle,spec,result,trajectoryPlanSha});result.cases.push(row);await write(spec.id+'-result.json',row);if(row.status!=='COMPLETED')throw Error('PRODUCT_CHECK: continuous case failed '+spec.id);
  }
  result.status=result.cases.every(r=>r.status==='COMPLETED')&&!result.page_errors.length?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
 }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.failure_captures=await persistPageFailures(page,out,'original-case');result.blocking_class=classifyFailureCaptures(result.failure_captures);}catch(x){result.failure_capture_missing=String(x);}if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.ur2a?.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.harness_source_after=source();result.artifacts_after=await artifacts();result.source_stable=result.harness_source_before.source_tree_sha256===result.harness_source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);if(!result.source_stable||!result.artifacts_stable||result.crash)result.status='HARD_FAILURE';await write('worker-result.json',result);}
 console.log(JSON.stringify({stage:'worker-closed',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){const[sdkArg,outArg,variant,oracle,referenceCatalog,trajectoryPlanPath]=process.argv.slice(2);await worker({sdkArg,outArg,variant,oracle,referenceCatalog,trajectoryPlanPath});}
