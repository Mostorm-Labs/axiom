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
 const pointer=(phase,x,y)=>{clock+=8000000;const rect=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);const s=state();const r={phase,x,y,timestampNs:String(clock),start,end:performance.now(),contacts:s.activeContacts,generation:s.generation};row.events.push(r);return r;};
 const settle=async()=>{const start=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now();check(performance.now()-start<25000,'stable exact tail exceeded25s');await pause(20);}};
 const snapshot=async label=>{const s=state(),v=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks(label+'.axsnap',v.bytes,window.waChunk);const p=readWebGLTarget(sdk.canvas,{role:'canonical'});row.readbacks??=[];row.readbacks.push({label,metadata:p.metadata});check(p.metadata.status==='VALID','target readback');await sendBinaryChunks(label+'.rgba',p.bytes,window.waChunk);return{sha256:await digest(v.bytes),byteLength:v.bytes.length,state:s,rgba_sha256:await digest(p.bytes),readback:p.metadata};};
 const erase=async(moves,round=0)=>{const before=state(),r={moves,round,before,events:[]};let x=420-round*18,y=160;r.down=pointer('down',x,y);check(r.down.contacts===1,'Down rejected');
  for(let i=1;i<=moves;++i){x=420-round*18+7*Math.sin(i*.17);y=160+170*i/moves;r.events.push(pointer('move',x,y));if(i%8===0)await pause(0);}
  if(c.diagnostic)window.waTrace.enable(c.traceOn!==false);
  r.up=pointer('up',x,y);if(c.diagnostic)window.waTrace.enable(false);r.afterUp=state();const sx=c.mode==='growth'?x+1:300,sy=c.mode==='growth'?y+1:230;
  r.nextDown=pointer('down',sx,sy);r.nextMove=pointer('move',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:242);
  if(c.mode!=='growth')for(let i=1;i<=8;++i)r.events.push(pointer('move',300,242+i*3));
  if(c.diagnostic)window.waTrace.enable(c.traceOn!==false);
  r.nextUp=pointer('up',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:266);if(c.diagnostic)window.waTrace.enable(false);r.afterNext=state();r.undoReadiness={at:performance.now(),canUndo:state().canUndo,generation:state().generation};
  if(c.diagnostic)row.tracePages=(row.tracePages??[]).concat(window.waTrace.take());row.rounds.push(r);return r;};
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===c.fixture.sha256,'fixture identity');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();check(state().objects===40,'fixed40 restore');
  const imageBytes=Uint8Array.from([0x89,0x50,0x4e,0x47,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130]);
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();row.initial=state();row.initialSnapshot=await snapshot('initial');row.initialReadyAt=performance.now();
  begin();const count=c.mode==='growth'?10:1;for(let round=0;round<count;++round)await erase(c.mode==='growth'?160:1600,round);row.sequenceEndAt=performance.now();row.readyAt=await settle();row.terminalAt=performance.now();row.terminalState=state();end();
  if(c.diagnostic){window.waTrace.enable(false);row.tracePages=(row.tracePages??[]).concat(window.waTrace.take());row.traceEndAt=performance.now();}
  await sdk.selectTool('select');const cleanup=emptyViewPoint(state().camera);row.cleanup=cleanup;pointer('down',...cleanup.view);pointer('up',...cleanup.view);await settle();check(sdk.readSelection().count===0,'snapshot HUD must be absent');
  row.final=await snapshot('final');await sdk.undo();await settle();row.undo=await snapshot('undo');await sdk.redo();await settle();row.redo=await snapshot('redo');
  row.entropy=globalThis.exactFixtureEntropy.transcript();row.end=state();row.activity=observer.activity();check(row.end.activeContacts===0&&row.end.generation===row.end.lastCommit.presentedGeneration,'terminal receipt');check(row.errors.length===0,'public errors');
  row.status='OBSERVED';return row;
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failureState=state();row.failureActivity=observer.activity();row.failureAt=performance.now();}catch{}throw e;}
 finally{end();observer.restore();if(sdk){try{if(state().activeContacts)pointer('cancel',row.events.at(-1)?.x??32,row.events.at(-1)?.y??900);await sdk.close();}catch(error){row.cleanupError=String(error.stack??error);}}await window.waMetadata(row);}
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
  for(const spec of request.cases){activeSDK=spec.sdk??request.sdk;const name=spec.id,directory=path.join(out,name);await fs.mkdir(directory);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy('+spec.seed+');'});const transfers=new Map();const consoleLines=[];page.on('console',m=>{if(consoleLines.length<4096)consoleLines.push({type:m.type(),text:m.text().slice(0,8192)});});page.on('pageerror',e=>result.errors.push(String(e)));
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
   }finally{await fs.writeFile(path.join(directory,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');if(page){await page.close();page=null;}}
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
