import fs from'node:fs/promises';import path from'node:path';import http from'node:http';
import{chromium}from'../../node_modules/playwright/index.mjs';
import{consumer as stressConsumer}from'./erase_exact_canonical.mjs';
import{validateChunk}from'./ur2d_selection_up_contract.mjs';
import{qualifyPersistedRoundtrip}from'./ur2d_selection_up.mjs';
export function localizeUndo(row){
 const first=rows=>rows?.find(x=>x.at>=row.up.start&&(x.canUndo||x.enabled))?.at??null;
 const runtime=first(row.runtime),change=first(row.changes),dom=first(row.dom),up=row.up.end-row.up.start;
 return{up_ms:up,firstRuntimeTrue:runtime,firstOnChangeTrue:change,firstDOMEnabled:dom,
  classification:runtime===null?'HISTORY_NOT_READY':up>=1000?'SYNC_UP_BLOCK':dom!==null&&dom-row.up.end>=1000?'DOM_NOTIFICATION_LAG':'OBSERVED_READY',
  limitation:'Owner state cannot be read synchronously inside a blocked Up; first public observation is an upper bound.'};
}
export{waitForTerminalGeneration}from'./erase_exact_canonical_contract.mjs';
const wrapper=`import{createRuntime as original}from'/real/axiom.mjs';export *from'/real/axiom.mjs';
export async function createRuntime(options){const fixture=window.undoFixture;const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());
let runtime;const userChange=options.onChange;const value=await original({...options,assetBaseURL:new URL('/real/',location.href),builtinFonts:false,documentId:fixture.documentId,restore:{bytes,metadata:fixture.metadata},onChange(state){const actual=runtime?.readState()??state;window.undoChanges.push({at:performance.now(),canUndo:actual.canUndo,generation:actual.generation,activeContacts:actual.activeContacts});userChange?.(state);}});
runtime=value;window.rootRuntime=value;return value;}`;
export async function longConsumer({moves,busySuccessor=false,action='erase',mixed=false}){
 const sdk=window.rootRuntime;const{installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const{waitForTerminalGeneration,observeCorrectFeedback}=await import('/g5/erase_exact_canonical_contract.mjs');
 const observer=installNavigationObserver(()=>sdk.canvas),pause=ms=>new Promise(r=>setTimeout(r,ms));let clock=90000000000;
 const row={moves,runtime:[],changes:[],dom:[],heartbeats:[],events:[]};window.undoPartial=row;
 const check=(v,m)=>{if(!v)throw Error(m);};const state=()=>sdk.readState();
 const pointer=(phase,x,y)=>{clock+=8000000;const rect=sdk.canvas.getBoundingClientRect();const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);const end=performance.now(),s=state();row.runtime.push({at:end,canUndo:s.canUndo,generation:s.generation,activeContacts:s.activeContacts,presented:s.lastCommit.presentedGeneration});const v={phase,start,end,contacts:s.activeContacts};row.events.push(v);return v;};
 const settle=async()=>{const begin=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return;check(performance.now()-begin<25000,'stable demand deadline');await pause(20);}};
 const snapshot=async label=>{const s=state();const value=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(window.undoFixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:window.undoFixture.metadata.recoveryFrontier});
  if(window.persistChunk)await sendBinaryChunks(label+'.axsnap',value.bytes,window.persistChunk);
  const image=readWebGLTarget(sdk.canvas,{role:'canonical'});check(image.metadata.status==='VALID','final readiness pixels');
  if(window.persistChunk)await sendBinaryChunks(label+'.rgba',image.bytes,window.persistChunk);
  const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
  return{sha256:await digest(value.bytes),byteLength:value.bytes.length,state:s,rgba_sha256:await digest(image.bytes),readback:image.metadata};};
 sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();await sdk.selectTool('select');await sdk.setSelectionMode('point');pointer('down',400,120);pointer('up',400,120);await settle();check(sdk.readSelection().count===1,'source selection');
 for(let i=0;i<24;++i)await sdk.duplicateSelection({offset:[0,0]});await settle();await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();check(state().objects===40,'40copies');
 if(mixed){
  row.preseed=[];
  for(const [kind,x,y]of[['rectangle',850,500],['sticky',1050,500],['table',850,750]]){
   if(kind==='rectangle')await sdk.setAppearance({selected:false,fillEnabled:true,fill:[.2,.5,.8,1],strokeEnabled:true});
   await sdk.selectShape(kind);pointer('move',x,y);pointer('down',x,y);pointer('up',x,y);await settle();row.preseed.push({kind,id:sdk.readSelection().primary,selection:sdk.readSelection()});
  }
  const bytes=Uint8Array.from([0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0,0,0,0x0d,0x49,0x48,0x44,0x52,0,0,0,1,0,0,0,1,8,6,0,0,0,0x1f,0x15,0xc4,0x89,0,0,0,0x0d,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0,5,0,1,0xff,0x89,0x99,0x3d,0x1d,0,0,0,0,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82]);
  const image=await sdk.importImage(bytes,'image/png');await sdk.resizeImage(image.objectId,80,80,true);await sdk.transformSelection([1,0,0,1,1150,750]);await settle();row.preseed.push({kind:'image',id:image.objectId,selection:sdk.readSelection()});
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();check(state().objects===44,'mixed44objects');
 }
 const button=document.getElementById('undo');check(button,'actual demo undo');const mutations=new MutationObserver(()=>row.dom.push({at:performance.now(),enabled:!button.disabled}));mutations.observe(button,{attributes:true,attributeFilter:['disabled']});
 const heartbeat=setInterval(()=>{const s=state();row.heartbeats.push({at:performance.now()});row.runtime.push({at:performance.now(),canUndo:s.canUndo,generation:s.generation,activeContacts:s.activeContacts,presented:s.lastCommit.presentedGeneration});},50);
 const successorRect={x:592,y:482,width:16,height:16};
 const captureSuccessor=()=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:successorRect});check(p.metadata.status==='VALID','successor ROI readback');return p;};
 if(busySuccessor&&action==='erase'){const p=captureSuccessor();row.successorBaseline={at:performance.now(),rect:successorRect,readback:p.metadata,rgba:Array.from(p.bytes),basis:state()};}
 const actionRect={x:1640,y:960,width:280,height:220};
 let actionBaseline;
 if(busySuccessor&&action!=='erase'){
  const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:actionRect});check(p.metadata.status==='VALID','action baseline capture');actionBaseline=Array.from(p.bytes);row.actionBaseline={at:performance.now(),rect:actionRect,rgba:actionBaseline};
  const object=row.preseed?.find(x=>x.kind==='rectangle')?.selection;
  if(object){const x=object.transform[4]+object.width/2,y=object.transform[5]+object.height/2;row.navigationSentinel={world:[x,y],physicalBefore:[Math.round(x*2),Math.round(y*2)],expectedColor:[51,128,204,255]};}
 }
 if(window.persistChunk)row.initialSnapshot=await snapshot('initial');
 observer.begin(row.observation={});row.before=state();row.initialDOM={at:performance.now(),enabled:!button.disabled};let x=420,y=160;pointer('down',x,y);await pause(60);
 for(let i=1;i<=moves;++i){x=420+7*Math.sin(i*.17);y=160+170*i/moves;pointer('move',x,y);if(i%8===0)await pause(0);}
 row.lastMove=performance.now();row.up=pointer('up',x,y);
 if(busySuccessor&&action==='erase'){
  // Correctness-only fixed known-ink ROI, disjoint from the predecessor's
  // x≈420 trace. Readback never belongs to the 160-Move paired timing phase.
  // main Up -> next Down and first Move are deliberately the same DOM task.
  const down=pointer('down',300,230),first=pointer('move',300,242);row.successor={down,first,rect:successorRect,events:[],captures:[],beforeUp:true};
  for(let i=1;i<=8;++i){row.successor.events.push(pointer('move',300,242+i*3));await pause(16);
   if(i===4||i===8){const p=captureSuccessor();row.successor.captures.push({at:performance.now(),readback:p.metadata,rgba:Array.from(p.bytes)});}}
  const last=row.successor.captures.at(-1);let positive=0,baselineInk=0;for(let i=0;i<last.rgba.length;i+=4){
   const before=row.successorBaseline.rgba;const ink=before[i]<100&&before[i+1]<100&&before[i+2]<150&&before[i+3]===255;
   if(ink)++baselineInk;if(ink&&last.rgba[i]>=250&&last.rgba[i+1]>=250&&last.rgba[i+2]>=250&&last.rgba[i+3]===255)++positive;
  }
  row.successor.positiveInteriorPixels=positive;row.successor.baselineInkPixels=baselineInk;
  row.successor.pixelWitness=positive?'OBSERVED_DARK_TO_WHITE_BEFORE_UP':'MISSING';row.successor.up=pointer('up',300,266);check(down.contacts===1&&first.contacts===1&&row.successor.up.contacts===0,'successor contact lifecycle');
  row.actionAdmitted=down.contacts===1&&first.contacts===1;row.actionPoseCorrect=true;row.actionPositivePixels=positive;row.actionFeedbackAt=last.at;
 }else if(busySuccessor&&(action==='pan'||action==='zoom')){
  row.successor={kind:action,events:[],captures:[]};const before=state().camera;
  for(let i=0;i<3;++i){const rect=sdk.canvas.getBoundingClientRect(),event=new WheelEvent('wheel',{clientX:rect.left+600,clientY:rect.top+400,ctrlKey:action==='zoom',deltaX:action==='pan'?8:0,deltaY:action==='pan'?4:-8,bubbles:true,cancelable:true});const start=performance.now();sdk.canvas.dispatchEvent(event);row.successor.events.push({start,end:performance.now(),camera:state().camera});await pause(32);}
  const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:actionRect});check(p.metadata.status==='VALID','navigation active pixels');
  let positive=0;for(let i=0;i<p.bytes.length;i+=4)if(Math.max(...[0,1,2,3].map(c=>Math.abs(p.bytes[i+c]-actionBaseline[i+c])))>2)++positive;
  const expected=action==='pan'?{scale:before.scale,x:before.x-24,y:before.y-12}:
   {scale:before.scale*Math.exp(.24),x:600-Math.exp(.24)*(600-before.x),y:400-Math.exp(.24)*(400-before.y)};
  row.successor.expectedCamera=expected;row.actionAdmitted=true;row.actionPoseCorrect=['scale','x','y'].every(k=>Math.abs(state().camera[k]-expected[k])<.01);row.actionPositivePixels=positive;row.actionFeedbackAt=performance.now();row.successor.capture={rect:actionRect,rgba:Array.from(p.bytes),at:row.actionFeedbackAt,camera:state().camera};
  const world=row.navigationSentinel.world,sx=Math.round(2*(world[0]*expected.scale+expected.x)),sy=Math.round(2*(world[1]*expected.scale+expected.y));
  const sentinelRect={x:sx-2,y:sy-2,width:4,height:4};
  const feedback=await observeCorrectFeedback({probe:()=>{
   const sentinel=readWebGLTarget(sdk.canvas,{role:'canonical',rect:sentinelRect});check(sentinel.metadata.status==='VALID','navigation sentinel capture');
   let positive=0;for(let i=0;i<sentinel.bytes.length;i+=4)if([0,1,2,3].every(c=>Math.abs(sentinel.bytes[i+c]-row.navigationSentinel.expectedColor[c])<=2))++positive;
   return{positive,rgba:Array.from(sentinel.bytes),readback:sentinel.metadata};},pause,now:()=>performance.now()});
  row.successor.feedbackObservations=feedback.observations;row.successor.expectedSentinel={rect:sentinelRect,rgba:feedback.value.rgba,correctInteriorPixels:feedback.value.positive};
  row.actionPositivePixels=feedback.value.positive;row.actionFeedbackAt=feedback.firstCorrectAt??performance.now();
 }else if(busySuccessor){
  row.successor={kind:action,events:[],captures:[]};
  if(action==='image'){
   await sdk.selectTool('select');await sdk.setSelectionMode('point');pointer('down',1190,790);pointer('up',1190,790);
   check(sdk.readSelection().primary===row.preseed.find(x=>x.kind==='image').id,'image selection identity');
  }else{
   await sdk.selectShape(action);await sdk.setAppearance({selected:false,fillEnabled:true,fill:[.2,.5,.8,1],strokeEnabled:true});pointer('move',900,300);pointer('down',900,300);pointer('up',900,300);
   check(sdk.readSelection().count===1,'created selected object');
  }
  const selected=sdk.readSelection(),x=action==='image'?1190:930,y=action==='image'?790:330;
  row.successor.selected=selected;const down=pointer('down',x,y);row.successor.down=down;
  for(const[dx,dy]of[[8,4],[40,16],[80,30]]){row.successor.events.push(pointer('move',x+dx,y+dy));await pause(32);}
  const activeSelection=sdk.readSelection();const rect=action==='image'?{x:2500,y:1620,width:24,height:24}:
   action==='table'?{x:2*Math.floor(selected.transform[4]+selected.width+30),y:2*Math.floor(selected.transform[5]+78)-2,width:20,height:4}:
   {x:2*Math.floor(selected.transform[4]+selected.width+32),y:2*Math.floor(selected.transform[5]+42),width:16,height:16};
  const feedback=await observeCorrectFeedback({probe:()=>{
   const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect});check(p.metadata.status==='VALID','active object pixels');let positive=0;
   for(let i=0;i<p.bytes.length;i+=4){if(action==='image'){if(p.bytes[i]>200&&p.bytes[i+1]<80&&p.bytes[i+2]<80)++positive;}else if(action==='table'){
     if(p.bytes[i]<160&&p.bytes[i+1]<160&&p.bytes[i+2]<160)++positive;
    }else if(Math.abs(p.bytes[i]-51)<=2&&Math.abs(p.bytes[i+1]-128)<=2&&Math.abs(p.bytes[i+2]-204)<=2)++positive;}
   return{positive,rgba:Array.from(p.bytes),readback:p.metadata};},pause,now:()=>performance.now()});
  row.actionAdmitted=down.contacts===1&&row.successor.events.every(x=>x.contacts===1);
  row.actionPoseCorrect=activeSelection.primary===selected.primary;
  row.successor.expectedPose={dx:80,dy:30,initialTransform:selected.transform,initialBounds:selected.bounds};
  row.actionPositivePixels=feedback.value.positive;row.actionFeedbackAt=feedback.firstCorrectAt??performance.now();row.successor.feedbackObservations=feedback.observations;
  row.successor.capture={rect,rgba:feedback.value.rgba,at:row.actionFeedbackAt,beforeUp:true,selection:activeSelection};
  row.successor.up=pointer('up',x+80,y+30);row.successor.afterSelection=sdk.readSelection();
  row.actionPoseCorrect&&=Math.abs(row.successor.afterSelection.transform[4]-selected.transform[4]-80)<.1&&Math.abs(row.successor.afterSelection.transform[5]-selected.transform[5]-30)<.1;
 }
 await settle();row.readyAt=performance.now();await pause(1000);row.after=state();row.dom.push({at:performance.now(),enabled:!button.disabled});row.changes=window.undoChanges.slice();row.activity=observer.activity();
 // Readiness is observed before an independent next gesture, then real UI Undo/Redo.
 pointer('down',1300,900);pointer('up',1300,900);await settle();
 if(window.persistChunk)row.finalSnapshot=await snapshot('final');
 const beforeUndo=state().generation;button.click();await waitForTerminalGeneration({expected:(BigInt(beforeUndo)+1n).toString(),read:state,pause,now:()=>performance.now()});await settle();row.undo=state();
 if(window.persistChunk)row.undoSnapshot=await snapshot('undo');
 document.getElementById('redo').click();await waitForTerminalGeneration({expected:(BigInt(beforeUndo)+2n).toString(),read:state,pause,now:()=>performance.now()});await settle();row.redo=state();
 if(window.persistChunk)row.redoSnapshot=await snapshot('redo');
 check(BigInt(row.undo.generation)===BigInt(beforeUndo)+1n&&BigInt(row.redo.generation)===BigInt(beforeUndo)+2n,'actual Demo Undo/Redo generations');
 row.canonicalRoundtrip={finalGeneration:beforeUndo,undoGeneration:row.undo.generation,redoGeneration:row.redo.generation};
 pointer('down',1300,900);pointer('cancel',1301,901);await settle();check(state().activeContacts===0,'terminal contacts');
 observer.end(row.observation);clearInterval(heartbeat);mutations.disconnect();observer.restore();row.status='OBSERVED';return row;
}
export async function undoWorker(request,out){
 await fs.mkdir(out,{recursive:true});const root=path.resolve(new URL('../../..',import.meta.url).pathname);let sdk=request.variants[0].sdk,browser,server,page;const result={id:request.id,status:'STARTED',rows:[],errors:[]};
 try{
  server=http.createServer(async(req,res)=>{try{const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
   if(name==='/example.html'){res.setHeader('Content-Type','text/html');res.end(await fs.readFile(path.join(sdk,'example.html')));return;}
   if(name==='/stress.html'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{consumer}from"/stress-driver.mjs";window.run=consumer;</script>');return;}
   if(name==='/stress-driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+stressConsumer.toString());return;}
   if(name==='/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(wrapper);return;}
   if(name==='/fixture.axsnap')file=request.fixture.path;else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));else if(name.startsWith('/sdk/'))file=path.join(sdk,name.slice(5));else file=path.join(sdk,name.startsWith('/real/')?name.slice(6):name.slice(1));
   res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const variant of request.variants){sdk=variant.sdk;
  for(const moves of request.undoLengths??[800,1600]){page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));await page.addInitScript(fixture=>{window.undoFixture=fixture;window.undoChanges=[];},request.fixture);await page.goto('http://127.0.0.1:'+server.address().port+'/example.html');await page.waitForFunction(()=>window.rootRuntime&&document.getElementById('undo'));
   const directory=path.join(out,variant.name+'-'+moves);await fs.mkdir(directory);const transfers=new Map();
   await page.exposeFunction('persistChunk',async chunk=>{const previous=transfers.get(chunk.name);if(previous?.complete)throw Error('buffer replay');const next=validateChunk(chunk,previous),bytes=Buffer.from(chunk.base64,'base64'),file=path.join(directory,chunk.name+'.partial');const h=await fs.open(file,chunk.offset?'r+':'wx');try{await h.write(bytes,0,bytes.length,chunk.offset);}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};});
   const row=await page.evaluate(longConsumer,{moves,busySuccessor:!!request.busySuccessor});row.localization=localizeUndo(row);row.variant=variant.name;row.status='QUALIFIED';result.rows.push(row);await fs.writeFile(path.join(out,variant.name+'-undo-'+moves+'.json'),JSON.stringify(row,null,2)+'\n');console.log(JSON.stringify({stage:'undo-case',variant:variant.name,moves,localization:row.localization}));await page.close();page=null;if(row.localization.up_ms>=10000)break;
  }}
  if(request.stressCandidate){
   sdk=request.variants.find(x=>x.name==='candidate').sdk;const directory=path.join(out,'candidate-10round');await fs.mkdir(directory);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});const transfers=new Map();
   await page.exposeFunction('persistMetadata',row=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(row,null,2)+'\n'));
   await page.exposeFunction('persistChunk',async chunk=>{const next=validateChunk(chunk,transfers.get(chunk.name)),file=path.join(directory,chunk.name+'.partial'),h=await fs.open(file,chunk.offset?'r+':'wx');try{await h.write(Buffer.from(chunk.base64,'base64'),0,undefined,chunk.offset);}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};});
   await page.goto('http://127.0.0.1:'+server.address().port+'/stress.html');await page.waitForFunction(()=>window.run);const row=await page.evaluate(config=>window.run(config),{fixture:request.fixture,variant:'candidate',repetition:0,stress:true});await page.close();page=null;
   row.roundtrip=await qualifyPersistedRoundtrip({directory,name:'S2',basis:row.pixel_basis,oracle:request.oracle});if(row.roundtrip.status!=='QUALIFIED')throw Error('stress pixel failure');result.rows.push(row);await fs.writeFile(path.join(directory,'qualified-row.json'),JSON.stringify(row,null,2)+'\n');
  }
  if(result.errors.length)throw Error('actual Demo errors');result.status='OBSERVED';
 }catch(e){result.status='FAILED';result.error=String(e.stack??e);if(page)try{result.partial=await page.evaluate(()=>window.undoPartial);}catch{}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const r=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await undoWorker(r,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
