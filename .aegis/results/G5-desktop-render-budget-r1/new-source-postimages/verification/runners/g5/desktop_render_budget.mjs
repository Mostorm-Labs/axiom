import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {qualifyPersistedRoundtrip} from './ur2d_selection_up.mjs';
import {validateChunk} from './ur2d_selection_up_contract.mjs';

const sha=b=>createHash('sha256').update(b).digest('hex');
export function budgetCases(){
 return [600,1431].flatMap(width=>['shape','stroke'].flatMap(kind=>['old','candidate'].map(variant=>({
  width,height:width===600?900:1010,dpr:2,kind,variant,id:`${width}-${kind}-${variant}`}))));
}
export function validateBudgetRequest(request){
 if(request.id!=='G5MB1-AREA-COMPARE'||request.variants?.length!==2||
    request.variants[0].name!=='old'||request.variants[1].name!=='candidate'||
    request.variants[0].sdk===request.variants[1].sdk||request.fixture?.name!=='S2'||
    !/^[0-9a-f]{64}$/.test(request.fixture.sha256)||
    request.oracle?.sha256!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')
  throw Error('invalid frozen area comparison request');
 return budgetCases();
}

export async function budgetConsumer(config){
 const {createRuntime}=await import('/sdk/axiom.mjs');
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const {boundedDisplayOpportunity}=await import('/g5/prepared_body_index_reverify_pacing.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const digest=async bytes=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),v=>v.toString(16).padStart(2,'0')).join('');
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(ok,msg)=>{if(!ok)throw Error(msg);};
 const row={...config.spec,errors:[],operations:[],events:[],captures:{}};window.areaPartial=row;
 let sdk,clock=90000000000,observer;const state=()=>sdk.readState();
 const pointer=(phase,x,y)=>{
  clock+=16666667;const rect=sdk.canvas.getBoundingClientRect();
  const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,
   button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
  Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);
  const event={phase,x,y,start,end:performance.now(),contacts:state().activeContacts};row.events.push(event);return event;
 };
 const settle=async()=>{const start=performance.now();for(;;){
  const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now()-start;
  check(performance.now()-start<20000,'canonical settle bound');await pause(10);
 }};
 const clearSelection=async()=>{pointer('down',30,850);pointer('up',30,850);await settle();check(sdk.readSelection().count===0,'blank click did not clear selection');};
 const snapshot=async stage=>{
  const s=state(),value=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(config.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:config.fixture.metadata.recoveryFrontier});
  await sendBinaryChunks('S2-'+stage+'.axsnap',value.bytes,window.areaPersist);
  const image=readWebGLTarget(sdk.canvas,{role:'canonical'});check(image.metadata.status==='VALID'&&image.bytes,'qualified target unavailable');
  await sendBinaryChunks('S2-'+stage+'.rgba',image.bytes,window.areaPersist);
  const result={sha256:await digest(value.bytes),byteLength:value.bytes.length,state:s,rgba_sha256:await digest(image.bytes),readback:image.metadata};row.captures[stage]=result;return result;
 };
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===config.fixture.sha256,'fixture identity');
  observer=installNavigationObserver(()=>sdk?.canvas);
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:config.fixture.documentId,
   restore:{bytes,metadata:config.fixture.metadata},builtinFonts:false,permission:'localSingleUser',
   onError:e=>row.errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();row.build=sdk.readBuildInfo();
  await sdk.selectShape('rectangle');pointer('move',400,650);pointer('down',400,650);pointer('up',400,650);await settle();
  const shape=sdk.readSelection();check(shape.count===1,'shape creation selection');row.shape_id=shape.primary;
  await sdk.selectTool('select');await sdk.setSelectionMode('point');
  pointer('down',400,120);pointer('up',400,120);await settle();const stroke=sdk.readSelection();
  check(stroke.count===1&&stroke.primary!==row.shape_id,'frozen stroke point selection');
  await sdk.duplicateSelection({offset:[0,0]});await settle();row.stroke_id=sdk.readSelection().primary;
  check(state().objects===18,'fixed mixed fixture cardinality');
  const [x,y]=config.spec.kind==='shape'?[450,690]:[400,120];
  pointer('down',x,y);pointer('up',x,y);await settle();
  const selected=sdk.readSelection();check(selected.count===1&&selected.primary===(config.spec.kind==='shape'?row.shape_id:row.stroke_id),'actual active object identity');
  row.initial={state:state(),selection:selected};const count=row.operations.length;
  const observation={};observer.begin(observation);row.down=pointer('down',x,y);check(state().activeContacts===1,'drag Down rejected');
  row.moves=[];for(const [dx,dy]of [[4,2],[11,4],[18,6]]){row.moves.push(pointer('move',x+dx,y+dy));await boundedDisplayOpportunity({timeoutMs:5000});}
  row.up=pointer('up',x+18,y+6);row.completion_wait_ms=await settle();observer.end(observation);row.observation=observation;
  row.after={state:state(),selection:sdk.readSelection()};
  check(row.operations.length-count===1&&BigInt(row.after.state.generation)===BigInt(row.initial.state.generation)+1n,'drag did not commit exactly once');
  check(JSON.stringify(row.after.selection.transform)!==JSON.stringify(selected.transform),'drag transform unchanged');
  check(row.after.state.activeContacts===0&&row.after.state.lastCommit.presentedGeneration===row.after.state.generation,'drag receipt/contact');
  const prior=state().generation;pointer('down',x+18,y+6);pointer('move',x+24,y+8);pointer('cancel',x+24,y+8);await settle();
  check(state().generation===prior&&state().activeContacts===0,'drag cancel mutated document');row.cancel_generation=prior;
  await clearSelection();row.final=await snapshot('final');
  await sdk.undo();await settle();row.undo=await snapshot('undo');
  await sdk.redo();await settle();row.redo=await snapshot('redo');
  row.pixel_basis={width:sdk.canvas.width,height:sdk.canvas.height,logical_width:config.spec.width,logical_height:config.spec.height,
   camera:state().camera,final:row.final,redo:row.redo};
  check(row.errors.length===0,'public handler errors');row.status='OBSERVED';return row;
 }catch(error){row.status='FAILED';row.error=String(error.stack??error);throw error;}
 finally{if(observer)observer.restore();if(sdk)await sdk.close();await window.areaMetadata(row);}
}

export async function worker(requestPath,out){
 const request=JSON.parse(await fs.readFile(requestPath,'utf8')),cases=validateBudgetRequest(request);
 await fs.mkdir(out,{recursive:true});const root=path.resolve(new URL('../../..',import.meta.url).pathname);
 let browser,server,page,activeSdk;const result={id:request.id,status:'STARTED',rows:[],errors:[]};
 const write=(name,value)=>fs.writeFile(path.join(out,name),JSON.stringify(value,null,2)+'\n');
 try{
  server=http.createServer(async(req,res)=>{try{
   const u=new URL(req.url,'http://localhost'),name=decodeURIComponent(u.pathname);let file;
   if(name==='/'){const width=Number(u.searchParams.get('width')),height=Number(u.searchParams.get('height'));
    res.setHeader('Content-Type','text/html');res.end(`<body style="margin:0"><div id="mount" style="width:${width}px;height:${height}px"></div><script type="module">import{budgetConsumer}from'/driver.mjs';window.run=budgetConsumer;</script>`);return;}
   if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+budgetConsumer.toString());return;}
   if(name.startsWith('/sdk/'))file=path.join(activeSdk,name.slice(5));
   else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));
   else if(name==='/fixture.axsnap')file=request.fixture.path;
   if(!file)throw Error('not found');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
  }catch(error){res.statusCode=404;res.end(String(error));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of cases){
   activeSdk=request.variants.find(v=>v.name===spec.variant).sdk;const directory=path.join(out,spec.id);await fs.mkdir(directory);
   page=await browser.newPage({viewport:{width:spec.width,height:spec.height},deviceScaleFactor:2});
   page.on('pageerror',e=>result.errors.push(String(e)));page.on('crash',()=>result.errors.push('browser page crash'));
   const transfers=new Map();await page.exposeFunction('areaPersist',async chunk=>{
    const previous=transfers.get(chunk.name);if(previous?.complete)throw Error('completed buffer replay');
    const next=validateChunk(chunk,previous),bytes=Buffer.from(chunk.base64,'base64'),temporary=path.join(directory,chunk.name+'.partial');
    const handle=await fs.open(temporary,chunk.offset?'r+':'wx');try{const written=await handle.write(bytes,0,bytes.length,chunk.offset);if(written.bytesWritten!==bytes.length)throw Error('short persistence');}finally{await handle.close();}
    if(next.complete)await fs.rename(temporary,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};
   });
   await page.exposeFunction('areaMetadata',async row=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(row,null,2)+'\n'));
   console.log(JSON.stringify({stage:'case-start',id:spec.id}));
   await page.goto(`http://127.0.0.1:${server.address().port}/?width=${spec.width}&height=${spec.height}`);await page.waitForFunction(()=>typeof window.run==='function');
   const row=await page.evaluate(config=>window.run(config),{spec,fixture:request.fixture});
   // All timing and cold buffers persist before independent qualification.
   await page.close();page=null;
   row.roundtrip=await qualifyPersistedRoundtrip({directory,name:'S2',basis:row.pixel_basis,oracle:request.oracle});
   await fs.writeFile(path.join(directory,'qualified-row.json'),JSON.stringify(row,null,2)+'\n');result.rows.push(row);
   if(row.roundtrip.status!=='QUALIFIED')throw Error('actual canonical pixel obligation failed');
   console.log(JSON.stringify({stage:'case-complete',id:spec.id,status:row.roundtrip.status}));
  }
  if(result.errors.length)throw Error('page errors');result.status='OBSERVED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.areaPartial);}catch{}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await write('worker-result.json',result);
  console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const result=await worker(process.argv[2],path.resolve(process.argv[3]));if(result.status!=='OBSERVED')process.exitCode=1;}
