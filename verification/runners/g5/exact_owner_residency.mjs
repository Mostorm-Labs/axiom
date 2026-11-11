import fs from'node:fs/promises';import path from'node:path';import http from'node:http';
import{chromium}from'../../node_modules/playwright/index.mjs';import{execFileSync}from'node:child_process';
import{createHash}from'node:crypto';import{validateChunk}from'./ur2d_selection_up_contract.mjs';
import{entropyStream,installEntropy}from'./exact_fixed_scene_contract.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function webConsumer(c){
 const{createRuntime}=await import('/sdk/axiom.mjs');
 const{installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const{typedWitness,qualifyWitness,navigationWitness}=await import('/g5/exact_query_working_set_contract.mjs');
 const{observeCorrectFeedback}=await import('/g5/erase_exact_canonical_contract.mjs');
 const{emptyViewPoint}=await import('/g5/exact_fixed_scene_contract.mjs');
 const row={kind:c.kind,mode:c.mode,rounds:[],errors:[],operations:[],frames:[],events:[]};window.waPartial=row;
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};let sdk,clock=90000000000;
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 const observer=installNavigationObserver(()=>sdk?.canvas),state=()=>sdk.readState();let observing=false;
 const begin=()=>{observer.begin(row.observation={});observing=true;};const end=()=>{if(observing){observer.end(row.observation);observing=false;}};
 const pointer=(phase,x,y)=>{clock+=8000000;const rect=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);const s=state();const r={phase,x,y,start,end:performance.now(),contacts:s.activeContacts,generation:s.generation};row.events.push(r);return r;};
 const settle=async()=>{const start=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now();check(performance.now()-start<25000,'stable exact tail exceeded25s');await pause(20);}};
 const snapshot=async label=>{const s=state(),v=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks(label+'.axsnap',v.bytes,window.waChunk);const p=readWebGLTarget(sdk.canvas,{role:'canonical'});check(p.metadata.status==='VALID','target readback');await sendBinaryChunks(label+'.rgba',p.bytes,window.waChunk);return{sha256:await digest(v.bytes),byteLength:v.bytes.length,state:s,rgba_sha256:await digest(p.bytes),readback:p.metadata};};
 const erase=async(moves,round=0)=>{const before=state(),r={moves,round,before,events:[]};let x=420-round*18,y=160;r.down=pointer('down',x,y);check(r.down.contacts===1,'Down rejected');
  for(let i=1;i<=moves;++i){x=420-round*18+7*Math.sin(i*.17);y=160+170*i/moves;r.events.push(pointer('move',x,y));if(i%8===0)await pause(0);}
  if(c.diagnostic)window.waTrace.enable(c.traceOn!==false);
  r.up=pointer('up',x,y);if(c.diagnostic)window.waTrace.enable(false);r.afterUp=state();const sx=c.mode==='growth'?x+1:300,sy=c.mode==='growth'?y+1:230;
  r.nextDown=pointer('down',sx,sy);r.nextMove=pointer('move',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:242);
  if(c.mode!=='growth')for(let i=1;i<=8;++i)r.events.push(pointer('move',300,242+i*3));
  if(c.diagnostic)window.waTrace.enable(c.traceOn!==false);
  r.nextUp=pointer('up',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:266);if(c.diagnostic)window.waTrace.enable(false);r.afterNext=state();
  if(c.diagnostic)row.tracePages=(row.tracePages??[]).concat(window.waTrace.take());row.rounds.push(r);return r;};
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===c.fixture.sha256,'fixture identity');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();check(state().objects===40,'fixed40 restore');
  const imageBytes=Uint8Array.from([0x89,0x50,0x4e,0x47,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130]);
  if(c.mode==='witness'||c.mode==='action'){
   await sdk.appearance.setDefaults({enabled:false});row.witnessAppearancePolicy=sdk.appearance.readDefaults();
   row.preseed=[];
   for(const[kind,x,y]of[['rectangle',850,500],['sticky',1050,500],['table',850,750]]){if(kind==='rectangle')await sdk.setAppearance({selected:false,fillEnabled:true,fill:[.2,.5,.8,1],strokeEnabled:true});await sdk.selectShape(kind);pointer('move',x,y);pointer('down',x,y);pointer('up',x,y);await settle();row.preseed.push({kind,id:sdk.readSelection().primary});}
   const image=await sdk.importImage(imageBytes,'image/png');await sdk.resizeImage(image.objectId,80,80,true);await sdk.transformSelection([1,0,0,1,1150,750]);await settle();row.preseed.push({kind:'image',id:image.objectId});
   check(state().objects===44,'44mixedfixture');row.imageResourceSHA=await digest(imageBytes);
  }
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();row.initial=state();row.initialSnapshot=await snapshot('initial');
  if(c.mode==='witness'||c.mode==='action'){
   let eraseBaseline,navPlan,navBaseline;
   if(c.kind==='pan'||c.kind==='zoom'){navPlan=navigationWitness(c.kind,state().camera);const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:navPlan.rect});check(p.metadata.status==='VALID','navigation baseline');navBaseline=Array.from(p.bytes);check(!navBaseline.some((_,i)=>i%4===0&&navPlan.color.every((v,c)=>Math.abs(navBaseline[i+c]-v)<=2)),'navigation baseline already qualifies');}
   if(c.kind==='erase'){const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:{x:592,y:482,width:16,height:16}});check(p.metadata.status==='VALID','erase baseline');eraseBaseline=Array.from(p.bytes);}
   begin();let x=420,y=160;pointer('down',x,y);
   for(let i=1;i<=1600;++i){x=420+7*Math.sin(i*.17);y=160+170*i/1600;pointer('move',x,y);if(i%8===0)await pause(0);}
   row.mainUp=pointer('up',x,y);const generationAfterErase=state().generation;
   if(c.mode==='action'){
    row.action={kind:c.kind,before:state()};
    if(c.kind==='ready'){row.action.readyAt=await settle();row.action.qualified=state().canUndo;}
    else if(c.kind==='erase'){
     const rect={x:592,y:482,width:16,height:16};const baseline={bytes:eraseBaseline};row.action.baseline=eraseBaseline;
     row.action.down=pointer('down',300,230);row.action.first=pointer('move',300,242);check(row.action.down.contacts===1&&row.action.first.contacts===1,'successor admission');
     for(let i=1;i<=8;++i){pointer('move',300,242+i*3);await pause(16);}
     const feedback=await observeCorrectFeedback({probe:()=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect});check(p.metadata.status==='VALID','erase active');let positive=0;for(let i=0;i<p.bytes.length;i+=4)if(baseline.bytes[i]<100&&baseline.bytes[i+1]<100&&baseline.bytes[i+2]<150&&p.bytes[i]>=250&&p.bytes[i+1]>=250&&p.bytes[i+2]>=250)++positive;return{positive,rgba:Array.from(p.bytes)};},pause,now:()=>performance.now(),timeoutMs:15000});
     row.action.feedback=feedback;row.action.activeBeforeUp=true;row.action.up=pointer('up',300,266);row.action.qualified=feedback.status==='QUALIFIED';
    }else{
     const old=state().camera;for(let i=0;i<3;++i){const rect=sdk.canvas.getBoundingClientRect();const event=new WheelEvent('wheel',{clientX:rect.left+600,clientY:rect.top+400,ctrlKey:c.kind==='zoom',deltaX:c.kind==='pan'?8:0,deltaY:c.kind==='pan'?4:-8,bubbles:true,cancelable:true});sdk.canvas.dispatchEvent(event);await pause(32);}
     const expected=navPlan.expected;row.action.witness=navPlan;row.action.baseline=navBaseline;
     row.action.expected=expected;check(['scale','x','y'].every(k=>Math.abs(state().camera[k]-expected[k])<.01),'navigation camera');
     // An analytic preseed rectangle interior follows the actual camera.
     const rect=navPlan.rect;
     const feedback=await observeCorrectFeedback({probe:()=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect});check(p.metadata.status==='VALID','navigation expected pixels');let positive=0;for(let i=0;i<p.bytes.length;i+=4)if([51,128,204,255].every((v,c)=>Math.abs(v-p.bytes[i+c])<=2))++positive;return{positive,rgba:Array.from(p.bytes)};},pause,now:()=>performance.now(),timeoutMs:15000});row.action.feedback=feedback;row.action.qualified=feedback.status==='QUALIFIED';
    }
    check(row.action.qualified,'actual successor feedback');row.readyAt=await settle();end();
   }else{
   if(c.kind==='image'){await sdk.selectTool('select');await sdk.setSelectionMode('point');pointer('down',1190,790);pointer('up',1190,790);check(sdk.readSelection().primary===row.preseed.find(x=>x.kind==='image').id,'image primary');}
   else{if(c.kind==='rectangle')await sdk.setAppearance({selected:false,fillEnabled:true,fill:[.2,.5,.8,1],strokeEnabled:true});await sdk.selectShape(c.kind);pointer('move',900,300);pointer('down',900,300);pointer('up',900,300);check(sdk.readSelection().count===1,'created selected');}
   const s=sdk.readSelection();let typed;
   if(c.kind==='sticky')typed=sdk.readSelectedSticky();else if(c.kind==='table')typed=sdk.table.read(s.primary);else if(c.kind==='image')typed=sdk.readSelectedImage();else typed={available:true,objectId:s.primary,width:s.width,height:s.height};
   row.typed=typed;row.selected=s;row.plan=typedWitness(c.kind,s,typed,row.imageResourceSHA);
   if(c.kind==='image'){row.imageResourceToken=sdk.resources.token(typed.content.resourceId);row.imageActualSHA=await digest(sdk.resources.readBytes(row.imageResourceToken));check(row.imageActualSHA===row.imageResourceSHA,'image resource SHA');}
   const baseline=readWebGLTarget(sdk.canvas,{role:'canonical',rect:row.plan.rect});check(baseline.metadata.status==='VALID','witness baseline');row.baseline={rgba:Array.from(baseline.bytes),at:performance.now(),metadata:baseline.metadata};
   check(qualifyWitness(row.plan,baseline.bytes,[80,30]).positive===0,'baseline ROI already matches expected material');
   // Tablepick remains far from row/column resize edges (5px tolerance).
   const px=s.transform[4]+30,py=s.transform[5]+(c.kind==='table'?22:30);row.dragDown=pointer('down',px,py);check(row.dragDown.contacts===1,'drag admission');
   for(const[dx,dy]of[[8,4],[40,16],[80,30]]){pointer('move',px+dx,py+dy);await pause(16);}
   const feedback=await observeCorrectFeedback({probe:()=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:row.plan.rect});check(p.metadata.status==='VALID','witness active capture');const q=qualifyWitness(row.plan,p.bytes,[80,30]);return{positive:q.positive,rgba:Array.from(p.bytes),metadata:p.metadata};},pause,now:()=>performance.now(),timeoutMs:15000});
   row.feedback=feedback;row.activePixels={rect:row.plan.rect,rgba:feedback.value.rgba,at:feedback.firstCorrectAt,beforeUp:true};row.dragUp=pointer('up',px+80,py+30);row.afterSelection=sdk.readSelection();
   row.poseDelta=[row.afterSelection.transform[4]-s.transform[4],row.afterSelection.transform[5]-s.transform[5]];
   check(row.poseDelta.every((x,i)=>Math.abs(x-[80,30][i])<.1),'wrong committed pose');check(feedback.status==='QUALIFIED','active witness missing');
   const expected=BigInt(generationAfterErase)+(c.kind==='image'?1n:2n);check(BigInt(state().generation)===expected,'create/transform commit cardinality');row.readyAt=await settle();end();
   }
  }else{
   begin();const count=c.mode==='growth'?(c.roundCount??6):1;
   for(let round=0;round<count;++round)await erase(c.moves??160,round);
   row.readyAt=await settle();end();
  }
  if(c.diagnostic){window.waTrace.enable(false);row.tracePages=(row.tracePages??[]).concat(window.waTrace.take());row.traceEndAt=performance.now();}
  await sdk.selectTool('select');const cleanup=emptyViewPoint(state().camera);row.cleanup=cleanup;pointer('down',...cleanup.view);pointer('up',...cleanup.view);await settle();check(sdk.readSelection().count===0,'snapshot HUD must be absent');
  row.final=await snapshot('final');await sdk.undo();await settle();row.undo=await snapshot('undo');await sdk.redo();await settle();row.redo=await snapshot('redo');
  row.entropy=globalThis.exactFixtureEntropy.transcript();row.end=state();row.activity=observer.activity();check(row.end.activeContacts===0&&row.end.generation===row.end.lastCommit.presentedGeneration,'terminal receipt');check(row.errors.length===0,'public errors');
  row.status='OBSERVED';return row;
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);throw e;}
 finally{end();observer.restore();if(sdk)await sdk.close();await window.waMetadata(row);}
}
const qualificationFactory=`import factory from '/engine/internal/axiom.mjs';export default async function(options){const m=await factory(options);if(m._axiom_g5_trace_control(2,731)!==0)throw Error('private trace initialization');
const drain=()=>{const pages=[];for(let i=0;i<1024;++i){if(m._axiom_g5_trace_control(4,0)!==0)throw Error('trace drain');const p=m._axiom_g5_trace_bytes(),n=m._axiom_g5_trace_size(),v=JSON.parse(new TextDecoder().decode(m.HEAPU8.subarray(p,p+n)));pages.push(v);if(v.finished)return pages;}throw Error('trace drain pages exhausted');};
const collected=[];window.waTrace={before(moves,round){collected.push(...drain());},after(moves,round){collected.push(...drain());},enable(on){if(m._axiom_g5_trace_control(on?1:0,0)!==0)throw Error('trace toggle');},take(){collected.push(...drain());return collected.splice(0);}};return m;}`;
export async function runWindow(request,out){
 const root=path.resolve(new URL('../../..',import.meta.url).pathname);await fs.mkdir(out,{recursive:true});let server,browser,page,activeSDK;
 const result={id:request.id,status:'STARTED',rows:[],errors:[],pilot:null,pairComparisons:[]};
 try{
  server=http.createServer(async(req,res)=>{try{const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
   if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{webConsumer}from"/driver.mjs";window.run=webConsumer;</script>');return;}
   if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+webConsumer.toString());return;}
   if(request.qualification&&name==='/sdk/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(qualificationFactory);return;}
   if(name==='/fixture.axsnap')file=request.fixture.path;else if(name.startsWith('/sdk/'))file=path.join(activeSDK,name.slice(5));else if(name.startsWith('/engine/'))file=path.join(activeSDK,name.slice(8));else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));
   if(!file)throw Error('unknown HTTPpath');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of request.cases){activeSDK=spec.sdk??request.sdk;const name=spec.id,directory=path.join(out,name);await fs.mkdir(directory);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy('+spec.seed+');'});const transfers=new Map();page.on('pageerror',e=>result.errors.push(String(e)));
   await page.exposeFunction('waMetadata',v=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(v,null,2)+'\n'));
   await page.exposeFunction('waChunk',async c=>{const old=transfers.get(c.name);if(old?.complete)throw Error('completed buffer replay');const next=validateChunk(c,old),bytes=Buffer.from(c.base64,'base64'),p=path.join(directory,c.name+'.partial'),h=await fs.open(p,c.offset?'r+':'wx');try{const n=await h.write(bytes,0,bytes.length,c.offset);if(n.bytesWritten!==bytes.length)throw Error('shortwrite');}finally{await h.close();}if(next.complete)await fs.rename(p,path.join(directory,c.name));transfers.set(c.name,next);return{nextOffset:next.nextOffset};});
   console.log(JSON.stringify({stage:'case-start',id:name,...spec}));
   try{
    await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);
    // The factory is initialized inside createRuntime. Enable only when the
    // consumer reaches the measured gesture after setup.
    const row=await page.evaluate(config=>window.run(config),{...spec,fixture:request.fixture,diagnostic:request.qualification});
    if(request.qualification)row.traceAfterClose=await page.evaluate(()=>window.waTrace?.take());
    const checker=path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip');row.snapshotRoundtrip=JSON.parse(execFileSync(checker,[path.join(directory,'final.axsnap'),path.join(directory,'redo.axsnap')],{encoding:'utf8',timeout:10000}));
    if(!row.snapshotRoundtrip.objects_equal||!row.snapshotRoundtrip.document_digest_equal)throw Error('canonical UndoRedo mismatch');row.status='QUALIFIED';await fs.writeFile(path.join(directory,'qualified-row.json'),JSON.stringify(row,null,2)+'\n');result.rows.push({...row,id:name});
   }catch(e){const failure={id:name,status:'FAILED',error:String(e.stack??e)};try{failure.partial=await page.evaluate(()=>window.waPartial);}catch{}await fs.writeFile(path.join(directory,'failure.json'),JSON.stringify(failure,null,2)+'\n');result.rows.push(failure);
    // Verification-row bugs are isolated and do not hide independent kinds.
    if(/Target crashed|browser.*closed|canonical UndoRedo mismatch|OUT_OF_MEMORY/.test(failure.error))throw e;
   }finally{if(page){await page.close();page=null;}}
   console.log(JSON.stringify({stage:'case-end',id:name,status:result.rows.at(-1).status}));
   if(spec.variant==='previous'||spec.variant==='candidate'){
    const oldSpec=request.cases.find(x=>x.variant==='old'&&x.pair===spec.pair&&x.mode===spec.mode);
    if(oldSpec){const proof={pair:spec.pair,mode:spec.mode,old:oldSpec.id,candidate:spec.id,variant:spec.variant,stages:{},entropy_equal:false};
     const oldDir=path.join(out,oldSpec.id),nextDir=path.join(out,spec.id);
     for(const stage of['initial','final','undo','redo'])proof.stages[stage]=JSON.parse(execFileSync(path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip'),[path.join(oldDir,stage+'.axsnap'),path.join(nextDir,stage+'.axsnap')],{encoding:'utf8',timeout:10000}));
     const a=JSON.parse(await fs.readFile(path.join(oldDir,'qualified-row.json'),'utf8')),b=JSON.parse(await fs.readFile(path.join(nextDir,'qualified-row.json'),'utf8'));
     proof.entropy_equal=JSON.stringify(a.entropy)===JSON.stringify(b.entropy);
     proof.status=Object.values(proof.stages).every(x=>x.objects_equal&&x.manifest_background_equal&&x.document_digest_equal)&&proof.entropy_equal?'QUALIFIED':'EXACT_CANONICAL_MISMATCH';
     await fs.writeFile(path.join(out,spec.pilot?'pilot.json':`pair${spec.pair}-${spec.variant}-${spec.mode}-canonical.json`),JSON.stringify(proof,null,2)+'\n');
     if(spec.pilot)result.pilot=proof;else result.pairComparisons.push(proof);
     if(proof.status!=='QUALIFIED')throw Error('EXACT_FIXED_SCENE_PILOT_MISMATCH');
    }
   }
   if(spec.pilot&&result.rows.at(-1).status!=='QUALIFIED')throw Error('EXACT_FIXED_SCENE_PILOT_MISMATCH');

  }
  result.status=result.rows.every(r=>r.status==='QUALIFIED')&&!result.errors.length?'OBSERVED':'PARTIAL';
 }catch(e){result.status='FAILED';result.error=String(e.stack??e);}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const r=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await runWindow(r,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
