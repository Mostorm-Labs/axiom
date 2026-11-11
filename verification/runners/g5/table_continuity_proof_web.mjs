// Verification only. Ordinary immutable SDKs own input, document and rendering.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {validateChunk} from './ur2d_selection_up_contract.mjs';
import {entropyStream,installEntropy} from './exact_fixed_scene_contract.mjs';

export async function webConsumer(c){
 const {createRuntime}=await import('/sdk/axiom.mjs');
 const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const {typedWitness,qualifyWitness}=await import('/g5/exact_query_working_set_contract.mjs');
 const {probeUntilContent,compareExpectedROI}=await import('/g5/table_continuity_proof_contract.mjs');
 const {emptyViewPoint}=await import('/g5/exact_fixed_scene_contract.mjs');
 const row={id:c.id,status:'STARTED',rounds:[],errors:[],operations:[],frames:[],events:[],feedbacks:[],cleanupErrors:[],pacing:'ORIGINAL_WITH_REQUIRED_PREDOWN_EVIDENCE'};
 const pause=ms=>new Promise(r=>setTimeout(r,ms));
 const check=(v,m)=>{if(!v)throw Error(m);};
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 let sdk,clock=90000000000,observing=false;
 const state=()=>sdk.readState();
 const observer=installNavigationObserver(()=>sdk?.canvas);
 const deferredRecords=new Map(),deferredBuffers=[];
 const save=async(name,value)=>{if(c.deferEvidence)deferredRecords.set(name,structuredClone(value));else await window.tpRecord({name,value});};
 const sendBuffer=async(name,bytes)=>{if(c.deferEvidence)deferredBuffers.push({name,bytes});else await sendBinaryChunks(name,bytes,window.tpChunk);};
 const begin=()=>{observer.begin(row.observation={});observing=true;};
 const end=()=>{if(observing){observer.end(row.observation);observing=false;}};
 const pointer=(phase,x,y)=>{
  clock+=8000000;const rect=sdk.canvas.getBoundingClientRect();
  const event=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
  Object.defineProperty(event,'timeStamp',{value:clock/1e6});
  const start=performance.now();observer.dispatch(sdk.canvas,event);const s=state();
  const value={phase,x,y,start,end:performance.now(),contacts:s.activeContacts,generation:s.generation};row.events.push(value);return value;
 };
 const settle=async()=>{const start=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now();check(performance.now()-start<25000,'stable exact tail exceeded25s');await pause(20);}};
 const small=rect=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect});return{metadata:p.metadata,rgba:p.bytes?Array.from(p.bytes):null,state:state(),at:performance.now()};};
 const capture=()=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical'});return{metadata:p.metadata,bytes:p.bytes,state:state(),at:performance.now()};};
 const storeCapture=async(label,p)=>{
  const v={metadata:p.metadata,state:p.state,at:p.at,byteLength:p.bytes?.length??0};
  await save(label+'-readback',v);
  if(p.bytes){await sendBuffer(label+'.rgba',p.bytes);v.sha256=await digest(p.bytes);}
  return v;
 };
 const snapshot=async(label,includeFrame=true)=>{
  const before=state();check(before.activeContacts===0,'no active snapshot');const started=performance.now();
  const v=await sdk.captureSnapshot({generation:before.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(before.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});
  const frame=includeFrame?capture():null;
  const value={started,capturedAt:performance.now(),sha256:await digest(v.bytes),byteLength:v.bytes.length,before,after:state(),throughCommit:v.throughCommit};
  await save(label+'-snapshot',value);await sendBuffer(label+'.axsnap',v.bytes);
  if(frame)value.frame=await storeCapture(label,frame);
  value.persistedAt=performance.now();return value;
 };
 const imageBytes=Uint8Array.from([0x89,0x50,0x4e,0x47,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130]);
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===c.fixture.sha256,'fixture identity');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({at:performance.now(),code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();check(state().objects===40,'fixed40 restore');
  await sdk.appearance.setDefaults({enabled:false});row.witnessAppearancePolicy=sdk.appearance.readDefaults();row.preseed=[];
  for(const [kind,x,y]of [['rectangle',850,500],['sticky',1050,500],['table',850,750]]){
   if(kind==='rectangle')await sdk.setAppearance({selected:false,fillEnabled:true,fill:[.2,.5,.8,1],strokeEnabled:true});
   await sdk.selectShape(kind);pointer('move',x,y);pointer('down',x,y);pointer('up',x,y);await settle();
   row.preseed.push({kind,id:sdk.readSelection().primary,selection:sdk.readSelection()});
  }
  const image=await sdk.importImage(imageBytes,'image/png');await sdk.resizeImage(image.objectId,80,80,true);await sdk.transformSelection([1,0,0,1,1150,750]);await settle();row.preseed.push({kind:'image',id:image.objectId});row.imageResourceSHA=await digest(imageBytes);
  check(state().objects===44,'preseed cardinality');
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();row.initial=await snapshot('initial');
  begin();let x=420,y=160;
  if(c.scenario!=='focused-table')for(let round=0;round<6;++round){
   x=420-round*18;y=160;const r={round,down:pointer('down',x,y)};check(r.down.contacts===1,'erase Down rejected');
   for(let i=1;i<=160;++i){x=420-round*18+7*Math.sin(i*.17);y=160+170*i/160;pointer('move',x,y);if(i%8===0)await pause(0);}
   r.up=pointer('up',x,y);row.rounds.push(r);
   if(round===5){row.mainUp=r.up;break;}
   r.shortDown=pointer('down',x+1,y+1);r.shortMove=pointer('move',x+2,y+4);r.shortUp=pointer('up',x+2,y+4);
  }
  else row.localization='SAME_MIXED_INITIAL_WITHOUT_ERASE_OR_STICKY_DRAG';
  const kinds=c.scenario==='focused-table'?['table']:['sticky','table','image'];
  for(const kind of kinds){
   const item={kind,status:'STARTED',pacing:row.pacing,errors:[],beforeSelect:state(),predecessorUpStart:row.mainUp?.start};row.feedbacks.push(item);
   try{
    await sdk.selectTool('select');await sdk.setSelectionMode('point');
    const point=kind==='sticky'?[1080,530]:kind==='table'?[880,772]:[1190,790];
    item.selectDown=pointer('down',...point);item.selectUp=pointer('up',...point);
    const selected=sdk.readSelection(),typed=kind==='sticky'?sdk.readSelectedSticky():kind==='table'?sdk.table.read(selected.primary):sdk.readSelectedImage();
    item.basis={kind,selected,typed,appearance:sdk.appearance.read(),state:state(),imageResourceSHA:row.imageResourceSHA,reference:c.tableReference};
    await save(kind+'-basis',item.basis);
    check(selected.primary===row.preseed.find(v=>v.kind===kind).id,'actual typed identity');
    const legacy=typedWitness(kind,selected,typed,row.imageResourceSHA);
    const rect=kind==='table'?c.tableReference.rect:legacy.rect;
    item.plan={...legacy,rect,originalRect:legacy.rect,expected:kind==='table'?c.tableReference.rgba:null,requiredPixels:kind==='table'?c.tableReference.requiredPixels:null};
    if(kind==='table'){
     // This is the most recent COMMITTED basis, after the sticky's legitimate Up.
     // No settle is inserted, and snapshot is never requested with contact active.
     if(c.deferEvidence){
      // Frozen localization only: the initial snapshot and actual operations
      // bind the latest canonical pose without host IO/snapshot in transition.
      item.basis.latestCommittedBasis={initialSnapshot:row.initial,knownStickyTransform:[1,0,0,1,1130,530],completedSticky:row.feedbacks.find(v=>v.kind==='sticky')?.finalSelection,operations:row.operations.slice(),state:state(),method:'initialcanonical+legitimatepublicUp+operationandgenerationstamps;offlinecloneanddecode'};
     }else{
      item.preDownSnapshot=await snapshot('table-preDown',false);
      item.basis.latestCommittedSnapshot=item.preDownSnapshot;
     }
     item.basis.preDownSelected=sdk.readSelection();item.basis.preDownTyped=sdk.table.read(selected.primary);item.basis.preDownAppearance=sdk.appearance.read();
     check(selected.primary===c.tableReference.objectId,'table oracle object ID');
     check(JSON.stringify(selected.transform)===JSON.stringify(c.tableReference.oldTransform),'table oracle pose');
     check(typed.rows.map(v=>v.size).join(',')==='48,48'&&typed.columns.map(v=>v.size).join(',')==='120,120','table oracle dimensions');
     check(typed.grid.width===1&&typed.grid.color.every((v,i)=>Math.abs(v-c.tableReference.grid.color[i])<1e-6),'table oracle material');
     check(state().camera.scale===1&&state().camera.x===0&&state().camera.y===0,'table oracle viewport');
    }
    if(kind==='image'){
     const token=sdk.resources.token(typed.content.resourceId);item.actualResourceSHA=await digest(sdk.resources.readBytes(token));check(item.actualResourceSHA===row.imageResourceSHA,'image resource bytes');
    }
    const qualify=p=>kind==='table'?compareExpectedROI(p.rgba,c.tableReference.rgba,c.tableReference):qualifyWitness(legacy,p.rgba,[80,30]);
    item.baseline=small(rect);item.baselineOriginal=kind==='table'?small(c.tableReference.originalRect):null;
    item.baselineBroad=kind==='table'?small(c.tableReference.broadRect):null;
    item.plan.baselineQualification=item.baseline.rgba?qualify(item.baseline):{positive:false,status:'INVALID'};
    await save(kind+'-basis',item);
    check(item.baseline.metadata.status==='VALID','baseline readback INVALID');
    check(!item.plan.baselineQualification.positive,'baseline already contains new content or image overlap');
    const px=selected.transform[4]+30,py=selected.transform[5]+(kind==='table'?22:30);
    item.down=pointer('down',px,py);await save(kind+'-stage',item);check(item.down.contacts===1,'typed Down rejected');
    for(const [dx,dy]of [[8,4],[40,16],[80,30]]){pointer('move',px+dx,py+dy);await pause(16);}
    item.lastMove=row.events.at(-1);item.guardPollStart=performance.now();
    let terminalFrame=null,terminalBroad=null;
    item.feedback=await probeUntilContent({basis:item.basis,now:()=>performance.now(),pause,guardMs:15000,
     probe:()=>{
      const p=small(rect),q=p.metadata.status==='VALID'&&p.rgba?qualify(p):{positive:false,matches:0,reason:'INVALID_READBACK'};
      const value={...p,...q,selection:sdk.readSelection(),typedTable:kind==='table'?sdk.table.read(selected.primary):null,original:kind==='table'?small(c.tableReference.originalRect):null,readbackClassification:p.metadata.status==='VALID'?'VALID':'INVALID'};
      if(!terminalFrame&&(q.positive||performance.now()-item.guardPollStart>=15000)){
       terminalFrame=capture();terminalBroad=kind==='table'?small(c.tableReference.broadRect):null;
      }
      return value;
     },
     persist:async feedback=>{
      // Persist on every poll before the caller performs any assertion/cleanup.
      item.feedback=feedback;await save(kind+'-feedback',feedback);
     }
    });
    item.activeAfterProbe=state();
    // Capture the exact terminal observation before host streaming can yield.
    terminalFrame??=capture();terminalBroad??=kind==='table'?small(c.tableReference.broadRect):null;
    item.terminalBroad=terminalBroad;
    if(c.deferEvidence){
     // Keep the synchronous terminal capture, but do not await crypto/host IO
     // between the successful sticky Up and the original table Down.
     deferredBuffers.push({name:kind+'-active-'+item.feedback.status.toLowerCase()+'.rgba',bytes:terminalFrame.bytes});
     item.terminalFrame={metadata:terminalFrame.metadata,state:terminalFrame.state,at:terminalFrame.at,byteLength:terminalFrame.bytes?.length??0,deferred:true};
    }else item.terminalFrame=await storeCapture(kind+'-active-'+item.feedback.status.toLowerCase(),terminalFrame);
    item.feedbackClock={predecessorUpStart:item.predecessorUpStart,downStart:item.down.start,lastMoveEnd:item.lastMove.end,guardPollStart:item.guardPollStart,firstQualifiedObservation:item.feedback.firstCorrectAt??null,limitation:'observation bounds; not physical input-to-photon; required evidence snapshot/readback overhead included'};
    await save(kind+'-stage',item);
    check(item.activeAfterProbe.activeContacts===1,'typed active contact missing');
    if(item.feedback.status==='QUALIFIED'){
     item.up=pointer('up',px+80,py+30);item.finalSelection=sdk.readSelection();
     item.status='QUALIFIED';await save(kind+'-stage',item);
     check(Math.abs(item.finalSelection.transform[4]-selected.transform[4]-80)<.1&&Math.abs(item.finalSelection.transform[5]-selected.transform[5]-30)<.1,'typed final pose');
    }else{
     item.status=item.feedback.status;item.failure='UNQUALIFIED_TABLE_CONTENT';
     item.cancel=pointer('cancel',px+80,py+30);item.afterCancel=state();
     row.pacing='MODIFIED_RECOVERY_AFTER_EXPLICIT_CANCEL';
     await save(kind+'-stage',item);check(item.afterCancel.activeContacts===0,'Cancel left active contact');
    }
   }catch(error){
    item.status='FAILED';item.errors.push(String(error.stack??error));
    try{item.failureState=state();}catch(e){item.errors.push('state:'+String(e));}
    // All already captured small pixels, typed basis and original error survive.
    await save(kind+'-stage',item);
    try{if(state().activeContacts){const last=row.events.at(-1);item.cancel=pointer('cancel',last.x,last.y);}item.afterCancel=state();}
    catch(e){item.cancelError=String(e.stack??e);row.cleanupErrors.push({stage:kind+'-cancel',error:item.cancelError});}
    row.pacing='MODIFIED_RECOVERY_AFTER_EXPLICIT_CANCEL';await save(kind+'-stage',item);
    if(!item.afterCancel||item.afterCancel.activeContacts||item.cancelError)throw error;
   }
  }
  row.sequenceEndAt=performance.now();row.readyAt=await settle();end();
  await sdk.selectTool('select');const point=emptyViewPoint(state().camera);row.hudCleanup=point;
  pointer('down',...point.view);pointer('up',...point.view);await settle();check(sdk.readSelection().count===0,'snapshot HUD absent');
  row.final=await snapshot('final');await sdk.undo();await settle();row.undo=await snapshot('undo');await sdk.redo();await settle();row.redo=await snapshot('redo');
  row.entropy=globalThis.exactFixtureEntropy.transcript();row.end=state();row.activity=observer.activity();
  check(row.end.activeContacts===0&&row.end.generation===row.end.lastCommit.presentedGeneration,'terminal receipt');
  check(row.errors.length===0,'public SDK errors');
  row.status=row.feedbacks.every(v=>v.status==='QUALIFIED')?'OBSERVED':'CLASSIFIED_FAILURE';
 }catch(error){row.status='FAILED';row.error=String(error.stack??error);try{row.failureState=state();}catch{}}
 finally{
  try{end();observer.restore();}catch(error){row.cleanupErrors.push({stage:'observer',error:String(error.stack??error)});}
  if(sdk){
   try{if(state().activeContacts){const last=row.events.at(-1);row.finalCancel=pointer('cancel',last?.x??24,last?.y??986);}}
   catch(error){row.cleanupErrors.push({stage:'cancel',error:String(error.stack??error)});}
   try{await sdk.close();row.closed=true;}catch(error){row.cleanupErrors.push({stage:'close',error:String(error.stack??error)});}
  }
  try{row.entropy??=globalThis.exactFixtureEntropy.transcript();}catch{}
  for(const [name,value]of deferredRecords)await window.tpRecord({name,value});
  for(const b of deferredBuffers)if(b.bytes)await sendBinaryChunks(b.name,b.bytes,window.tpChunk);
  await window.tpMetadata(row);
 }
 return {status:row.status,id:row.id,feedbacks:row.feedbacks.map(v=>({kind:v.kind,status:v.status})),cleanupErrors:row.cleanupErrors};
}

export function createServer(request,active){
 const root=path.resolve(new URL('../../..',import.meta.url).pathname);
 return http.createServer(async(req,res)=>{try{
  const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
  if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{webConsumer}from"/driver.mjs";window.run=webConsumer;</script>');return;}
  if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+webConsumer.toString());return;}
  if(name==='/fixture.axsnap')file=request.fixture.path;
  else if(name.startsWith('/sdk/'))file=path.join(active.sdk,name.slice(5));
  else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));
  if(!file)throw Error('unknown HTTP path');
  res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch(error){res.statusCode=404;res.end(String(error));}});
}

export async function runWindow(request,out){
 await fs.mkdir(out,{recursive:true});const active={sdk:null};let browser,page,server;
 const result={id:request.id,status:'STARTED',rows:[],errors:[],cleanupErrors:[]};
 try{
  server=createServer(request,active);await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of request.cases){
   active.sdk=spec.sdk;const directory=path.join(out,spec.id);await fs.mkdir(directory);const transfers=new Map(),consoleLines=[];
   page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});
   await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy('+spec.seed+');'});
   page.on('console',m=>{if(consoleLines.length<4096)consoleLines.push({type:m.type(),text:m.text().slice(0,8192)});});
   page.on('pageerror',e=>result.errors.push({id:spec.id,error:String(e)}));
   await page.exposeFunction('tpMetadata',v=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(v,null,2)+'\n'));
   await page.exposeFunction('tpRecord',async({name,value})=>{if(!/^[a-zA-Z0-9_-]{1,96}$/.test(name))throw Error('record name');await fs.writeFile(path.join(directory,name+'.json'),JSON.stringify(value,null,2)+'\n');});
   await page.exposeFunction('tpChunk',async chunk=>{
    const old=transfers.get(chunk.name);if(old?.complete)throw Error('completed buffer replay');
    const next=validateChunk(chunk,old),bytes=Buffer.from(chunk.base64,'base64'),file=path.join(directory,chunk.name+'.partial');
    const h=await fs.open(file,chunk.offset?'r+':'wx');try{const n=await h.write(bytes,0,bytes.length,chunk.offset);if(n.bytesWritten!==bytes.length)throw Error('short write');}finally{await h.close();}
    if(next.complete)await fs.rename(file,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};
   });
   console.log(JSON.stringify({stage:'case-start',id:spec.id}));
   try{
    await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);
    const summary=await page.evaluate(config=>window.run(config),{...spec,fixture:request.fixture,tableReference:request.tableReference});
    result.rows.push(summary);
   }catch(error){result.rows.push({id:spec.id,status:'FAILED',error:String(error.stack??error)});}
   finally{await fs.writeFile(path.join(directory,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');if(page){await page.close();page=null;}}
   console.log(JSON.stringify({stage:'case-end',...result.rows.at(-1)}));
   const last=result.rows.at(-1);if(last.status==='FAILED'||last.cleanupErrors?.length)throw Error('unsafe/incomplete page prevents next row');
  }
  result.status=result.rows.every(v=>['OBSERVED','CLASSIFIED_FAILURE'].includes(v.status))&&!result.errors.length?'CAPTURED':'FAILED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);}
 finally{
  try{if(page)await page.close();}catch(error){result.cleanupErrors.push({stage:'page',error:String(error)});}
  try{if(browser)await browser.close();}catch(error){result.cleanupErrors.push({stage:'browser',error:String(error)});}
  try{if(server)await new Promise(r=>server.close(r));}catch(error){result.cleanupErrors.push({stage:'server',error:String(error)});}
  await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status}));
 }
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await runWindow(request,path.resolve(process.argv[3]))).status!=='CAPTURED')process.exitCode=1;}
