import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {validateChunk} from './ur2d_selection_up_contract.mjs';

export async function consumer(config){
 const {createRuntime}=await import('/sdk/axiom.mjs');
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const pause=ms=>new Promise(r=>setTimeout(r,ms));
 const hash=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),x=>x.toString(16).padStart(2,'0')).join('');
 const check=(v,m)=>{if(!v)throw Error(m);};
 const results=[];let sdk,clock=90000000000,observer;
 const persist=(name,bytes)=>sendBinaryChunks(name,bytes,window.persistChunk);
 const stage=async(name,detail={})=>window.persistStage({stage:name,at:performance.now(),...detail});
 const state=()=>sdk.readState();
 const pointer=(phase,x,y,id=1)=>{
  const rect=sdk.canvas.getBoundingClientRect();clock+=16666667;
  const event=new PointerEvent('pointer'+phase,{pointerId:id,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
  Object.defineProperty(event,'timeStamp',{value:clock/1e6});
  const before=state().activeContacts,start=performance.now();observer.dispatch(sdk.canvas,event);
  const admitted=state().activeContacts;return{phase,id,x,y,start,end:performance.now(),beforeContacts:before,afterContacts:admitted};
 };
 const settle=async()=>{const deadline=performance.now()+30000;for(;;){const s=state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;check(performance.now()<deadline,'canonical settle deadline');await pause(20);}};
 const record=label=>({label,state:state(),selection:sdk.readSelection()});
 const snapshot=async label=>{
  const s=state(),request={generation:s.generation,documentRevision:s.generation,recoveryFrontier:[8,1,16,1,24,1]};
  await stage('snapshot-start',{label});const v=await sdk.captureSnapshot(request);await persist(label+'.axsnap',v.bytes);
  return {sha256:await hash(v.bytes),byteLength:v.bytes.length,state:s,metadata:{documentRevision:request.documentRevision,recoveryFrontier:request.recoveryFrontier,checkpointDigest:v.checkpointDigest,resourceBinding:sdk.resources.list().binding}};
 };
 const pixels=async label=>{await stage('pixels-start',{label});const r=readWebGLTarget(sdk.canvas,{role:'canonical'});check(r.bytes&&r.metadata.status==='VALID','actual canonical pixels unavailable');await persist(label+'.rgba',r.bytes);return{sha256:await hash(r.bytes),metadata:r.metadata};};
 for(const fixture of config.fixtures){
  const errors=[],operations=[];let frameCount=0;const bytes=new Uint8Array(await(await fetch('/fixtures/'+fixture.name+'.axsnap')).arrayBuffer());
  observer=installNavigationObserver(()=>sdk?.canvas);await stage('open-start',{fixture:fixture.name});
  check(await hash(bytes)===fixture.sha256,'fixture sha256');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:fixture.documentId,restore:{bytes,metadata:fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>operations.push({at:performance.now(),bytes:b.length}),onFrame:()=>++frameCount});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await settle();const row={fixture:fixture.name,initial:record('initial'),errors,operations,selection:[],erase:[]};window.partial=results;results.push(row);await stage('opened',{fixture:fixture.name,state:state()});
  await sdk.selectTool('select');await sdk.setSelectionMode('point');
  await stage('creation-start',{fixture:fixture.name});await sdk.selectShape('rectangle');pointer('move',1000,150);pointer('down',1000,150);pointer('up',1000,150);await stage('creation-up',{fixture:fixture.name,state:state()});await settle();
  row.created=record('rectangle');await stage('placed',{fixture:fixture.name,state:state()});await sdk.setSelectionMode('point');
  const repetitions=config.id==='U2D1-OLD-F'||config.id==='U2D1-COMPARE'?1:3;
  for(let repeat=0;repeat<repetitions;repeat++){
   await stage('selection-start',{fixture:fixture.name,repeat});const observation={kind:'selection',repeat};observer.begin(observation);
   const events=[pointer('down',30,30),pointer('move',35,35),pointer('move',235,205),pointer('up',235,205),pointer('down',1300,850),pointer('up',1300,850)];
   check(events[0].afterContacts===1&&events[2].afterContacts===1&&events[3].afterContacts===0&&events[4].afterContacts===1,'selection did not enter active drag/reentry');
   await pause(20);observer.end(observation);const after=record('point-blank-terminal');check(after.state.activeContacts===0,'selection Up retained contact');
   row.selection.push({repeat,events,after,observation});await stage('selection-terminal',{fixture:fixture.name,repeat,state:state()});
  }
  if(config.id!=='U2D1-OLD-F'&&config.id!=='U2D1-COMPARE'){
   for(const [mode,terminal]of [['rectangle','cancel'],['lasso','lost'],['point','blur']]){
    await sdk.setSelectionMode(mode);const before=record('before-'+mode);check(pointer('down',35,35,7).afterContacts===1,'Cancel fixture Down rejected');pointer('move',235,205,7);check(state().activeContacts===1,'Cancel fixture not active');
    if(terminal==='cancel')pointer('cancel',235,205,7);
    else if(terminal==='lost'){const e=new PointerEvent('lostpointercapture',{pointerId:7,pointerType:'mouse',bubbles:true});Object.defineProperty(e,'timeStamp',{value:(clock+1)/1e6});sdk.canvas.dispatchEvent(e);}
    else window.dispatchEvent(new Event('blur'));
    check(state().activeContacts===0,'source lost did not terminally cancel');check(pointer('down',1320,850,7).afterContacts===1,'source lost immediateDown rejected');pointer('up',1320,850,7);check(state().activeContacts===0,'selection Cancel/source-lost retained contact');row.selection.push({mode,terminal,before,after:record('after-'+mode)});
   }
  }
  if(config.id!=='U2D1-A-F'){
   await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();
   for(let repeat=0;repeat<3;repeat++){
    const before=state(),count=operations.length;const observation={kind:'erase-immediate',repeat};observer.begin(observation);
    const x=465+repeat*22,y=180;const events=[pointer('down',x,y),pointer('move',x+2,y+15),pointer('move',x+5,y+30),pointer('up',x+5,y+30),pointer('down',x+10,y+35),pointer('move',x+12,y+50),pointer('up',x+12,y+50)];
    // Deliberately no settle or await between Up and subsequent Down/wheel.
    const wheel=new WheelEvent('wheel',{clientX:600,clientY:400,deltaX:8,deltaY:5,bubbles:true,cancelable:true});const start=performance.now();sdk.canvas.dispatchEvent(wheel);events.push({phase:'wheel',start,end:performance.now()});
    check(events[0].afterContacts===1&&events[3].afterContacts===0&&events[4].afterContacts===1&&events[6].afterContacts===0,'erase immediate Down/Up admission failure');
    await settle();observer.end(observation);check(state().activeContacts===0,'Up reentry retained contact');
    check(operations.length-count===2&&BigInt(state().generation)===BigInt(before.generation)+2n,'exact proven erase did not commit two operations');
    row.erase.push({repeat,before,events,operation_delta:operations.length-count,after:record('erase-final'),observation});
   }
   const before=state().generation;pointer('down',1300,900,11);pointer('move',1310,900,11);pointer('cancel',1310,900,11);check(state().activeContacts===0&&state().generation===before,'erase cancel mutated document');
  }
  await settle();row.final=await snapshot(fixture.name+'-final');row.pixels=await pixels(fixture.name+'-final');
  if(config.id==='U2D1-B-F'||config.id==='U2D1-COMPARE'){
   await sdk.undo();await settle();row.undo=await snapshot(fixture.name+'-undo');await sdk.redo();await settle();row.redo=await snapshot(fixture.name+'-redo');row.redoPixels=await pixels(fixture.name+'-redo');
   // Exact file hashes remain metadata. The frozen comparator owns pixel
   // acceptance and permits <=2 channel levels outside its independent band.
   row.roundtrip=await window.compareRoundtrip(fixture.name);check(row.roundtrip.status==='QUALIFIED','Undo/Redo pixel policy failed');
  }
  row.frameCount=frameCount;check(errors.length===0,'public handler errors');await window.persistRow(row);observer.restore();await sdk.close();document.getElementById('mount').replaceChildren();sdk=null;
 }
 return {rows:results};
}

async function main(){
 const request=JSON.parse(await fs.readFile(process.argv[2],'utf8')),out=path.resolve(process.argv[3]);await fs.mkdir(out,{recursive:true});
 const root=path.resolve(new URL('../../..',import.meta.url).pathname);const result={id:request.id,status:'STARTED',errors:[]};let browser,server,page;
 try{
  server=http.createServer(async(req,res)=>{try{
   const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
   if(name==='/') {res.setHeader('Content-Type','text/html');res.end(`<meta charset="utf-8"><style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import {consumer} from '/driver.mjs';window.run=consumer;</script>`);return;}
   if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
   else if(name.startsWith('/sdk/'))file=path.join(request.sdk,name.slice(5));
   else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));
   else if(name.startsWith('/fixtures/'))file=request.fixtures.find(f=>'/fixtures/'+f.name+'.axsnap'===name)?.path;
   if(!file)throw Error('missing');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
  }catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  result.console=[];result.consoleDropped=0;
  const openPage=async runOut=>{
  await fs.mkdir(runOut,{recursive:true});page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{if(m.type()==='error'){if(result.console.length<256)result.console.push(m.text());else ++result.consoleDropped;}});
  await page.exposeFunction('persistStage',async row=>{await fs.appendFile(path.join(runOut,'stages.jsonl'),JSON.stringify(row)+'\n');console.log(JSON.stringify(row));});
  await page.exposeFunction('persistRow',async row=>fs.writeFile(path.join(runOut,'row-'+row.fixture+'.json'),JSON.stringify(row,null,2)+'\n'));
  await page.exposeFunction('compareRoundtrip',async name=>{
   if(!/^S[02]$/.test(name))throw Error('unknown fixture');
   const {comparePixels}=await import('./ur0/pixel_oracle.mjs');
   const a=await fs.readFile(path.join(runOut,name+'-final.rgba')),b=await fs.readFile(path.join(runOut,name+'-redo.rgba'));
   const comparison=comparePixels(b,a,new Uint8Array(a.length/4));
   await fs.writeFile(path.join(runOut,name+'-roundtrip.json'),JSON.stringify(comparison,null,2)+'\n');return comparison;
  });
  const transfers=new Map();
  await page.exposeFunction('persistChunk',async chunk=>{
   if(transfers.size>=32&&!transfers.has(chunk.name))throw Error('transfer cardinality exceeded');
   const prior=transfers.get(chunk.name);if(prior?.complete)throw Error('completed transfer replay');
   const next=validateChunk(chunk,prior),bytes=Buffer.from(chunk.base64,'base64');
   const temporary=path.join(runOut,chunk.name+'.partial'),final=path.join(runOut,chunk.name);
   const handle=await fs.open(temporary,chunk.offset===0?'wx':'r+');
   try{const written=await handle.write(bytes,0,bytes.length,chunk.offset);if(written.bytesWritten!==bytes.length)throw Error('partial buffer write');}finally{await handle.close();}
   if(next.complete){await fs.rename(temporary,final);await fs.appendFile(path.join(runOut,'buffers.jsonl'),JSON.stringify({name:chunk.name,bytes:next.total,sha256:createHash('sha256').update(await fs.readFile(final)).digest('hex')})+'\n');}
   transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};
  });
  await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>typeof window.run==='function');
  };
  const variants=request.id==='U2D1-COMPARE'?request.variants:[{name:'candidate',sdk:request.sdk}];
  if(request.id==='U2D1-COMPARE'&&(!Array.isArray(variants)||variants.length!==2||new Set(variants.map(v=>v.name)).size!==2))throw Error('paired comparison requires old and candidate SDKs');
  result.runs=[];
  for(let repetition=0;repetition<(request.id==='U2D1-COMPARE'?3:1);repetition++){
   for(const variant of variants){
    request.sdk=variant.sdk;
    const sdkFiles={};for(const name of ['axiom.mjs','internal/axiom.mjs','axiom.wasm','axiom.data'])sdkFiles[name]=createHash('sha256').update(await fs.readFile(path.join(variant.sdk,name))).digest('hex');
    await fs.appendFile(path.join(out,'variants.jsonl'),JSON.stringify({name:variant.name,repetition,sdk:variant.sdk})+'\n');
    // Fresh page prevents imported module identity from crossing old/candidate.
    if(page)await page.close();await openPage(path.join(out,variant.name+'-'+repetition));
    const value=await page.evaluate(config=>window.run(config),{...request,variant:variant.name,repetition});
    result.runs.push({variant:variant.name,repetition,sdkFiles,rows:value.rows});
   }
  }
  result.rows=result.runs[0]?.rows;
  result.status=result.errors.length?'FUNCTIONAL_FAILURE':'OBSERVED';
 }catch(e){result.status='FUNCTIONAL_OR_HARNESS_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.partial);}catch{}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));if(result.status!=='OBSERVED')process.exitCode=1;}
}
if(process.argv[1]===new URL(import.meta.url).pathname)await main();
