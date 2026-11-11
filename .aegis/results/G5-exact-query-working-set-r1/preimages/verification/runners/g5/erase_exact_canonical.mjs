import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {chromium} from '../../node_modules/playwright/index.mjs';import {createHash}from'node:crypto';
import{qualifyPersistedRoundtrip}from'./ur2d_selection_up.mjs';import{validateChunk}from'./ur2d_selection_up_contract.mjs';
import{WINDOWS,qualifyRounds}from'./erase_exact_canonical_contract.mjs';
export async function consumer(c){
 const{createRuntime}=await import('/sdk/axiom.mjs');const{installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const{tracePoint,qualifyRounds}=await import('/g5/erase_exact_canonical_contract.mjs');
 const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={variant:c.variant,repetition:c.repetition,rounds:[],errors:[],operations:[],frames:[],functional:[],captures:{}};
 let sdk,clock=90000000000;const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 const observer=installNavigationObserver(()=>sdk?.canvas);const state=()=>sdk.readState();window.partial=row;
 const pointer=(phase,x,y)=>{clock+=8000000;const rect=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);return{phase,x,y,start,end:performance.now(),contacts:state().activeContacts,generation:state().generation};};
 const settle=async()=>{const start=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now()-start;check(performance.now()-start<20000,'stable exact demand did not converge');await pause(20);}};
 const capture=async stage=>{await settle();const s=state();const snap=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks('S2-'+stage+'.axsnap',snap.bytes,window.persistChunk);const pixels=readWebGLTarget(sdk.canvas,{role:'canonical'});check(pixels.metadata.status==='VALID'&&pixels.bytes,'canonical readback unavailable');await sendBinaryChunks('S2-'+stage+'.rgba',pixels.bytes,window.persistChunk);return{sha256:await digest(snap.bytes),byteLength:snap.bytes.length,state:s,rgba_sha256:await digest(pixels.bytes),readback:pixels.metadata};};
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===c.fixture.sha256,'fixture hash');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:f=>{if(row.frames.length<4096)row.frames.push({at:performance.now(),presented:f.presented});}});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();await sdk.selectTool('select');await sdk.setSelectionMode('point');pointer('down',400,120);pointer('up',400,120);await settle();check(sdk.readSelection().count===1,'select original stroke');
  for(let i=0;i<24;++i)await sdk.duplicateSelection({offset:[0,0]});await settle();await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();check(state().objects===40,'40 same bodies');row.initial=state();row.build=sdk.readBuildInfo();
  console.log(JSON.stringify({stage:'ready',variant:c.variant,repetition:c.repetition}));
  const count=c.stress?10:3;
  for(let round=0;round<count;++round){
   const observation={round};observer.begin(observation);const before=state(),op=row.operations.length,events=[];let[x,y]=tracePoint(round,0);events.push(pointer('down',x,y));check(events.at(-1).contacts===1,'main Down reject');
   for(let i=1;i<=160;++i){[x,y]=tracePoint(round,i);events.push(pointer('move',x,y));if(i%8===0)await pause(0);}
   const main=pointer('up',x,y);events.push(main);const next=pointer('down',x+1,y+1);events.push(next);events.push(pointer('move',x+2,y+3));const tiny=pointer('up',x+2,y+3);events.push(tiny);
   const lastUp=performance.now();if(!c.stress||round===count-1)await pause(6000);
   const after=state(),activity=observer.activity();observer.end(observation);
   const r={round,before,events,main,mainCommitted:BigInt(main.generation)===BigInt(before.generation)+1n,mainContacts:main.contacts,immediateDownContacts:next.contacts,tinyContacts:tiny.contacts,lastUp,after,activity,operationDelta:c.stress?Number(BigInt(after.generation)-BigInt(before.generation)):row.operations.length-op,operationCallbacksObserved:row.operations.length-op,generationDelta:Number(BigInt(after.generation)-BigInt(before.generation)),observation};row.rounds.push(r);await window.persistMetadata(row);console.log(JSON.stringify({stage:'round',variant:c.variant,repetition:c.repetition,round,main_ms:main.end-main.start,state:after,activity}));
  }
  row.qualification=qualifyRounds(row.rounds,count,{stress:!!c.stress});check(row.qualification.status==='QUALIFIED','round operation/contact/idle proof');
  // Separate functional gesture: actual Up immediately followed by pan/zoom,
  // cancellation and no-op. The primary paired waveform above stays exact.
  const functionalBefore=state().generation;
  row.functional.push(pointer('down',300,230),pointer('move',300,245),pointer('up',300,260));
  row.functionalHit=BigInt(state().generation)===BigInt(functionalBefore)+1n;
  const immediateWheel=new WheelEvent('wheel',{clientX:600,clientY:400,deltaX:4,deltaY:2,bubbles:true,cancelable:true});sdk.canvas.dispatchEvent(immediateWheel);
  const prior=state().generation;row.functional.push(pointer('down',1300,850),pointer('move',1310,855),pointer('cancel',1310,855));check(state().generation===prior&&state().activeContacts===0,'cancel mutation');
  row.functional.push(pointer('down',1300,900),pointer('up',1300,900));check(state().generation===prior,'blank no-op committed');
  for(const ctrlKey of[false,true]){const wheel=new WheelEvent('wheel',{clientX:600,clientY:400,ctrlKey,deltaX:ctrlKey?0:4,deltaY:ctrlKey?-2:2,bubbles:true,cancelable:true});sdk.canvas.dispatchEvent(wheel);}
  row.functional_settle_ms=await settle();
  // HUD is explicitly absent for independent CPU comparison.
  await sdk.selectTool('select');pointer('down',1300,900);pointer('up',1300,900);await settle();check(sdk.readSelection().count===0,'empty HUD basis');
  row.final=await capture('final');await sdk.undo();await settle();row.undo=await capture('undo');await sdk.redo();await settle();row.redo=await capture('redo');
  row.pixel_basis={width:sdk.canvas.width,height:sdk.canvas.height,logical_width:1431,logical_height:1010,camera:state().camera,final:row.final,redo:row.redo};check(row.errors.length===0,'handler errors');row.status='QUALIFIED';return row;
 }catch(error){row.status='FAILED';row.error=String(error.stack??error);throw error;}
 finally{observer.restore();if(sdk)await sdk.close();await window.persistMetadata(row);}
}
export async function worker(request,out){
 if(!WINDOWS[request.id])throw Error('foreign window');await fs.mkdir(out,{recursive:true});const root=path.resolve(new URL('../../..',import.meta.url).pathname);let browser,server,page,activeSdk;
 const result={id:request.id,status:'STARTED',errors:[],rows:[]};
 try{
  server=http.createServer(async(req,res)=>{try{const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
   if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{consumer}from"/driver.mjs";window.run=consumer;</script>');return;}
   if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
   if(name.startsWith('/sdk/'))file=path.join(activeSdk,name.slice(5));else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));else if(name==='/fixture.axsnap')file=request.fixture.path;
   if(!file)throw Error('missing');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  const runs=request.id==='G5EC1-FINAL-PAIRS'?[0,1,2].flatMap(repetition=>request.variants.map(v=>({...v,repetition}))):request.variants.map(v=>({...v,repetition:0}));
  for(const run of runs){activeSdk=run.sdk;const directory=path.join(out,run.name+'-'+run.repetition);await fs.mkdir(directory);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));page.on('crash',()=>result.errors.push('crash'));page.on('console',m=>{try{const x=JSON.parse(m.text());if(x.stage)console.log(JSON.stringify(x));}catch{}});
   await page.exposeFunction('persistMetadata',r=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(r,null,2)+'\n'));const transfers=new Map();
   await page.exposeFunction('persistChunk',async chunk=>{const previous=transfers.get(chunk.name);if(previous?.complete)throw Error('transfer replay');const next=validateChunk(chunk,previous),bytes=Buffer.from(chunk.base64,'base64'),temporary=path.join(directory,chunk.name+'.partial');const h=await fs.open(temporary,chunk.offset?'r+':'wx');try{const r=await h.write(bytes,0,bytes.length,chunk.offset);if(r.bytesWritten!==bytes.length)throw Error('shortwrite');}finally{await h.close();}if(next.complete)await fs.rename(temporary,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};});
   await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>typeof window.run==='function');const row=await page.evaluate(c=>window.run(c),{fixture:request.fixture,variant:run.name,repetition:run.repetition,stress:request.id==='G5EC1-STRESS'});await page.close();page=null;
   row.roundtrip=await qualifyPersistedRoundtrip({directory,name:'S2',basis:row.pixel_basis,oracle:request.oracle});if(row.roundtrip.status!=='QUALIFIED')throw Error('pixel qualification failed');await fs.writeFile(path.join(directory,'qualified-row.json'),JSON.stringify(row,null,2)+'\n');result.rows.push(row);
  }
  if(result.errors.length)throw Error('browser errors');result.status='OBSERVED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);if(page)try{result.partial=await page.evaluate(()=>window.partial);}catch{}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await worker(request,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
