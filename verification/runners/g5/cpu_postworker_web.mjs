// CPUW7 bounded postWorker phase attribution; original CPUW6b input/history order.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createReadStream,createWriteStream} from 'node:fs';
import {pipeline} from 'node:stream/promises';
import {createGzip} from 'node:zlib';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {entropyStream,installEntropy} from './exact_fixed_scene_contract.mjs';
import {installWorkerWitness} from './cpu_pending_web.mjs';
import {validateChunk} from './ur2d_selection_up_contract.mjs';
export function installPostworkerObserver(){
 let active=false,loss=0;const raf=[],longTasks=[],native=[],maximum=12000;
 const raw=requestAnimationFrame;
 window.requestAnimationFrame=fn=>raw(timestamp=>{if(!active)return fn(timestamp);
  const began=performance.now();try{return fn(timestamp);}finally{
   if(raf.length<maximum)raf.push({timestamp,started:began,ended:performance.now()});else ++loss;
  }
 });
 const observer=new PerformanceObserver(list=>{if(active)for(const e of list.getEntries()){
  if(longTasks.length<maximum)longTasks.push({started:e.startTime,duration:e.duration});else ++loss;
 }});try{observer.observe({type:'longtask',buffered:true});}catch{}
 for(const name of ['error','warn','log','info']){const original=console[name];console[name]=function(...args){
  if(active&&typeof args[0]==='string'&&/^AXIOM_(FRAME_TIMING|POSTWORKER_COMMIT|GPU_RETIREMENT|.*LOSS)/.test(args[0])){
   if(native.length<maximum)native.push({at:performance.now(),text:args[0]});else ++loss;
  }return original.apply(this,args);
 };}
 window.postworkerObservation={enable(){active=true;},finish(){active=false;observer.disconnect();return{raf,longTasks,native,loss,policy:'bounded12000each; no observer-ownedRAF; timing excludesconsolewritewithin nativeStage'}}};
}
export async function consumer(c){
 const {createRuntime}=await import('/sdk/axiom.mjs');
 const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={id:c.id,status:'STARTED',events:[],operations:[],frames:[],errors:[],stages:{},snapshots:{}};
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(x,m)=>{if(!x)throw Error(m);};
 let sdk,sequence=90000000000;const operations=[],witness=window.cpuPendingWitness;
 const state=()=>sdk.readState();
 const privateFacts=()=>{const m=window.cpuPendingModule;if(!m)return null;const p=m.stringToNewUTF8(JSON.stringify({command:'cpuPending'}));try{return JSON.parse(m.UTF8ToString(m._axiom_js_call(p,0,0)));}finally{m._free(p);}};
 const synchronize=()=>{const m=window.cpuPendingModule;if(!m)return null;
  const p=m.stringToNewUTF8(JSON.stringify({command:'cpuPending',action:'clock'}));const before=performance.now();
  try{const value=JSON.parse(m.UTF8ToString(m._axiom_js_call(p,0,0)));const after=performance.now();return{before,after,nowNs:value.nowNs};}finally{m._free(p);}
 };
 const observe=label=>{const s=state();row.stages[label]={at:performance.now(),state:s,privateFacts:privateFacts()};return s;};
 const save=()=>window.persistMetadata(row);
 const pointer=(phase,x,y)=>{sequence+=8000000;const b=sdk.canvas.getBoundingClientRect();
  const event=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'pen',clientX:b.left+x,clientY:b.top+y,
   pressure:.5,buttons:['up','cancel'].includes(phase)?0:1,bubbles:true,cancelable:true});
  Object.defineProperty(event,'timeStamp',{value:sequence/1e6});const at=performance.now();sdk.canvas.dispatchEvent(event);
  row.events.push({phase,x,y,at,end:performance.now()});
 };
 const stable=async()=>{const at=performance.now();let quiet=0;
  for(;;){const s=state();if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration){if(++quiet===3)return performance.now();}else quiet=0;
   check(performance.now()-at<160000,'pending drain limit');await pause(16);}
 };
 const capture=async label=>{const captureStart=performance.now();const s=state();const saved=await sdk.captureSnapshot({generation:s.generation,
  documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});
  const snapshotEncodedAt=performance.now();await sendBinaryChunks(label+'.axsnap',saved.bytes,window.persistChunk);
  const read=readWebGLTarget(sdk.canvas,{role:'canonical'});check(read.metadata.status==='VALID','readback '+label);
  await sendBinaryChunks(label+'.rgba',read.bytes,window.persistChunk);
  row.snapshots[label]={captureStart,snapshotEncodedAt,captureEnd:performance.now(),state:s,readback:read.metadata,through:operations.length,metadata:{generation:s.generation,
   documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier,checkpointDigest:saved.checkpointDigest,resourceBinding:'1'}};
  await save();
 };
 const wheel=async()=>{const b=sdk.canvas.getBoundingClientRect();for(const [ctrl,dx,dy]of [[false,24,12],[true,0,-9]]){
  const at=performance.now();sdk.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:b.left+500,clientY:b.top+300,ctrlKey:ctrl,deltaX:dx,deltaY:dy,bubbles:true,cancelable:true}));
  row.events.push({kind:'wheel',ctrl,dx,dy,at,end:performance.now()});await pause(16);}
 };
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,
   restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',
   onError:e=>row.errors.push({code:e.code,text:String(e)}),
   onOperation:b=>{operations.push(b);const s=state();row.operations.push({at:performance.now(),bytes:b.length,generation:s.generation,commit:s.lastCommit});},
   onFrame:f=>{const s=state();row.frames.push({at:performance.now(),frame:f,presentedGeneration:s.lastCommit.presentedGeneration});}});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;window.postworkerObservation?.enable();
  await stable();check(state().objects===40,'Fixed40');row.build=sdk.readBuildInfo();row.clockStart=synchronize();const initial=observe('initial');
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});
  const down=performance.now();pointer('down',420,160);
  for(let i=1;i<=7200;++i){pointer('move',420+7*Math.sin(i*.17),160+170*i/7200);
   if(i%8===0)await pause(c.paced?30000/7200*8:0);
  }
  row.A_down_to_up_ms=performance.now()-down;const up=performance.now();pointer('up',999,999);observe('A-Up');
  pointer('down',300,230);observe('B-Down');for(let i=1;i<=8;++i)pointer('move',300,230+i*4);pointer('up',999,999);observe('B-Up');
  if(c.navigation)await wheel();
  const settled=await stable();observe('AB-settled');row.up_to_drain_ms=settled-up;
  row.first_A_commit_ms=row.operations.find(v=>v.at>=up&&BigInt(v.generation)>BigInt(initial.generation))?.at-up;
  row.first_A_present_ms=row.frames.find(v=>v.at>=up&&BigInt(v.presentedGeneration)>BigInt(initial.generation))?.at-up;
  row.first_AB_present_ms=row.frames.find(v=>v.at>=up&&BigInt(v.presentedGeneration)>=BigInt(initial.generation)+2n)?.at-up;
  row.observation_policy='onFrame/onOperation capture one readState; primary interval includes matching observer overhead in both variants';
  row.worker={jobs:witness.audit.jobs,terminals:witness.audit.terminals,errors:witness.audit.errors};await save();
  check(row.errors.length===0,'root SDK errors');
  await capture('AB');
  // Reference/serialization occurs after the primary timing interval.
  observe('before-undo');await sdk.undo();await stable();observe('undo-settled');await capture('undo');observe('before-redo');await sdk.redo();await stable();observe('redo-settled');await capture('redo');
  if(c.followup){
   observe('before-C');const at=performance.now();const camera=state().camera;
   pointer('down',350*camera.scale+camera.x,210*camera.scale+camera.y);
   pointer('move',360*camera.scale+camera.x,230*camera.scale+camera.y);pointer('up',999,999);observe('C-Up');
   await stable();row.followup_erase_ms=performance.now()-at;observe('followup-erase');
   await sdk.selectTool('select');await sdk.setSelectionMode('rectangle');
   const beforeSelection=state();const selectionAt=performance.now();pointer('down',80*camera.scale+camera.x,90*camera.scale+camera.y);
   pointer('move',620*camera.scale+camera.x,480*camera.scale+camera.y);pointer('up',620*camera.scale+camera.x,480*camera.scale+camera.y);
   const wait=performance.now();for(;;){const s=state();if(!s.activeContacts&&BigInt(s.selectionRevision)>BigInt(beforeSelection.selectionRevision)&&
      JSON.stringify([...s.selected].sort())===JSON.stringify([...c.fixture.objectIds].sort()))break;
    check(performance.now()-wait<30000,'masked selection incomplete');await pause(16);}
   row.masked_selection_ms=performance.now()-selectionAt;row.masked_selection=sdk.readSelection();
   row.selection_completion='public selectionRevision advanced; complete original40 targetID set; no partial set accepted';
   await sdk.setSelectionMode('point');pointer('down',1300,950);pointer('up',1300,950);await stable();
   await capture('followup');
  }
  row.worker={jobs:witness.audit.jobs,terminals:witness.audit.terminals,errors:witness.audit.errors};
  row.clockEnd=synchronize();row.observation=window.postworkerObservation?.finish();row.entropy=window.exactFixtureEntropy.transcript();row.status='OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{observe('failure');}catch{}}
 finally{
  if(!row.observation)row.observation=window.postworkerObservation?.finish();
  for(let i=0;i<operations.length;++i)await sendBinaryChunks('operation-'+i+'.axsnap',operations[i],window.persistChunk);
  if(sdk)try{if(state().activeContacts)pointer('cancel',0,0);await sdk.close();row.closed=true;}catch(e){row.cleanupError=String(e);}
  if(witness)row.worker={jobs:witness.audit.jobs,terminals:witness.audit.terminals,errors:witness.audit.errors};
  await save();
 }
 return row;
}
export async function run(request,out){
 await fs.mkdir(out,{recursive:true});let browser,page,server;const chunks=new Map(),consoleLines=[];
 const result={id:request.id,status:'STARTED',errors:[]};
 try{
  server=http.createServer(async(req,res)=>{try{const name=new URL(req.url,'http://local').pathname;let file;
   if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{consumer}from"/driver.mjs";window.run=consumer;</script>');return;}
   if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
   if(name==='/sdk/internal/axiom.mjs'&&request.diagnostic){res.setHeader('Content-Type','text/javascript');res.end("import factory from '/engine/internal/axiom.mjs';export default async o=>{const m=await factory(o);window.cpuPendingModule=m;return m;};");return;}
   if(name.startsWith('/engine/'))file=path.join(request.sdk,name.slice(8));
   else if(name==='/fixture.axsnap')file=request.fixture.path;
   else if(name.startsWith('/sdk/'))file=path.join(request.sdk,name.slice(5));
   else if(name.startsWith('/g5/'))file=path.join(process.cwd(),'verification/runners/g5',name.slice(4));
   if(!file)throw Error('unknown route');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
  }catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:request.dpr});
  await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});
  await page.addInitScript(installWorkerWitness);await page.addInitScript(installPostworkerObserver);
  page.on('console',v=>consoleLines.push({type:v.type(),text:v.text()}));page.on('pageerror',e=>result.errors.push(String(e)));
  await page.exposeFunction('persistMetadata',v=>fs.writeFile(path.join(out,'raw.json'),JSON.stringify(v,null,2)+'\n'));
  await page.exposeFunction('persistChunk',async c=>{const next=validateChunk(c,chunks.get(c.name));const file=path.join(out,c.name+'.partial');const handle=await fs.open(file,c.offset?'r+':'wx');try{await handle.write(Buffer.from(c.base64,'base64'),0,next.size,c.offset);}finally{await handle.close();}if(next.complete){await pipeline(createReadStream(file),createGzip(),createWriteStream(path.join(out,c.name+'.gz')));await fs.unlink(file);}chunks.set(c.name,next);return{nextOffset:next.nextOffset};});
  await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);
  const row=await page.evaluate(c=>window.run(c),request);result.status=row.status;result.error=row.error;result.cleanupError=row.cleanupError;
  if(result.errors.length||row.cleanupError)result.status='FAILED';
 }catch(e){result.status='FAILED';result.error=String(e.stack??e);}
 finally{if(page)await page.close();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const result=await run(JSON.parse(await fs.readFile(process.argv[2],'utf8')),process.argv[3]);console.log(JSON.stringify(result));if(result.status!=='OBSERVED')process.exitCode=1;}
