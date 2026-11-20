import {installPreviewObserver} from './observer.mjs';
import {readWebGLTarget} from '../webgl_target_readback.mjs';
export async function consume(q){
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};
 const all={id:q.id,kind:q.kind,status:'STARTED',cases:[],errors:[],started:performance.now()};
 let sdk,observer,current;
 const stage=async name=>{all.stage=name;await window.hdMetadata(all);};
 const state=()=>sdk.readState();
 const stable=async()=>{const begin=performance.now();let quiet=0;for(;;){const s=state();
  if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration&&observer.pending()===0)++quiet;else quiet=0;
  if(quiet>=3)return s;check(performance.now()-begin<45000,'settle timeout '+all.stage);await pause(16);
 }};
 const persist=async(name,b)=>{for(let i=0;i<b.length;i+=262144){let text='';for(let k=i;k<Math.min(i+262144,b.length);k+=8192)text+=String.fromCharCode(...b.subarray(k,Math.min(k+8192,b.length)));await window.hdBytes({name,offset:i,total:b.length,base64:btoa(text)});}return{name,bytes:b.length};};
 const snapshot=async name=>{const s=state(),v=await sdk.captureSnapshot({generation:s.generation,documentRevision:'1',recoveryFrontier:[8,1,16,1,24,1]});return {...await persist(name+'.axsnap',v.bytes),metadata:{generation:v.generation,checkpointDigest:v.checkpointDigest,throughCommit:v.throughCommit}};};
 const capture=async(role,name)=>{check(q.kind==='correctness','readback in timed lane');const canvas=role==='preview'?document.querySelector('canvas[id$="-preview"]'):sdk.canvas;
  const v=readWebGLTarget(canvas,{role});check(v.bytes&&v.metadata.status==='VALID'&&v.metadata.restoration.status==='RESTORED','qualified readback unavailable');
  return {...await persist(name+'.rgba',v.bytes),readback:v.metadata};
 };
 try{
  const {createRuntime}=await import('/sdk/axiom.mjs');
  current={profile:q.profile,dpr:q.dpr,status:'STARTED',operations:[],frames:[],captures:[]};all.cases.push(current);
  observer=installPreviewObserver(()=>document.querySelector('canvas[id$="-preview"]'));
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'10000000000000000000000000000001',builtinFonts:false,permission:'localSingleUser',
   onError:e=>all.errors.push({code:e.code,message:e.message,at:performance.now()}),onOperation:bytes=>{const row={at:performance.now(),length:bytes.length};if(q.kind==='correctness')row.data=new Uint8Array(bytes);current.operations.push(row);},onFrame:f=>current.frames.push({at:performance.now(),value:f})});
  // Synthetic pointer facts use the real collector; capture is not meaningful
  // for untrusted events and is disabled only on this owned qualification page.
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await sdk.selectBrush(q.profile,3);const config=sdk.readBrushConfig();
  await sdk.setBrushConfig({...config,size:32,color:[.12,.27,.61,1],opacity:.65,pressureSource:1,startTaper:0,endTaper:0,startCap:true,endCap:true});
  await sdk.selectTool('ink');await stable();
  current.initial=state();current.build=sdk.readBuildInfo();current.config=sdk.readBrushConfig();
  current.entropyBefore=globalThis.exactFixtureEntropy.transcript();
  const canvas=sdk.canvas,preview=document.querySelector('canvas[id$="-preview"]'),rect=canvas.getBoundingClientRect();
  current.dimensions={logical:[rect.width,rect.height],canonical:[canvas.width,canvas.height],preview:[preview.width,preview.height],dpr:devicePixelRatio};
  check(rect.width===1431&&rect.height===1010&&canvas.width===1431*q.dpr&&canvas.height===1010*q.dpr&&preview.width===canvas.width&&preview.height===canvas.height,'dimensions');
  const point=i=>[100+1.65*i,505+170*Math.sin(i*.043)+38*Math.sin(i*.137)];
  const dispatch=(type,i)=>{const [x,y]=point(i);const event=new PointerEvent('pointer'+type,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:type==='move'?-1:0,buttons:type==='up'||type==='cancel'?0:1,pressure:type==='up'?0:.5,bubbles:true,cancelable:true});
   Object.defineProperty(event,'timeStamp',{value:1000+i*12000/720});const started=performance.now();observer.dispatch(canvas,event,i);return {index:i,type,started,ended:performance.now()};};
  await stage('ready');current.inputs=[];observer.begin();current.down=performance.now();current.inputs.push(dispatch('down',0));
  for(let i=1;i<=720;++i){const planned=current.down+i*12000/720;await pause(Math.max(0,planned-performance.now()));current.inputs.push({...dispatch('move',i),planned});}
  current.lastMoveReturned=performance.now();current.entropyHeld=globalThis.exactFixtureEntropy.transcript();
  if(q.kind==='correctness'){
   await pause(100);current.heldState=state();current.captures.push(await capture('preview',q.caseId+'-held'));
  }
  current.up=performance.now();current.inputs.push(dispatch('up',720));current.upReturned=performance.now();
  await stable();current.stableAt=performance.now();current.final=state();current.observation=observer.end();
  check(!current.observation.dropped,'observer overflow');check(!all.errors.length,'SDK errors');check(current.final.objects===1,'canonical stroke missing');
  current.entropyFinal=globalThis.exactFixtureEntropy.transcript();
  if(q.kind==='correctness'){
   current.captures.push(await capture('canonical',q.caseId+'-canonical'));current.snapshot=await snapshot(q.caseId+'-final');
   current.selection=sdk.readSelection();
   await sdk.undo();await stable();current.afterUndo=state();await sdk.redo();await stable();current.afterRedo=state();
   current.redoSnapshot=await snapshot(q.caseId+'-redo');
   const operations=[];for(let i=0;i<current.operations.length;++i){const op=current.operations[i];if(op.data)operations.push({...await persist(q.caseId+'-op-'+i+'.bin',op.data),at:op.at});}current.operations=operations;
   dispatch('down',0);dispatch('move',1);dispatch('cancel',1);await stable();current.afterCancel=state();
   check(current.afterCancel.objects===1,'cancel mutated document');
  }
  await sdk.close();sdk=null;current.closed=true;current.status='OBSERVED';observer.restore();observer=null;all.status='OBSERVED';
 }catch(e){all.status='FAILED';all.error=String(e.stack??e);try{all.failureState=state();}catch{}}
 finally{
  if(sdk)try{if(observer)current.observation=observer.end();await sdk.close();current.closed=true;}catch(e){all.closeError=String(e);}
  observer?.restore();all.closed=!!current?.closed;all.ended=performance.now();await stage('closed');
 }return all;
}
