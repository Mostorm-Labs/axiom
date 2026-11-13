import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import{chromium}from'../../node_modules/playwright/index.mjs';
import{validateChunk}from'./ur2d_selection_up_contract.mjs';
import{entropyStream,installEntropy}from'./exact_fixed_scene_contract.mjs';

export async function consumer(c){
 const{createRuntime}=await import('/sdk/axiom.mjs');
 const{confirmedTrace,navigationEvents,expectedCamera,rowSpec}=await import('/g5/cpu_edit_navigation_contract.mjs');
 const{operationValue,snapshotObjects}=await import('/g5/cpu_v2_wire.mjs');
 const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const{installNavigationObserver}=await import('/g5/navigation_observer.mjs');
 const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={id:c.id,status:'STARTED',errors:[],events:[],frames:[],operations:[],states:{},trace:[],clock:[]};
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);};
 let sdk,observer,observed,traceTimer,active=false,time=90_000_000_000;const ops=[];
 const save=()=>window.enMetadata(row);
 const binary=(name,b)=>sendBinaryChunks(name,b,window.enChunk);
 const state=()=>sdk.readState();
 const stable=async()=>{let count=0;const start=performance.now();for(;;){
   const s=state(),a=observer?.activity();
   if(!s.activeContacts&&!s.pending?.intents.length&&s.generation===s.lastCommit.presentedGeneration&&(!a||a.pending===0))++count;else count=0;
   if(count>=3)return;check(performance.now()-start<20000,'settle20s');await pause(16);
 }};
 const pointer=(phase,x,y,timestampNs)=>{
   time=timestampNs??time+8_000_000;const b=sdk.canvas.getBoundingClientRect();
   const event=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'pen',clientX:b.left+x,clientY:b.top+y,pressure:.5,buttons:phase==='up'||phase==='cancel'?0:1,bubbles:true,cancelable:true});
   Object.defineProperty(event,'timeStamp',{value:time/1e6});
   const started=performance.now();if(active)observer.dispatch(sdk.canvas,event);else sdk.canvas.dispatchEvent(event);
   row.events.push({phase,x,y,timestampNs:time,started,ended:performance.now()});
 };
 const traceControl=(op,value=0)=>{const m=window.enModule;check(m?._axiom_g5_trace_control,'missing qualification trace');return m._axiom_g5_trace_control(op,value);};
 const drain=()=>{if(!c.diagnostic)return;for(let i=0;i<1200;++i){traceControl(4);const text=window.enModule.UTF8ToString(window.enModule._axiom_g5_trace_bytes());const v=JSON.parse(text);row.trace.push(v);row.traceLoss=v.loss;if(v.finished)break;check(i<1199,'trace drain bound');}};
 const snapshot=async label=>{
   const s=state(),saved=await sdk.captureSnapshot({generation:s.generation,documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});
   await binary(label+'.axsnap',saved.bytes);row[label]={state:s,metadata:{documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier,checkpointDigest:saved.checkpointDigest,resourceBinding:'1'},bytes:saved.bytes.length};return saved;
 };
 const capture=async label=>{const v=readWebGLTarget(sdk.canvas,{role:'canonical'});check(v.metadata.status==='VALID','invalid readback');await binary(label+'.rgba',v.bytes);row[label+'Readback']=v.metadata;};
 try{
   const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());
   sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,text:String(e)}),onOperation:b=>{ops.push(b);row.operations.push({at:performance.now(),bytes:b.length});},onFrame:f=>row.frames.push({at:performance.now(),presented:f.presented})});
   sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
   observer=installNavigationObserver(()=>sdk.canvas);row.build=sdk.readBuildInfo();
   const image=new Uint8Array(await(await fetch('/image.png')).arrayBuffer());
   if(c.mode==='author'){
     await stable();check(state().objects===40,'original Fixed40 missing');row.materials=[];
     for(const[profile,family,y]of[['chalk-compact-v1',3,380],['crayon-wax-v1',4,410],['watercolor-lite-v1',5,440]]){
       check(sdk.brushPresets().some(p=>p.profileId===profile&&p.revision===3),'preset missing '+profile);
       await sdk.selectBrush(profile,3);const config=sdk.readBrushConfig();check(config?.family===family&&config.version===2,'public material config');
       pointer('down',250,y);for(let i=1;i<=256;++i)pointer('move',250+i*1.8,y+20*Math.sin(i*.11));pointer('up',999,999);
       await stable();
       row.materials.push({profile,revision:3,config});
     }
     row.fixtureObjects=[];
     for(const[kind,x,y]of[['rectangle',800,100],['triangle',1000,100],['ellipse',1150,100],['sticky',800,400],['table',1030,400]]){
       await sdk.selectShape(kind);pointer('down',x,y);pointer('up',x,y);if(state().text?.phase)await sdk.text.finish();await stable();const s=sdk.readSelection();check(s.primary,'public create selection');row.fixtureObjects.push({kind,id:s.primary,bounds:s.bounds});
     }
     await sdk.importImage(image,'image/png');let selected=sdk.readSelection();await sdk.resizeImage(selected.primary,120,80);selected=sdk.readSelection();const pose=[...selected.transform];pose[4]=800;pose[5]=700;await sdk.transformSelection(pose);selected=sdk.readSelection();row.fixtureObjects.push({kind:'image',id:selected.primary,bounds:selected.bounds});
     if(state().text?.phase)await sdk.text.finish();await sdk.selectTool('ink');await sdk.selectBrush('round-handdrawn',4);await stable();
     check(state().objects===49,'fixed mixed object count');row.resources=sdk.resources.list();
     const saved=await snapshot('fixture');const objects=snapshotObjects(saved.bytes);row.wireObjects=objects;
     const expected=new Map([[3,6],[4,5],[5,6]]);for(const[f,a]of expected)check(objects.some(o=>o.config?.family===f&&o.config.algorithm===a),'actual native material wire '+f);
     check(row.resources.entries.length===1,'image resource catalog');row.status='OBSERVED';
   }else{
     const catalog=sdk.resources.list();check(catalog.entries.length===1,'restored image manifest');
     await sdk.resources.publish(catalog.entries[0].token,image);row.resources=sdk.resources.list();
     await sdk.selectBrush('round-handdrawn',4);check(sdk.readBrushConfig()?.family===1,'default test brush');
     await sdk.zoomAt(0,0,c.camera.scale);const zoomed=state().camera;await sdk.panBy(zoomed.x-c.camera.x,zoomed.y-c.camera.y);await stable();
     row.states.warm=state();check(Math.abs(state().camera.scale-c.camera.scale)<1e-5&&Math.abs(state().camera.x-c.camera.x)<1e-4,'warm camera');
     const spec=rowSpec(c.rowId??c.id);if(spec.edit==='append')await sdk.selectTool('ink');else{await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});}
     await stable();row.states.before=state();
     if(c.diagnostic){check(traceControl(2,401)===0,'trace initialize');check(traceControl(1)===0,'trace enable');traceControl(3);row.clock.push({js:performance.now(),native:JSON.parse(window.enModule.UTF8ToString(window.enModule._axiom_g5_trace_bytes()))});traceTimer=setInterval(drain,100);}
     observed={};observer.begin(observed);active=true;
     const initial=state().camera;row.editStarted=performance.now();
     for(const p of confirmedTrace(spec.edit))pointer(p.phase,p.x*initial.scale+initial.x,p.y*initial.scale+initial.y,p.timestampNs);
     row.upReturned=performance.now();
     if(spec.timing==='settled'){observer.end(observed);active=false;await stable();observer.begin(observed);active=true;row.settledBeforeNavigation=true;}
     const nav=navigationEvents(spec.direction),base=spec.timing==='immediate'?row.upReturned:performance.now();row.navigationPlanned=base;row.navigation=[];
     await new Promise(resolve=>setTimeout(resolve,0));
     const dispatchNavigation=e=>{
       const rect=sdk.canvas.getBoundingClientRect(),event=new WheelEvent('wheel',{clientX:rect.left+e.x,clientY:rect.top+e.y,ctrlKey:e.ctrlKey,deltaX:e.deltaX,deltaY:e.deltaY,deltaMode:0,bubbles:true,cancelable:true});
       const started=performance.now();observer.dispatch(sdk.canvas,event);row.navigation.push({...e,actualAnchor:{x:event.clientX-rect.left,y:event.clientY-rect.top},planned:base+e.plannedOffsetMs,started,ended:performance.now()});
     };
     dispatchNavigation(nav[0]);
     const tasks=nav.slice(1).map(e=>new Promise(resolve=>setTimeout(()=>{dispatchNavigation(e);resolve();},Math.max(0,base+e.plannedOffsetMs-performance.now()))));
     await Promise.all(tasks);row.navigationReturned=performance.now();
     await stable();row.settledAt=performance.now();observer.end(observed);active=false;row.observation=observed;
     row.states.after=state();row.expectedCamera=expectedCamera(initial,spec.direction,row.navigation[0].actualAnchor);
     for(const k of ['x','y','scale'])check(Math.abs(row.states.after.camera[k]-row.expectedCamera[k])<.003,'final camera '+k);
     if(c.diagnostic){clearInterval(traceTimer);traceTimer=null;traceControl(0);drain();const m=window.enModule,p=m.stringToNewUTF8('{"command":"cpuPending"}');try{row.cpuFacts=JSON.parse(m.UTF8ToString(m._axiom_js_call(p,0,0)));}finally{m._free(p);}}
     row.proxy={firstDefaultFboContentAt:observed.colorOps.find(v=>v.kind==='content'&&v.started>=row.navigation[0].started)?.started??null,firstMatchingQualifiedSubmitAt:null,qualification:'ordinary GL write is proxy; diagnostic trace and cold pixels qualify separately'};
     const saved=await snapshot('final');row.wireObjects=snapshotObjects(saved.bytes);
     if(c.cold){await capture('final');await sdk.undo();await stable();await snapshot('undo');await sdk.redo();await stable();await snapshot('redo');await capture('redo');}
     row.status='OBSERVED';
   }
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{if(active){observer.end(observed);row.failureObservation=observed;active=false;}}catch{}try{row.failureState=state();}catch{}if(c.diagnostic)try{traceControl(0);drain();}catch{} }
 finally{
   if(traceTimer)clearInterval(traceTimer);
   for(let i=0;i<ops.length;++i)await binary('operation-'+i+'.axsnap',ops[i]);
   if(observer){try{if(active)observer.end(row.observation);observer.restore();}catch(e){row.observerCleanupError=String(e);}}
   if(sdk)try{if(row.status==='FAILED')await stable();await sdk.close();row.closed=true;}catch(e){row.cleanupError=String(e);}
   row.entropy=globalThis.exactFixtureEntropy?.transcript();await save();
 }
 return row;
}

export function createServer(request,active){return http.createServer(async(req,res)=>{try{
 const name=new URL(req.url,'http://local').pathname;let file;
 if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{consumer}from"/driver.mjs";window.run=consumer;</script>');return;}
 if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
 if(name==='/sdk/internal/axiom.mjs'&&active.diagnostic){res.setHeader('Content-Type','text/javascript');res.end("import factory from '/engine/internal/axiom.mjs';export default async o=>{const m=await factory(o);window.enModule=m;return m;};");return;}
 if(name==='/fixture.axsnap')file=active.fixture.path;else if(name==='/image.png')file=request.image;
 else if(name.startsWith('/sdk/'))file=path.join(active.sdk,name.slice(5));else if(name.startsWith('/engine/'))file=path.join(active.sdk,name.slice(8));else if(name.startsWith('/g5/'))file=path.join(process.cwd(),'verification/runners/g5',name.slice(4));
 if(!file)throw Error('unknown route');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch(e){res.statusCode=404;res.end(String(e));}});}

export async function run(request,out){
 await fs.mkdir(out,{recursive:true});const result={id:request.id,status:'STARTED',rows:[],errors:[]},active={};let server,browser,page;
 try{
   server=createServer(request,active);await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
   let fixture=request.fixture;const completed=[];
   for(const c of request.cases){
     if(c.selectTarget){
       const choices=completed.filter(v=>v.id.startsWith(c.selectTarget+'-')&&v.id.endsWith('-immediate')&&!v.id.includes('zoom-in'));
       choices.sort((a,b)=>{
         const value=r=>r.proxy?.firstDefaultFboContentAt===null?Infinity:(r.proxy?.firstDefaultFboContentAt??Infinity)-(r.navigation?.[0]?.started??0);
         return value(b)-value(a)||a.id.localeCompare(b.id);
       });
       if(!choices.length)throw Error('missing frozen target choices');c.rowId=choices[0].id;
       await fs.writeFile(path.join(out,c.id+'-target.json'),JSON.stringify({rule:'worst immediate out/pan per edit by ordinary first defaultFBO content proxy; qualified submit null',selected:c.rowId,choices:choices.map(v=>({id:v.id,proxy:v.proxy,navigation:v.navigation?.[0]}))},null,2)+'\n');
     }
     active.sdk=c.sdk;active.fixture=fixture;active.diagnostic=!!c.diagnostic;
     const dir=path.join(out,c.id);await fs.mkdir(dir);page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});
     await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});
     page.setDefaultTimeout(120000);const chunks=new Map(),logs=[];page.on('console',m=>logs.push({type:m.type(),text:m.text()}));page.on('pageerror',e=>result.errors.push(String(e)));
     await page.exposeFunction('enMetadata',v=>fs.writeFile(path.join(dir,'raw.json'),JSON.stringify(v,null,2)+'\n'));
     await page.exposeFunction('enChunk',async c=>{const next=validateChunk(c,chunks.get(c.name)),b=Buffer.from(c.base64,'base64'),file=path.join(dir,c.name+'.partial'),h=await fs.open(file,c.offset?'r+':'wx');try{await h.write(b,0,b.length,c.offset);}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(dir,c.name));chunks.set(c.name,next);return{nextOffset:next.nextOffset};});
     console.log(JSON.stringify({stage:'case-start',id:c.id}));await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);
     const row=await page.evaluate(v=>window.run(v),{...c,fixture,camera:request.camera});
     completed.push(row);
     result.rows.push({id:c.id,status:row.status,error:row.error,proxy:row.proxy,upReturned:row.upReturned,navigation:row.navigation,settledAt:row.settledAt});
     await fs.writeFile(path.join(dir,'console.json'),JSON.stringify(logs,null,2)+'\n');await page.close();page=null;
     if(row.status!=='OBSERVED'||row.cleanupError)throw Error('case failed '+c.id+' '+row.error);
     if(c.mode==='author'){fixture={path:path.join(dir,'fixture.axsnap'),metadata:row.fixture.metadata,documentId:request.fixture.documentId};await fs.writeFile(path.join(out,'fixture.json'),JSON.stringify({...fixture,resources:row.resources,fixtureObjects:row.fixtureObjects},null,2)+'\n');}
     console.log(JSON.stringify({stage:'case-complete',id:c.id}));
   }
   if(result.errors.length)throw Error('page errors');result.status='OBSERVED';
 }catch(e){result.status='FAILED';result.error=String(e.stack??e);}
 finally{if(page)await page.close();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status}));}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){if((await run(JSON.parse(await fs.readFile(process.argv[2],'utf8')),path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
