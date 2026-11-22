// G5-DPR4-render-sync-r1: same bound default scenario; diagnostic covers both Down events.
// Isolated current DPR4 verification adapted from retained ordinary r3 driver.
// New task ledger; no r3 window or acceptance thresholds are reused.
import fs from'node:fs/promises';import path from'node:path';import http from'node:http';import{createHash}from'node:crypto';
import{chromium}from'/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';
import{entropyStream,installEntropy}from'/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/runners/g5/exact_fixed_scene_contract.mjs';import{metrics,percentile}from'/Users/qing/.codex/worktrees/g5-wax-postup-navigation/verification/runners/g5/wax_postup_navigation/metrics.mjs';
const [sdk,out,mode]=process.argv.slice(2);const diagnostic=mode==='diagnostic',canary=mode==='canary'||diagnostic;if(!sdk||!out)throw Error('SDK/out required');
const helpers='/Users/qing/.codex/worktrees/g5-wax-postup-navigation/verification/runners/g5',sha=b=>createHash('sha256').update(b).digest('hex');
const result={sdk,driverSHA256:sha(await fs.readFile(new URL(import.meta.url))),status:'STARTED',cases:[],errors:[],served:{}};
let browser,server;const nativeConsole=[];const save=()=>fs.writeFile(path.join(out,'observation.json'),JSON.stringify(result,null,2)+'\n');
await fs.mkdir(out,{recursive:true});await save();
try{
 server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://localhost').pathname;let b;if(n==='/'){b=Buffer.from('<style>html,body{margin:0}#mount{position:relative;width:900px;height:650px}</style><div id="mount"></div>');res.setHeader('Content-Type','text/html');}
 else if(n==='/favicon.ico'){res.statusCode=204;res.end();return;}else if(n.startsWith('/sdk/')&&!n.includes('..'))b=await fs.readFile(path.join(sdk,n.slice(5)));else if(n.startsWith('/g5/')&&!n.includes('..'))b=await fs.readFile(path.join(helpers,n.slice(4)));else throw Error('route');
 if(n!=='/')res.setHeader('Content-Type',n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');result.served[n]={bytes:b.length,sha256:sha(b)};res.end(b);}catch(e){result.errors.push(String(e));res.statusCode=404;res.end(String(e));}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});result.browserVersion=browser.version();
 const system=await browser.newBrowserCDPSession();result.system=await system.send('SystemInfo.getInfo');
 for(const id of (canary?['wax16-default']:['wax16-default','wax16-overlap'])){
  console.log(JSON.stringify({stage:'setup',id}));const entry={id};result.cases.push(entry);await save();
  const context=await browser.newContext({viewport:{width:900,height:650},deviceScaleFactor:4}),page=await context.newPage();page.on('pageerror',e=>result.errors.push(String(e)));await page.exposeFunction('persistArtifact',async({name,index,last,bytes})=>{const p=path.join(out,name);if(index===0)await fs.writeFile(p,Buffer.from(bytes));else await fs.appendFile(p,Buffer.from(bytes));if(last){const b=await fs.readFile(p);entry.artifacts??={};entry.artifacts[name]={bytes:b.length,sha256:sha(b)};}});if(diagnostic)page.on('console',m=>nativeConsole.push({at:Date.now(),type:m.type(),text:m.text()}));await page.exposeFunction('saveLiveProgress',async value=>{entry.progress??=[];entry.progress.push(value);await save();});
  await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});await page.goto('http://127.0.0.1:'+server.address().port+'/');
  entry.data=await page.evaluate(async ({id,canary,diagnostic})=>{
   const{createRuntime}=await import('/sdk/axiom.mjs'),{installNavigationObserver}=await import('/g5/wax_postup_navigation/observer.mjs');
   const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};let sdk;const observer=installNavigationObserver(()=>[...document.querySelectorAll('#mount canvas')].filter(c=>c.id.endsWith('-canonical')||c.id.endsWith('-preview')).map(canvas=>({canvas,role:canvas.id.endsWith('-preview')?'preview':'canonical'})),globalThis,{trace:diagnostic}),frames=[],errors=[],operations=[],operationBuffers=[];
   sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'49000000000000000000000000000001',builtinFonts:false,permission:'localSingleUser',onError:e=>errors.push({code:e.code,message:e.message}),onOperation:b=>{operationBuffers.push(b.slice());operations.push({at:performance.now(),bytes:b.length});},onFrame:f=>frames.push({at:performance.now(),...f,semanticPresented:sdk?.readState().lastCommit.presentedGeneration??null,camera:sdk?.readState().camera??null})});
   sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
   const state=()=>sdk.readState(),settle=async()=>{let quiet=0;const started=performance.now();for(;;){const s=state(),a=observer.activity();quiet=!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration&&a.pending===0?quiet+1:0;
    if(quiet>=3)return{elapsed:performance.now()-started,state:s};if(performance.now()-started>40000)throw Error('truequiet timeout');await pause(16);}};
   const pointer=(phase,x,y,pointerId=1,type='mouse',timeStamp)=>{const e=new PointerEvent('pointer'+phase,{pointerId,pointerType:type,clientX:x,clientY:y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:.5,width:type==='touch'?10:1,height:type==='touch'?10:1,bubbles:true,cancelable:true});if(timeStamp!==undefined)Object.defineProperty(e,'timeStamp',{value:timeStamp});const started=performance.now();observer.dispatch(sdk.canvas,e);return{phase,x,y,pointerId,type,started,ended:performance.now(),timeStamp:e.timeStamp};};
   await sdk.selectBrush('crayon-wax-v1',3);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:32,opacity:.65,color:[.12,.27,.61,1],pressureSource:1,startTaper:0,endTaper:0});await sdk.selectTool('ink');await settle();
   const point=i=>[360+220*Math.sin(i*.012)+35*Math.sin(i*.043),300+140*Math.sin(i*.019)],drawEvents=[];drawEvents.push(pointer('down',...point(0),1,'mouse',1000));const began=performance.now();for(let i=1;i<=1800;i++){await pause(Math.max(0,began+i*5-performance.now()));drawEvents.push(pointer('move',...point(i),1,'mouse',1000+i*5));}drawEvents.push(pointer('up',...point(1800),1,'mouse',10005));await settle();
   await sdk.selectTool('select');await sdk.setSelectionMode('point');pointer('down',360,300);pointer('up',360,300);check(sdk.readSelection().count===1,'select first stroke');await settle();
   const duplicates=[];for(let i=1;i<=15;i++){const phase={name:'duplicate'+i,started:performance.now()};observer.begin(phase);const value=id==='wax16-default'?await sdk.duplicateSelection():await sdk.duplicateSelection({offset:[0,0]});phase.returned=performance.now();phase.commandResult=value;phase.generation=state().generation;await settle();phase.ended=performance.now();phase.firstFrame=frames.find(f=>f.at>=phase.started&&f.semanticPresented===phase.generation)?.at;observer.end(phase);duplicates.push(phase);await window.saveLiveProgress({stage:phase.name,phase});}
   check(state().objects===16,'16 objects');await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await settle();
   const repeats=[];for(let repeat=0;repeat<2;repeat++){
    const phase={name:repeat===0?'up-immediate-pan':'up-immediate-pinch',started:performance.now(),before:state(),events:[]};observer.begin(phase);phase.eraseStarted=performance.now();const eraseX=repeat===0?360:420,eventBase=1000000+repeat*10000;phase.events.push(pointer('down',eraseX,190,1,'mouse',eventBase));const start=performance.now();
    for(let i=1;i<=120;i++){const due=start+i*2000/120;await pause(Math.max(0,due-performance.now()));const e=pointer('move',eraseX+12*Math.sin(i*.12),190+220*i/120,1,'mouse',eventBase+i*2000/120);e.due=due;e.lateness=e.started-due;phase.events.push(e);}
    phase.preUp=state();phase.up=pointer('up',eraseX+12*Math.sin(120*.12),410,1,'mouse',eventBase+2001);phase.events.push(phase.up);phase.eraseGeneration=state().generation;phase.afterUp=state();phase.panStart=performance.now();
    const gesture=async(inverse=false)=>{const span=inverse?105:70,ratio=inverse?1/1.5:1.5;
     const gestureBase=eventBase+(inverse?5000:3000);phase.events.push(pointer('down',615-span/2,450,21,'touch',gestureBase));phase.events.push(pointer('down',615+span/2,450,22,'touch',gestureBase));if(!inverse)phase.panContacts=state().activeContacts;const began=performance.now();
     for(let i=1;i<=12;i++){const due=began+i*200/12;await pause(Math.max(0,due-performance.now()));const delta=repeat===0&&!inverse?-i*4:0,half=repeat===0&&!inverse?35:span*(1+(ratio-1)*i/12)/2;
      for(const [x,pid]of[[615-half+delta,21],[615+half+delta,22]]){const e=pointer('move',x,450,pid,'touch',gestureBase+i*200/12);e.due=due;e.lateness=e.started-due;phase.events.push(e);}}
     const delta=repeat===0&&!inverse?-48:0,half=repeat===0&&!inverse?35:span*ratio/2;
     phase.events.push(pointer('up',615-half+delta,450,21,'touch',gestureBase+201));phase.events.push(pointer('up',615+half+delta,450,22,'touch',gestureBase+201));
    };
    await gesture();phase.panEnded=performance.now();phase.afterPan=state();phase.settled=await settle();phase.gestureQuietAt=performance.now();
    check(phase.panContacts===2,'touch contacts not admitted');if(repeat===0)check(Math.abs(phase.afterPan.camera.x-phase.before.camera.x+48)<1,'pan displacement');else{
     check(Math.abs(phase.afterPan.camera.scale/phase.before.camera.scale-1.5)<.01,'pinch ratio');
     const before=phase.before.camera,after=phase.afterPan.camera;check(Math.abs(after.x-(615-(615-before.x)*1.5))<1&&Math.abs(after.y-(450-(450-before.y)*1.5))<1,'pinch anchor');
     phase.inverseStarted=performance.now();await gesture(true);phase.inverseEnded=performance.now();phase.inverseSettled=await settle();phase.inverseQuietAt=performance.now();check(Math.abs(state().camera.scale/before.scale-1)<.01,'inverse pinch');
    }
    phase.ended=performance.now();phase.generationFrame=frames.find(f=>f.at>=phase.panEnded&&f.semanticPresented===phase.eraseGeneration&&f.camera&&Math.abs(f.camera.scale-phase.afterPan.camera.scale)<.00001&&Math.abs(f.camera.x-phase.afterPan.camera.x)<.001&&Math.abs(f.camera.y-phase.afterPan.camera.y)<.001)?.at;observer.end(phase);repeats.push(phase);await window.saveLiveProgress({stage:phase.name,phase});
   }
   // Readback/serialization are outside the timed phases and use the actual canonical target.
   const final=state(),config=sdk.readBrushConfig(),build=sdk.readBuildInfo();
   const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
   const stream=async(name,bytes)=>{for(let offset=0,index=0;offset<bytes.length;offset+=49152,index++)await window.persistArtifact({name,index,last:offset+49152>=bytes.length,bytes:Array.from(bytes.subarray(offset,offset+49152))});};
   for(let i=0;i<operationBuffers.length;i++)await stream(id+'-operation-'+i+'.bin',operationBuffers[i]);
   const snapshot=await sdk.captureSnapshot({generation:final.generation,documentRevision:'30',recoveryFrontier:[8,1,16,1,24,1]});await stream(id+'-final.axsnap',snapshot.bytes);
   const pixels=readWebGLTarget(sdk.canvas,{role:'canonical'});check(pixels.metadata.status==='VALID','final canonical readback');await stream(id+'-final.rgba',pixels.bytes);
   const capture={readback:pixels.metadata,generation:snapshot.generation,throughCommit:snapshot.throughCommit};
   check(!errors.length,'ordinary SDK errors');await sdk.close();observer.restore();return{capture,build,config,drawEvents,duplicates,repeats,final,frames,errors,operations,closed:!document.querySelector('#mount canvas')};
  },{id,canary,diagnostic});
  entry.metrics=entry.data.repeats.map(metrics);entry.duplicateMetrics={p95:percentile(entry.data.duplicates.map(p=>p.ended-p.started)),firstFrameP95:percentile(entry.data.duplicates.map(p=>p.firstFrame-p.started)),commandMax:Math.max(...entry.data.duplicates.map(p=>p.returned-p.started)),callbackMaximum:Math.max(...entry.data.duplicates.flatMap(p=>p.rafCallbacks.map(r=>r.ended-r.started)))};
  await save();await context.close();console.log(JSON.stringify({stage:'case-complete',id,metrics:entry.metrics}));
 }
 result.status='OBSERVED';
}catch(e){result.status='FAILED';result.error=String(e.stack??e);process.exitCode=1;}
finally{await browser?.close();if(server)await new Promise(r=>server.close(r));result.closed=true;if(diagnostic)await fs.writeFile(path.join(out,'native-console.json'),JSON.stringify(nativeConsole,null,2)+'\n');await save();console.log(JSON.stringify({stage:'complete',status:result.status,error:result.error}));}
