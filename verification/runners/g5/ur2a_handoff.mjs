import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {fileURLToPath}from 'node:url';import {createHash}from 'node:crypto';import {execFileSync}from 'node:child_process';
import {CASES,qualifyCold,contentFeedback,qualifyDisplayJoins}from './ur2a_handoff_contract.mjs';
import {createReferenceQualifier}from './prepared_body_index_reverify_reference.mjs';
import {comparePixels}from './ur0/pixel_oracle.mjs';
const HERE=path.dirname(fileURLToPath(import.meta.url)),ROOT=path.resolve(HERE,'../../..');
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function handoffConsumer(){
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const {captureColdStage}=await import('/g5/ur2a_cold_capture.mjs');
 const {runFunctionalErase,boundedDisplayOpportunity}=await import('/g5/prepared_body_index_reverify_pacing.mjs');
 let sdk,fixture,initial,clock=31004000000n;const buffers={},phases=[],frames=[],errors=[],operations=[],cold=[];
 const state=()=>sdk.readState(),observer=installNavigationObserver(()=>sdk?.canvas);
 const error=e=>({message:String(e?.message??e),stack:String(e?.stack??''),code:e?.code??null});
 const check=(value,message)=>{if(!value)throw Error('PRODUCT_CHECK: '+message);};
 const raf=()=>new Promise(r=>{r.g5SamplerRaf=true;requestAnimationFrame(r);});
 const until=async at=>{while(performance.now()<at)await new Promise(r=>setTimeout(r,Math.max(0,at-performance.now())));};
 const settled=async()=>{const end=performance.now()+20000;for(;;){await raf();const s=state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;if(performance.now()>end)throw Error('PRODUCT_CHECK:20s exact/idle safety bound');}};
 const pointer=(phase,x,y)=>{clock+=4000000n;const r=sdk.canvas.getBoundingClientRect();const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+x,clientY:r.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});const started=performance.now();observer.dispatch(sdk.canvas,e);return{phase,started,ended:performance.now(),activeContacts:state().activeContacts,generation:state().generation,presented:state().lastCommit.presentedGeneration};};
 const wheel=ctrl=>{clock+=4000000n;const r=sdk.canvas.getBoundingClientRect();const e=new WheelEvent('wheel',{clientX:r.left+400,clientY:r.top+250,ctrlKey:ctrl,deltaX:ctrl?0:4,deltaY:ctrl?-4:2,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});observer.dispatch(sdk.canvas,e);};
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 const init=async f=>{
  fixture=f;const bytes=new Uint8Array(await(await fetch('/fixtures/'+f.name+'.axsnap')).arrayBuffer());check(await digest(bytes)===f.sha256,'fixture bytes');
  const {createRuntime}=await import('/axiom.mjs');sdk=await createRuntime({mount:document.getElementById('mount'),documentId:f.state.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes,metadata:f.metadata},onError:e=>errors.push(error(e)),onFrame:v=>frames.push({at:performance.now(),value:v,state:sdk?.readState()??null}),onOperation:b=>operations.push({at:performance.now(),bytes:b.length})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await settled();initial=state().generation;await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');await settled();
  return{state:state(),build:sdk.readBuildInfo(),fixture_sha256:f.sha256,resource_binding:sdk.resources.list().binding,dpr:devicePixelRatio,clock:window.ur2aClock?.()??null};
 };
 const readSurface=role=>{
  const canvas=role==='canonical'?sdk.canvas:document.querySelector('#mount canvas[id$="-preview"]');if(!canvas)throw Error('missing '+role+' canvas');
  const gl=canvas.getContext('webgl2');if(gl.isContextLost())throw Error('lost '+role+' context');
  const width=canvas.width,height=canvas.height,raw=new Uint8Array(width*height*4),bytes=new Uint8Array(raw.length);gl.readPixels(0,0,width,height,gl.RGBA,gl.UNSIGNED_BYTE,raw);
  let nonzeroAlpha=0;for(let y=0;y<height;++y)bytes.set(raw.subarray((height-1-y)*width*4,(height-y)*width*4),y*width*4);for(let i=3;i<bytes.length;i+=4)if(bytes[i])++nonzeroAlpha;
  const css=getComputedStyle(canvas);return{bytes,width,height,nonzeroAlpha,visible:css.display!=='none'&&css.visibility!=='hidden'&&Number(css.opacity)>0,css:{display:css.display,visibility:css.visibility,opacity:css.opacity},target:canvas.id};
 };
 const capture=async stage=>{
  const settleErrors=[];try{await settled();}catch(e){settleErrors.push(error(e));}
  const row=await captureColdStage(stage,{readState:()=>({state:state(),camera:state().camera,selection:sdk.readSelection(),resource_binding:sdk.resources.list().binding,activity:observer.activity()}),readSurface,
   persist:async(key,bytes)=>{buffers[key]=bytes;return{sha256:await digest(bytes),length:bytes.length};},
   snapshot:async row=>{const request={generation:row.state.generation,documentRevision:(BigInt(fixture.metadata.documentRevision)+BigInt(row.state.generation)-BigInt(initial)).toString(),recoveryFrontier:fixture.metadata.recoveryFrontier};const s=await sdk.captureSnapshot(request);return{bytes:s.bytes,request,checkpointDigest:s.checkpointDigest};}});
  row.errors.push(...settleErrors);cold.push(row);return row;
 };
 const run=async id=>{
  const p={id,status:'STARTED',before:state(),schedule:[],started:performance.now()};phases.push(p);observer.begin(p);
  const erase=async(index,primed=false,primeNext=false)=>{
   const point=i=>[480+index*70+24*Math.sin(i*.13),250+25*Math.cos(i*.17)];let nextDown;
   await runFunctionalErase({down:()=>{if(!primed)p.schedule.push(pointer('down',...point(0)));check(state().activeContacts===1,'erase Down admission');},move:(x,y)=>p.schedule.push(pointer('move',x,y)),
    up:()=>{p.schedule.push(pointer('up',...point(32)));p.afterUp={at:performance.now(),state:state()};if(primeNext){const next=[480+(index+1)*70,275];nextDown=pointer('down',...next);p.schedule.push({...nextDown,same_dom_task_after_up:true});}},
    cancel:()=>{if(state().activeContacts)pointer('cancel',...point(0));},position:point,schedule:p.schedule,opportunities:[],now:()=>performance.now(),until,display:boundedDisplayOpportunity});
   return nextDown;
  };
  try{
   if(id.endsWith('B-brush-erase')){
    await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
    p.schedule.push(pointer('down',540,230));for(let i=1;i<=8;++i){p.schedule.push(pointer('move',540+i*8,230+i*2));await boundedDisplayOpportunity({timeoutMs:5000});}
    const overlay=document.querySelector('#mount canvas[id$="-preview"]');p.brushOverlay={display:getComputedStyle(overlay).display,visibility:getComputedStyle(overlay).visibility,opacity:getComputedStyle(overlay).opacity};
    p.schedule.push(pointer('up',604,246));p.brushSealed={at:performance.now(),state:state()};await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');await erase(1);
   }else if(id.endsWith('E-reerase')){
    await erase(0,false,true);await erase(1,true);const planned=performance.now()+4;await until(planned);p.schedule.push({kind:'nextDown-planned',planned,actual:performance.now()});await erase(2);
   }else{
    await erase(1);
    if(id.endsWith('H-undo-before-present')){p.preUndo={at:performance.now(),state:state()};await sdk.undo();p.afterUndo={at:performance.now(),state:state()};await sdk.redo();p.afterRedo={at:performance.now(),state:state()};}
    else{
     const sequence=async(count,ctrl)=>{const start=performance.now();for(let i=0;i<count;++i){const planned=start+i*12;await until(planned);const actual=performance.now();wheel(ctrl);p.schedule.push({kind:'wheel',ctrl,i,planned,actual});}};
     await sequence(24,id.endsWith('Z-zoom-reentry'));if(id.endsWith('Z-zoom-reentry')){await until(performance.now()+1000);await sequence(12,false);await sequence(12,true);}
    }
   }
   p.status='INPUT_COMPLETED';
  }catch(e){p.error=error(e);p.status='FAILED';}
  finally{p.inputEnded=performance.now();p.after=state();observer.end(p);}
  return p;
 };
 window.ur2a={init,capture,run,state,undo:()=>sdk.undo(),redo:()=>sdk.redo(),dump:()=>({state:sdk?state():null,phases,frames,errors,operations,cold,activity:observer.activity()}),pull:(key,off,count)=>{const b=buffers[key].subarray(off,off+count);let text='';for(let i=0;i<b.length;i+=8192)text+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(text);},release:key=>{delete buffers[key];},async close(){await sdk?.close();observer.restore();return{closed:!document.querySelector('#mount canvas')};}};
}
const qualificationFactory=`import factory from '/_engine/internal/axiom.mjs';export default async function(args){const module=await factory(args);if(module._axiom_g5_trace_control(2,101)!==0||module._axiom_g5_trace_control(1,0)!==0)throw Error('trace init');const drain=()=>{const chunks=[];for(let i=0;i<256;++i){if(module._axiom_g5_trace_control(4,0)!==0)throw Error('trace drain');const p=module._axiom_g5_trace_bytes(),n=module._axiom_g5_trace_size(),v=JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(p,p+n)));chunks.push(v);if(v.finished)return chunks;}throw Error('trace guard');};let collected=[];const timer=setInterval(()=>collected.push(...drain()),100);window.ur2aTrace=()=>{const rows=[...collected,...drain()];collected=[];return rows;};window.ur2aTraceClose=()=>{clearInterval(timer);return window.ur2aTrace();};window.ur2aClock=()=>{const before=performance.now();module._axiom_g5_trace_control(3,0);const p=module._axiom_g5_trace_bytes(),n=module._axiom_g5_trace_size();return{before,...JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(p,p+n))),after:performance.now()};};return module;}`;
export async function worker({sdkArg,outArg,variant,oracle,referenceCatalog}){
 const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
 const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
 const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,'ur2a_handoff_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
 const artifacts=async()=>{const rows={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())rows[n]=sha(await fs.readFile(p));}return rows;};
 const result={schema:'g5-ur2a-r2-web-worker',status:'STARTED',variant,source_before:source(),artifacts_before:await artifacts(),cases:[],page_errors:[]};
 const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');let browser,server,page;
 const tagsText=await fs.readFile(path.join(ROOT,'runtime/foundation/include/canvas/foundation/internal/g5_observation.hpp'),'utf8');const tags=Object.fromEntries(tagsText.match(/enum class Tag[^]*?\{([^]*?)\};/)[1].split(',').map((v,i)=>[v.trim().split('=')[0].trim(),i+1]));
 try{
  const qualify=await createReferenceQualifier(JSON.parse(await fs.readFile(referenceCatalog)),oracle);
  server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://127.0.0.1');if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">await ('+handoffConsumer.toString()+')();</script>');return;}
   if(variant==='qualification'&&u.pathname==='/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(qualificationFactory);return;}
   const [base,suffix]=u.pathname.startsWith('/_engine/')?[sdk,u.pathname.slice(9)]:u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];const p=path.resolve(base,suffix);if(!p.startsWith(base+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(p));}catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));const {chromium}=await import('../../node_modules/playwright/index.mjs');browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
  for(const id of CASES){
   const row={id,status:'STARTED',cold:[],failures:[],traces:[]};result.cases.push(row);page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
   await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur2a);
   const save=async stage=>{
    const value=await page.evaluate(s=>ur2a.capture(s),stage);row.cold.push(value);await write(id+'-cold.json',row.cold);
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
   row.status=row.failures.length?'FAILED':'COMPLETED';await write(id+'-result.json',row);await write(id+'-cold.json',row.cold);await write(id+'-trace.json',row.traces);await page.close();page=null;console.log(JSON.stringify({stage:'case-closed',id,status:row.status,failures:row.failures}));
  }
  result.status=result.cases.every(r=>r.status==='COMPLETED')&&!result.page_errors.length?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
 }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.ur2a?.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.source_after=source();result.artifacts_after=await artifacts();result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);if(!result.source_stable||!result.artifacts_stable||result.crash)result.status='HARD_FAILURE';await write('worker-result.json',result);}
 console.log(JSON.stringify({stage:'worker-closed',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){const[sdkArg,outArg,variant,oracle,referenceCatalog]=process.argv.slice(2);await worker({sdkArg,outArg,variant,oracle,referenceCatalog});}
