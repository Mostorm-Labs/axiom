export async function webConsumer(c){
 const{createRuntime}=await import('/sdk/axiom.mjs');
 const{installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const{typedWitness,qualifyWitness,navigationWitness}=await import('/g5/exact_query_working_set_contract.mjs');
 const{observeCorrectFeedback}=await import('/g5/erase_exact_canonical_contract.mjs');
 const{emptyViewPoint}=await import('/g5/exact_fixed_scene_contract.mjs');
 const row={kind:c.kind,mode:c.mode,rounds:[],errors:[],operations:[],frames:[],events:[]};window.waPartial=row;window.hpTraceStore=[];
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};let sdk,clock=90000000000;
 const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
 const observer=installNavigationObserver(()=>sdk?.canvas),state=()=>sdk.readState();let observing=false;
 const begin=()=>{observer.begin(row.observation={});observing=true;};const end=()=>{if(observing){observer.end(row.observation);observing=false;}};
 const pointer=(phase,x,y)=>{clock+=8000000;const rect=sdk.canvas.getBoundingClientRect(),e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:clock/1e6});const start=performance.now();observer.dispatch(sdk.canvas,e);const s=state();const r={phase,x,y,timestampNs:String(clock),start,end:performance.now(),contacts:s.activeContacts,generation:s.generation};row.events.push(r);return r;};
 const settle=async()=>{const start=performance.now();for(;;){const s=state();if(!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending)return performance.now();check(performance.now()-start<25000,'stable exact tail exceeded25s');await pause(20);}};
 const snapshot=async label=>{const s=state(),v=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks(label+'.axsnap',v.bytes,window.waChunk);const p=readWebGLTarget(sdk.canvas,{role:'canonical'});row.readbacks??=[];row.readbacks.push({label,metadata:p.metadata});check(p.metadata.status==='VALID','target readback');await sendBinaryChunks(label+'.rgba',p.bytes,window.waChunk);return{sha256:await digest(v.bytes),byteLength:v.bytes.length,state:s,rgba_sha256:await digest(p.bytes),readback:p.metadata};};
 const erase=async(moves,round=0)=>{const before=state(),r={moves,round,before,events:[]};let x=420-round*18,y=160;r.down=pointer('down',x,y);check(r.down.contacts===1,'Down rejected');
  for(let i=1;i<=moves;++i){x=420-round*18+7*Math.sin(i*.17);y=160+170*i/moves;r.events.push(pointer('move',x,y));if(i%8===0)await pause(c.paced?16:0);}
  if(c.diagnostic&&!c.tracePreparation)window.waTrace.enable(c.mode!=='growth'||[0,5,9].includes(round));
  r.up=pointer('up',x,y);if(c.diagnostic&&!c.tracePreparation)window.waTrace.enable(false);r.afterUp=state();const sx=c.mode==='growth'?x+1:300,sy=c.mode==='growth'?y+1:230;
  r.nextDown=pointer('down',sx,sy);r.nextMove=pointer('move',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:242);
  if(c.mode!=='growth')for(let i=1;i<=8;++i)r.events.push(pointer('move',300,242+i*3));
  if(c.diagnostic&&!c.tracePreparation)window.waTrace.enable(c.mode!=='growth'||[0,5,9].includes(round));
  r.nextUp=pointer('up',c.mode==='growth'?sx+1:300,c.mode==='growth'?sy+3:266);if(c.diagnostic&&!c.tracePreparation)window.waTrace.enable(false);r.afterNext=state();r.undoReadiness={at:performance.now(),canUndo:state().canUndo,generation:state().generation};
  if(c.diagnostic)window.hpTraceStore.push(...window.waTrace.take());row.rounds.push(r);return r;};
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());check(await digest(bytes)===c.fixture.sha256,'fixture identity');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,message:String(e.message??e)}),onOperation:b=>row.operations.push({at:performance.now(),bytes:b.length}),onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settle();check(state().objects===40,'fixed40 restore');
  const imageBytes=Uint8Array.from([0x89,0x50,0x4e,0x47,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,207,192,240,31,0,5,0,1,255,137,153,61,29,0,0,0,0,73,69,78,68,174,66,96,130]);
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();row.initial=state();row.initialSnapshot=await snapshot('initial');row.initialReadyAt=performance.now();
  begin();if(c.diagnostic&&c.tracePreparation)window.waTrace.enable(true);const count=c.mode==='growth'?10:1;for(let round=0;round<count;++round)await erase(c.mode==='growth'?160:1600,round);row.sequenceEndAt=performance.now();row.readyAt=await settle();row.terminalAt=performance.now();row.terminalState=state();end();
  if(c.diagnostic){window.waTrace.enable(false);window.hpTraceStore.push(...window.waTrace.take());row.traceEndAt=performance.now();}
  await sdk.selectTool('select');const cleanup=emptyViewPoint(state().camera);row.cleanup=cleanup;pointer('down',...cleanup.view);pointer('up',...cleanup.view);await settle();check(sdk.readSelection().count===0,'snapshot HUD must be absent');
  row.final=await snapshot('final');await sdk.undo();await settle();row.undo=await snapshot('undo');await sdk.redo();await settle();row.redo=await snapshot('redo');
  row.entropy=globalThis.exactFixtureEntropy.transcript();row.end=state();row.activity=observer.activity();check(row.end.activeContacts===0&&row.end.generation===row.end.lastCommit.presentedGeneration,'terminal receipt');check(row.errors.length===0,'public errors');
  row.status='OBSERVED';return row;
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failureState=state();row.failureActivity=observer.activity();row.failureAt=performance.now();const p=readWebGLTarget(sdk.canvas,{role:'canonical'});row.failureReadback=p.metadata;if(p.bytes)await sendBinaryChunks('failure.rgba',p.bytes,window.waChunk);}catch(error){row.failureCaptureError=String(error);}return row;}
 finally{end();observer.restore();if(sdk){try{if(state().activeContacts)pointer('cancel',row.events.at(-1)?.x??32,row.events.at(-1)?.y??900);}catch(error){row.cancelError=String(error.stack??error);}try{await sdk.close();row.closed=true;}catch(error){row.cleanupError=String(error.stack??error);}}if(c.diagnostic){let part=0;for(const page of window.hpTraceStore){await window.hpTraceChunk({index:part++,page});}row.traceParts=part;window.hpTraceStore.length=0;}await window.waMetadata(row);}
}
export const qualificationFactory=`import factory from '/engine/internal/axiom.mjs';export default async function(options){const m=await factory(options);if(m._axiom_g5_trace_control(2,731)!==0)throw Error('private trace initialization');
const drain=()=>{const pages=[];for(let i=0;i<1024;++i){if(m._axiom_g5_trace_control(4,0)!==0)throw Error('trace drain');const p=m._axiom_g5_trace_bytes(),n=m._axiom_g5_trace_size(),v=JSON.parse(new TextDecoder().decode(m.HEAPU8.subarray(p,p+n)));pages.push(v);if(v.finished)return pages;}throw Error('trace drain pages exhausted');};
const collected=[];window.waTrace={before(moves,round){collected.push(...drain());},after(moves,round){collected.push(...drain());},enable(on){if(m._axiom_g5_trace_control(on?1:0,0)!==0)throw Error('trace toggle');},take(){collected.push(...drain());return collected.splice(0);}};return m;}`;
