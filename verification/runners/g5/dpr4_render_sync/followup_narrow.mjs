// Correctness-only serial follow-up: immutable r1 ordinary SDK versus repaired SDK.
// Replays the existing 18 Operations, then one erase/camera/return sequence.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {chromium} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';
import {entropyStream,installEntropy} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/runners/g5/exact_fixed_scene_contract.mjs';
const [baseline,candidate,fixture,out,reuseReference]=process.argv.slice(2);
if(!baseline||!candidate||!fixture||!out)throw Error('baseline/candidate/fixture/output required');
const sha=b=>createHash('sha256').update(b).digest('hex');
const helpers='/Users/qing/.codex/worktrees/g5-wax-postup-navigation/verification/runners/g5';
const result={purpose:'narrow ordinary exported SDK correctness; not performance sampling',status:'STARTED',driverSha256:sha(await fs.readFile(new URL(import.meta.url))),cases:[],errors:[]};
if(reuseReference){const previous=JSON.parse(await fs.readFile(reuseReference));const reference=previous.cases.find(c=>c.label==='r1'&&c.status==='OBSERVED');if(!reference)throw Error('exact retained r1 reference unavailable');result.cases.push(reference);result.reusedReference={path:reuseReference,sha256:sha(await fs.readFile(reuseReference))};}
await fs.mkdir(out,{recursive:true});
const save=()=>fs.writeFile(path.join(out,'observation.json'),JSON.stringify(result,null,2)+'\n');
let browser,server;
try{
 for(const [label,sdk] of (reuseReference?[['repaired',candidate]]:[['r1',baseline],['repaired',candidate]])){
  const row={label,sdk,status:'STARTED',served:{},artifacts:{}};result.cases.push(row);await save();
  server=http.createServer(async(req,res)=>{try{
   const n=new URL(req.url,'http://localhost').pathname;let bytes;
   if(n==='/'){res.setHeader('Content-Type','text/html');res.end('<style>html,body{margin:0}#mount{position:relative;width:900px;height:650px}</style><div id="mount"></div>');return;}
   if(n==='/favicon.ico'){res.statusCode=204;res.end();return;}
   if(n.includes('..'))throw Error('path');
   if(n.startsWith('/sdk/'))bytes=await fs.readFile(path.join(sdk,n.slice(5)));
   else if(n.startsWith('/g5/'))bytes=await fs.readFile(path.join(helpers,n.slice(4)));
   else if(n.startsWith('/operation/'))bytes=await fs.readFile(path.join(fixture,'wax16-default-operation-'+Number(n.slice(11))+'.bin'));
   else throw Error('route');
   row.served[n]={bytes:bytes.length,sha256:sha(bytes)};
   res.setHeader('Content-Type',n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(bytes);
  }catch(error){result.errors.push(String(error));res.statusCode=404;res.end(String(error));}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});row.browser=browser.version();
  const context=await browser.newContext({viewport:{width:900,height:650},deviceScaleFactor:4});
  const page=await context.newPage();page.on('pageerror',error=>result.errors.push(String(error)));page.on('crash',()=>{result.crash=true;});
  await page.exposeFunction('saveNarrowDiagnostic',async value=>{row.live=value;await save();});
  await page.exposeFunction('saveArtifact',async({name,index,last,bytes})=>{
   const p=path.join(out,label+'-'+name);const chunk=typeof bytes==='string'?Buffer.from(bytes,'base64'):Buffer.from(bytes);if(index===0)await fs.writeFile(p,chunk);else await fs.appendFile(p,chunk);
   if(last){const b=await fs.readFile(p);row.artifacts[name]={path:p,bytes:b.length,sha256:sha(b)};await save();}
  });
  await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(610839776);'});
  await page.goto('http://127.0.0.1:'+server.address().port+'/');
  row.observed=await page.evaluate(async()=>{
   const {createRuntime}=await import('/sdk/axiom.mjs');
   const {installNavigationObserver}=await import('/g5/wax_postup_navigation/observer.mjs');
   const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
   const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));const check=(v,m)=>{if(!v)throw Error(m);};
   let sdk;const frames=[],errors=[],operations=[];
   const observer=installNavigationObserver(()=>[...document.querySelectorAll('#mount canvas')].map(canvas=>({canvas,role:canvas.id.endsWith('-canonical')?'canonical':'preview'})),globalThis,{trace:false});
   sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'49000000000000000000000000000001',builtinFonts:false,permission:'localSingleUser',onError:e=>{errors.push({code:e.code,message:e.message});void window.saveNarrowDiagnostic({errors,state:sdk?.readState()});},onFrame:f=>frames.push({at:performance.now(),...f,camera:sdk?.readState().camera}),onOperation:b=>operations.push(b.slice())});
   sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
   const settle=async()=>{let quiet=0;const start=performance.now();for(;;){const s=sdk.readState(),frame=frames.at(-1);
    const matches=frame?.camera&&Math.abs(frame.camera.scale-s.camera.scale)<1e-6&&Math.abs(frame.camera.x-s.camera.x)<.001&&Math.abs(frame.camera.y-s.camera.y)<.001;
    quiet=!s.activeContacts&&s.generation===s.lastCommit.presentedGeneration&&!observer.activity().pending&&matches?quiet+1:0;
    if(quiet>=3)return s;check(performance.now()-start<45000,'narrow settle timeout');await pause(16);}};
   const stream=async(name,bytes)=>{const count=1048576;for(let offset=0,index=0;offset<bytes.length;offset+=count,index++){const chunk=bytes.subarray(offset,offset+count);let text='';for(let i=0;i<chunk.length;i+=8192)text+=String.fromCharCode(...chunk.subarray(i,i+8192));await window.saveArtifact({name,index,last:offset+count>=bytes.length,bytes:btoa(text)});}};
   for(let i=0;i<18;i++){const bytes=new Uint8Array(await(await fetch('/operation/'+i)).arrayBuffer());await sdk.applyReplayOperation(bytes);}
   await settle();check(sdk.readState().objects===16&&sdk.readState().generation==='18','frozen replay state');
   await sdk.zoomAt(450,325,1.5);await settle();
   await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();
   const pointer=(phase,x,y,stamp)=>{const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:x,clientY:y,button:phase==='move'?-1:0,buttons:phase==='up'?0:1,pressure:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:stamp});sdk.canvas.dispatchEvent(e);};
   // Choose a visible ink interior from the actual canonical pixels; this is
   // a correctness fixture location, not a second stroke/geometry evaluator.
   const prior=readWebGLTarget(sdk.canvas,{role:'canonical'});let hit=null;
   for(let y=100;y<550&&!hit;y++)for(let x=100;x<800;x++){
    const at=((y*4)*sdk.canvas.width+x*4)*4;
    if(prior.bytes[at]<180&&prior.bytes[at+2]>prior.bytes[at]+20){hit=[x,y];break;}}
   check(hit,'visible blue ink fixture location');
   const phase={started:performance.now(),hit};pointer('down',hit[0],hit[1],2000000);
   for(let i=1;i<=24;i++){await pause(16);pointer('move',hit[0]+8*Math.sin(i*.2),hit[1]+20*i/24,2000000+i*16);}
   pointer('up',hit[0]+8*Math.sin(4.8),hit[1]+20,2000385);phase.up=performance.now();
   // Same immediate command sequence for both ordinary SDKs; no new performance assertion.
   await sdk.panBy(-48,0);await settle();phase.cameraSettled=performance.now();
   const eraseState=sdk.readState();await window.saveNarrowDiagnostic({eraseState,phase,errors,newOperations:operations.length});
   await sdk.zoomAt(450,325,1);await settle();phase.returnSettled=performance.now();
   const state=sdk.readState(),snapshot=await sdk.captureSnapshot({generation:state.generation,documentRevision:'31',recoveryFrontier:[8,1,16,1,24,1]});
   await stream('final.axsnap',snapshot.bytes);const pixels=readWebGLTarget(sdk.canvas,{role:'canonical'});
   await stream('final.rgba',pixels.bytes);for(let i=0;i<operations.length;i++)await stream('new-operation-'+i+'.bin',operations[i]);
   check(pixels.metadata.status==='VALID','canonical readback qualification');check(errors.length===0,'SDK errors '+JSON.stringify(errors));check(eraseState.generation==='19','one erasure operation: '+JSON.stringify({eraseState,operations:operations.length}));
   const build=sdk.readBuildInfo();await sdk.close();observer.restore();return {state,build,frames,phase,capture:pixels.metadata,errors,newOperations:operations.length,closed:!document.querySelector('#mount canvas')};
  });
  row.status='OBSERVED';await save();await browser.close();browser=null;await new Promise(resolve=>server.close(resolve));server=null;
 }
 result.comparison={};
 for(const name of ['final.axsnap','final.rgba','new-operation-0.bin']){
  const a=result.cases[0].artifacts[name],b=result.cases[1].artifacts[name];result.comparison[name]={baseline:a,candidate:b,equal:!!a&&!!b&&a.bytes===b.bytes&&a.sha256===b.sha256};
 }
 result.status=Object.values(result.comparison).every(v=>v.equal)&&!result.errors.length&&!result.crash?'CORRECTNESS_OBSERVED':'FAILED';
}catch(error){result.status='FAILED';result.error=String(error.stack??error);process.exitCode=1;}
finally{await browser?.close();if(server)await new Promise(resolve=>server.close(resolve));result.closed=true;await save();}
console.log(JSON.stringify({status:result.status,error:result.error,comparison:result.comparison}));
if(result.status!=='CORRECTNESS_OBSERVED')process.exitCode=1;
