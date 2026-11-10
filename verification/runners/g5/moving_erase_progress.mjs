import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {qualifyTiming} from './moving_erase_progress_contract.mjs';
import {persistPageFailures} from './webgl_target_readback_host.mjs';
const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..');
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');

export async function consumer(){
 const {point,gestures,qualifyTiming}=await import('/g5/moving_erase_progress_contract.mjs');
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const {readWebGLTarget,captureFailureNow}=await import('/g5/webgl_target_readback.mjs');
 let sdk,initial,currentRow,clock=31004000000n;const buffers={},events=[],operations=[],errors=[],frames=[],captures=[];
 const observer=installNavigationObserver(()=>sdk?.canvas),timer=ms=>new Promise(r=>setTimeout(r,ms)),state=()=>sdk.readState();
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),x=>x.toString(16).padStart(2,'0')).join('');
 const check=(v,m)=>{if(!v)throw Error(m);};
 const settle=async()=>{const end=performance.now()+20000;for(;;){const s=state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;check(performance.now()<end,'20s canonical idle limit');await timer(20);}};
 const pointer=(phase,x,y,planned=performance.now(),stamp=null)=>{
  clock=stamp===null?clock+16666667n:BigInt(stamp);const rect=sdk.canvas.getBoundingClientRect();
  const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});
  const started=performance.now();observer.dispatch(sdk.canvas,e);const ended=performance.now(),s=state();const row={phase,x,y,planned,started,ended,clock_ns:String(clock),activeContacts:s.activeContacts,generation:s.generation};events.push(row);return row;
 };
 const init=async config=>{
  const {createRuntime}=await import('/axiom.mjs');const options={mount:document.getElementById('mount'),builtinFonts:false,permission:'localSingleUser',onError:e=>errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>operations.push({at:performance.now(),bytes:b.length}),onFrame:v=>frames.push({at:performance.now(),value:v})};
  if(config.restore){const bytes=new Uint8Array(await(await fetch('/baseline.axsnap')).arrayBuffer());check(await digest(bytes)===config.restore.sha256,'baseline snapshot identity');options.documentId=config.restore.documentId;options.restore={bytes,metadata:config.restore.metadata};}
  sdk=await createRuntime(options);sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();
  if(!config.restore){
   await sdk.selectBrush('round-handdrawn',4);await sdk.setBrushConfig({...sdk.readBrushConfig(),...config.workload.brush_overrides});await sdk.selectTool('ink');
   const trace=config.trace.points,origin=clock,first=trace[0].second;
   pointer('down',...trace[0].point,performance.now(),String(origin));
   for(const p of trace.slice(1))pointer('move',...p.point,performance.now(),String(origin+BigInt(Math.round((p.second-first)*1e9))));
   pointer('up',...trace.at(-1).point);await settle();await sdk.selectTool('select');
   for(let i=30;i<trace.length&&!state().selected.length;i+=40){pointer('down',...trace[i].point);pointer('up',...trace[i].point);}
   check(state().selected.length===1,'baseline reconstructed stroke selection');
   for(let i=0;i<config.workload.creation.copies;++i)await sdk.duplicateSelection({offset:config.workload.creation.duplicate_offset});
   await settle();
  }
  check(state().objects===19,'baseline must contain19 real ink objects');
  initial=state().generation;await sdk.setEraser({mode:config.workload.eraser.mode,diameter:config.workload.eraser.diameter});await sdk.selectTool('partialEraser');await settle();
  events.length=operations.length=errors.length=frames.length=0;
  return {state:state(),build:sdk.readBuildInfo(),canvas:[sdk.canvas.width,sdk.canvas.height],DPR:devicePixelRatio,resource_binding:sdk.resources.list().binding};
 };
 const snapshot=async key=>{
  const s=state(),request={generation:s.generation,documentRevision:s.generation,recoveryFrontier:[8,1,16,1,24,1]},value=await sdk.captureSnapshot(request);buffers[key]=value.bytes;
  return {key,sha256:await digest(value.bytes),length:value.bytes.length,documentId:s.documentId,state:s,metadata:{documentRevision:request.documentRevision,recoveryFrontier:request.recoveryFrontier,checkpointDigest:value.checkpointDigest,resourceBinding:sdk.resources.list().binding}};
 };
 const capture=(spec,i)=>{
  const prefix=Math.max(1,i-9),[x,y]=point(spec.kind,spec.g,prefix),dpr=devicePixelRatio;
  const rect={x:Math.floor((x-8)*dpr),y:Math.floor((y-8)*dpr),width:16*dpr,height:16*dpr};
  const r=readWebGLTarget(sdk.canvas,{role:'canonical',rect});
  if(r.metadata.status!=='VALID'){captureFailureNow({targets:{canonical:sdk.canvas,preview:()=>document.querySelector('#mount canvas[id$="-preview"]')},state:()=>({state:state(),camera:state().camera}),trigger:r,reason:'active target read invalid'});throw Error('active target read invalid');}
  const key='active-'+captures.length;buffers[key]=r.bytes;captures.push({key,spec,i,prefix,rect,at:performance.now(),state:state(),readback:r.metadata,beforeUp:true});
 };
 const fullStage=async stage=>{
  const row={stage,state:state(),surfaces:{},snapshot:await snapshot('full-'+stage+'-snapshot')};
  for(const role of ['canonical','preview']){const canvas=role==='canonical'?sdk.canvas:document.querySelector('#mount canvas[id$="-preview"]');const r=readWebGLTarget(canvas,{role});check(r.metadata.status==='VALID','full target read invalid');const key='full-'+stage+'-'+role;buffers[key]=r.bytes;row.surfaces[role]={key,width:r.width,height:r.height,readback:r.metadata,length:r.bytes.length,sha256:await digest(r.bytes)};}
  return row;
 };
 const run=async({hold=false,correctness=false,enforceProgress=false}={})=>{
  const row=currentRow={gestures:[],events,operations,errors,frames,observation:{},captures,cold:[],initial:state(),readback_free:!correctness,hold_control:hold};if(correctness)row.cold.push(await fullStage('before'));observer.begin(row.observation);
  try{
   const all=gestures();
   for(const [index,spec]of all.entries()){
    const down=pointer('down',...point(spec.kind,spec.g,0));check(down.activeContacts===1,'Down not admitted');const g={...spec,down:down.started,index};row.gestures.push(g);if(index&&all[index-1].gap===0)g.same_dom_task_after_up=true;
    const start=performance.now();
    for(let i=1;i<=spec.moves;++i){const planned=start+i*1000/60;await timer(Math.max(0,planned-performance.now()));pointer('move',...point(spec.kind,spec.g,i),planned);if(correctness&&[.25,.5,.75].some(f=>i===Math.floor(spec.moves*f)))capture(spec,i);}
    g.movingEnded=performance.now();if(hold)await timer(500);
    const up=pointer('up',...point(spec.kind,spec.g,spec.moves));g.up=up.started;
    if(spec.gap)await timer(spec.gap);
   }
   row.afterInput=state();const s={kind:'sustained',g:0};pointer('down',...point(s.kind,s.g,0));const start=performance.now();
   for(let i=1;i<=60;++i){const at=start+i*1000/60;await timer(Math.max(0,at-performance.now()));pointer('move',...point(s.kind,s.g,i),at);}pointer('cancel',...point(s.kind,s.g,60));
  }catch(error){row.error=String(error.stack??error);captureFailureNow({targets:{canonical:sdk.canvas,preview:()=>document.querySelector('#mount canvas[id$="-preview"]')},state:()=>({state:state(),camera:state().camera}),reason:row.error});throw error;}
  finally{observer.end(row.observation);}
  await settle();row.after=state();if(correctness)row.cold.push(await fullStage('after'));await sdk.undo();await settle();row.undo=state();if(correctness)row.cold.push(await fullStage('undo'));await sdk.redo();await settle();row.final=state();if(correctness)row.cold.push(await fullStage('redo'));
  row.timing=qualifyTiming(row);
  const valid=!errors.length&&operations.length===8&&BigInt(row.afterInput.generation)===BigInt(initial)+6n&&row.final.activeContacts===0&&row.final.generation===row.final.lastCommit.presentedGeneration;
  if(!valid||(enforceProgress&&row.timing.status!=='QUALIFIED')){const message=valid?'ACTIVE_PROGRESS_OR_PIXEL_RECEIPT_FAILURE':'input/operation/receipt invariant';captureFailureNow({targets:{canonical:sdk.canvas,preview:()=>document.querySelector('#mount canvas[id$="-preview"]')},state:()=>({state:state(),camera:state().camera}),reason:message});throw Error(message);}
  row.status='RECORDED';return row;
 };
 window.mp={init,snapshot,run,state,dump:()=>currentRow,captures:async()=>{for(const c of captures){c.sha256=await digest(buffers[c.key]);c.length=buffers[c.key].length;}return captures;},pull:(key,off,n)=>{let s='';const b=buffers[key].subarray(off,off+n);for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);},release:key=>delete buffers[key],async close(){await sdk.close();observer.restore();return true;}};
}

export async function worker(request){
 const {chromium}=await import(path.join(ROOT,'verification/node_modules/playwright/index.mjs'));
 const sdk=path.resolve(request.sdk),out=path.resolve(request.out),task=path.join(ROOT,'.aegis/packages/G5-UR2c-moving-preview-progress-r1'),g5=path.join(ROOT,'verification/runners/g5');await fs.mkdir(out,{recursive:true});
 const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');
 const artifactMap=async()=>{const m={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())m[n]=sha(await fs.readFile(p));}return m;};
 const result={schema:'g5-moving-erase-worker-v1',id:request.id,status:'STARTED',sdk_before:await artifactMap(),cases:[],errors:[]};let browser,server,page;
 const transfer=async(meta,filename)=>{const chunks=[];for(let off=0;off<meta.length;off+=262144)chunks.push(Buffer.from(await page.evaluate(({key,off})=>mp.pull(key,off,262144),{key:meta.key,off}),'base64'));const bytes=Buffer.concat(chunks);if(sha(bytes)!==meta.sha256)throw Error('raw transfer identity');await fs.writeFile(path.join(out,filename),bytes);await page.evaluate(key=>mp.release(key),meta.key);};
 try{
  server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://localhost');if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:590px;height:900px"></div><script type="module">await ('+consumer.toString()+')();</script>');return;}
   if(u.pathname==='/baseline.axsnap'){res.end(await fs.readFile(request.baseline));return;}
   const [base,suffix]=u.pathname.startsWith('/g5/')?[g5,u.pathname.slice(4)]:[sdk,u.pathname.slice(1)];const p=path.resolve(base,suffix);if(!p.startsWith(base+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(p));}catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
  const workload=JSON.parse(await fs.readFile(path.join(task,'inputs/workload.json'))),trace=JSON.parse(await fs.readFile(path.join(task,'inputs/stroke-trace.json')));
  for(const spec of request.passes??request.DPR.map(DPR=>({DPR,correctness:false,hold:false}))){const dpr=spec.DPR;
   page=await browser.newPage({viewport:{width:590,height:1000},deviceScaleFactor:dpr});page.setDefaultTimeout(25000);page.on('pageerror',e=>result.errors.push(String(e)));await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.mp);
   const row={DPR:dpr};row.init=await page.evaluate(c=>mp.init(c),{workload,trace,restore:request.fixture??null});
   if(request.variant==='old-normal'){const baseline=await page.evaluate(()=>mp.snapshot('baseline'));await transfer(baseline,'baseline.axsnap');await write('baseline.json',baseline);result.baseline=baseline;}
   const prefix='DPR'+dpr+(spec.correctness?'-pixels':spec.hold?'-hold':'-timing');
   row.observation=await page.evaluate(o=>mp.run(o),{hold:spec.hold,correctness:spec.correctness,enforceProgress:request.variant==='normal'&&!spec.correctness&&!spec.hold});row.captures=await page.evaluate(()=>mp.captures());for(const c of row.captures)await transfer(c,prefix+'-'+c.key+'.rgba');
   for(const stage of row.observation.cold){await transfer(stage.snapshot,prefix+'-'+stage.stage+'.axsnap');for(const [role,v]of Object.entries(stage.surfaces))await transfer(v,prefix+'-'+stage.stage+'.'+role+'.rgba');}
   row.timing=qualifyTiming(row.observation);row.spec=spec;result.cases.push(row);await write(prefix+'-result.json',row);row.closed=await page.evaluate(()=>mp.close());await page.close();page=null;
   console.log(JSON.stringify({stage:'moving-case-closed',DPR:dpr,timing:row.timing.status}));
  }
  result.status=request.variant==='old-normal'?(result.cases.some(x=>x.timing.status==='FAILED')?'EXPECTED_RED_OBSERVED':'EXPECTED_RED_NOT_OBSERVED'):'OBSERVED';
 }catch(error){result.status='HARD_FAILURE';result.error=String(error.stack??error);if(page&&!page.isClosed()){result.failure_captures=await persistPageFailures(page,out,'moving');result.partial=await page.evaluate(()=>mp.dump());}}
 finally{if(page&&!page.isClosed())try{result.failure_state=await page.evaluate(()=>mp.state());await page.evaluate(()=>mp.close());}catch(e){result.cleanup_error=String(e);}if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.sdk_after=await artifactMap();result.sdk_stable=JSON.stringify(result.sdk_before)===JSON.stringify(result.sdk_after);if(!result.sdk_stable||result.errors.length)result.status='HARD_FAILURE';await write('worker-result.json',result);}
 console.log(JSON.stringify({stage:'moving-worker-closed',status:result.status,error:result.error??null}));if(!['OBSERVED','EXPECTED_RED_OBSERVED'].includes(result.status))process.exitCode=2;return result;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))await worker(JSON.parse(await fs.readFile(process.argv[2])));
