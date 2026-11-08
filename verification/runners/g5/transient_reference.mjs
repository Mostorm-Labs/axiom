// Four explicitly budgeted local workers, normal public SDK only. No UR0 trace
// qualification, private factory shim, readback or oracle inside timed phases.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';

// Preserve the original product assertion and already completed observations.
// This wrapper does not reinterpret failure as a successful product outcome.
export async function runWithDiagnostics(action,dump){
  let value,failure=null;
  try{value=await action();}catch(e){failure={message:String(e.message??e),stack:String(e.stack??e),predicate:e.predicate??null};}
  return {status:failure?'FAILED':'COMPLETED',failure,value,diagnostics:dump()};
}

export function summarizeDurations(values){
  if(values.some(x=>!Number.isFinite(x)||x<0))throw Error('invalid duration');
  const a=[...values].sort((a,b)=>a-b);
  const q=p=>a.length?a[Math.max(0,Math.ceil(a.length*p)-1)]:null;
  return {count:a.length,p50:q(.5),p95:q(.95),max:q(1)};
}
export function summarizePhase(p){
  if(![p.started,p.inputEnded,p.ended].every(Number.isFinite)||p.started>p.inputEnded||p.inputEnded>p.ended)throw Error('invalid phase interval');
  const writes=p.colorOps.filter(x=>x.kind==='content'&&x.ended>=p.started&&x.ended<=p.inputEnded).map(x=>x.ended).sort((a,b)=>a-b);
  const marks=[p.started,...writes,p.inputEnded];
  return {handler_ms:summarizeDurations(p.inputEvents.map(e=>e.ended-e.started)),content_writes:writes.length,
    worst_content_submission_gap_ms:writes.length?Math.max(...marks.slice(1).map((t,i)=>t-marks[i])):null,
    canonical_wait_ms:p.ended-p.inputEnded,wall_ms:p.ended-p.started,
    scope:'DOM dispatch / actual default-framebuffer content writes; not physical input-to-photon'};
}

// Serialized into the external verification page. Geometry is exclusively the
// pinned literal input followed by the shared Runtime; this page never draws.
async function browserConsumer(){
  const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
  let sdk,contentNs=31004000000n,coldBytes=null;
  const errors=[],frames=[],phases=[];
  const observer=installNavigationObserver(()=>sdk?.canvas);
  const raf=()=>new Promise(resolve=>{resolve.g5SamplerRaf=true;requestAnimationFrame(resolve);});
  const check=(ok,label,predicate=null)=>{if(!ok)throw Object.assign(Error('PRODUCT_CHECK: '+label),{predicate});};
  const state=()=>sdk.readState();
  const sameCommit=(a,b)=>a.generation===b.generation&&a.lastCommit.operationId===b.lastCommit.operationId&&a.lastCommit.ordinal===b.lastCommit.ordinal;
  const pointer=(phase,x,y,time)=>{
    contentNs=time===undefined?contentNs+4000000n:BigInt(time);
    const r=sdk.canvas.getBoundingClientRect();
    const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+x,clientY:r.top+y,
      button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
    Object.defineProperty(e,'timeStamp',{value:Number(contentNs)/1e6});observer.dispatch(sdk.canvas,e);
  };
  const settled=async()=>{
    const deadline=performance.now()+30000;
    for(;;){await raf();const s=state();
      if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;
      if(performance.now()>deadline)throw Error('public completion timeout');
    }
  };
  const phase=async(id,action,complete=true)=>{
    const p={id,started:performance.now(),before:state()};phases.push(p);observer.begin(p);
    try{await action(p);p.inputEnded=performance.now();if(complete)await settled();p.after=state();p.ended=performance.now();}
    finally{observer.end(p);}
    return p;
  };
  const init=async()=>{
    const {createRuntime}=await import('/axiom.mjs');
    sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'99999999999999999999999999999999',permission:'localSingleUser',builtinFonts:false,
      onFrame:f=>frames.push({at:performance.now(),...f}),onError:e=>errors.push({at:performance.now(),code:e.code??null,message:String(e.message??e),stack:String(e.stack??''),...e})});
    sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
    await settled();
    const gl=sdk.canvas.getContext('webgl2'),ext=gl.getExtension('WEBGL_debug_renderer_info');
    return {build:sdk.readBuildInfo(),state:state(),dpr:devicePixelRatio,
      renderer:gl.getParameter(ext?ext.UNMASKED_RENDERER_WEBGL:gl.RENDERER),userAgent:navigator.userAgent};
  };
  const setup=async fixture=>{
    await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
    for(let i=0;i<fixture.samples.length;++i){const s=fixture.samples[i];pointer(s[4],s[0]/1000,s[1]/1000,s[2]);if(i%16===0)await raf();}
    await settled();await sdk.selectTool('select');
    const first=fixture.samples[0].slice(0,2).map(x=>x/1000);pointer('down',...first);pointer('up',...first);await settled();
    check(sdk.readSelection().count===1,'original head selection');
    for(let i=0;i<15;++i){await sdk.duplicateSelection({offset:[1,1]});await settled();console.info('UR1A_STAGE '+JSON.stringify({stage:'duplicate',copy:i+1}));}
    check(state().objects===16,'16 copies');
    return {state:state(),config:sdk.readBrushConfig(),head:first.map(x=>x+15)};
  };
  const drag=async(id,start)=>{
    await sdk.selectTool('select');
    return phase(id,async p=>{
      pointer('down',...start);const before=state();
      for(let i=1;i<=48;++i){pointer('move',start[0]+48*i/48,start[1]+20*i/48);await raf();check(sameCommit(before,state()),id+' Move changed document');}
      p.afterMoves=state();const t=performance.now();pointer('up',start[0]+48,start[1]+20);p.up_handler_ms=performance.now()-t;
    });
  };
  const erasePoint=(n,i)=>[390+70*Math.sin(i*.13+n*.7),280+55*Math.cos(i*.17+n*.4)];
  const erases=async()=>{
    await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');
    const before=state();pointer('down',...erasePoint(0,0));
    for(let n=0;n<2;++n){
      await phase('erase-'+n,async p=>{
        for(let i=1;i<=32;++i){pointer('move',...erasePoint(n,i));await raf();check(sameCommit(p.before,state()),'live erase changed document');}
        let t=performance.now();pointer('up',...erasePoint(n,32));p.up_handler_ms=performance.now()-t;p.afterUp=state();
        t=performance.now();pointer('down',...erasePoint(n+1,0));p.next_down_handler_ms=performance.now()-t;p.afterNextDown=state();
        check(p.afterNextDown.activeContacts===1,'immediate next Down rejected',{actual:p.afterNextDown.activeContacts,expected:1});
      },false);
    }
    await phase('live-erase-cancel',async p=>{
      for(let i=1;i<=24;++i){pointer('move',...erasePoint(2,i));await raf();check(sameCommit(p.before,state()),'live mask changed document');}
      pointer('cancel',...erasePoint(2,24));check(sameCommit(p.before,state()),'cancel committed');
    });
    check(BigInt(state().generation)===BigInt(before.generation)+2n,'two erases did not commit exactly twice');
  };
  const deselect=async()=>{await sdk.selectTool('select');pointer('down',8,8);pointer('up',8,8);await settled();check(sdk.readSelection().count===0,'cold selection not empty');};
  const pixels=async()=>{
    const gl=sdk.canvas.getContext('webgl2'),w=sdk.canvas.width,h=sdk.canvas.height;
    const raw=new Uint8Array(w*h*4),top=new Uint8Array(raw.length);gl.readPixels(0,0,w,h,gl.RGBA,gl.UNSIGNED_BYTE,raw);
    for(let y=0;y<h;++y)top.set(raw.subarray((h-1-y)*w*4,(h-y)*w*4),y*w*4);
    return {bytes:top,width:w,height:h};
  };
  const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b))).map(x=>x.toString(16).padStart(2,'0')).join('');
  const cold=async()=>{
    await deselect();const before=state(),beforePixels=await pixels();
    await sdk.undo();await settled();const undo=state();check(BigInt(undo.generation)===BigInt(before.generation)+1n,'Undo commit');
    const undoPixels=await pixels();
    await sdk.redo();await settled();await deselect();const redo=state(),afterPixels=await pixels();
    const beforeHash=await digest(beforePixels.bytes),afterHash=await digest(afterPixels.bytes);
    check(beforeHash===afterHash,'Undo/Redo pixel roundtrip');
    check(beforeHash!==await digest(undoPixels.bytes),'Undo had no visible effect');
    const generation=redo.generation;
    const snapshot=await sdk.captureSnapshot({generation,documentRevision:generation,recoveryFrontier:[8,1,16,1,24,1]});
    coldBytes={snapshot:snapshot.bytes,rgba:afterPixels.bytes};
    return {before,undo,redo,width:afterPixels.width,height:afterPixels.height,
      snapshot_bytes:snapshot.bytes.length,snapshot_sha256:await digest(snapshot.bytes),rgba_bytes:afterPixels.bytes.length,rgba_sha256:afterHash,
      camera:redo.camera,undo_redo_pixels_equal:true};
  };
  const dump=()=>({phases,frames,errors,state:sdk?state():null});
  window.ur1a={init,setup,dump,async run(head){return window.ur1aDiagnosticRun(async()=>{
    const plain=await drag('plain-drag',head);check(BigInt(plain.after.generation)===BigInt(plain.before.generation)+1n,'plain drag commit');
    await erases();const moved=[head[0]+48,head[1]+20];
    const masked=await drag('masked-drag',moved);check(BigInt(masked.after.generation)===BigInt(masked.before.generation)+1n,'masked drag commit');
    await sdk.selectTool('select');const before=state();pointer('down',moved[0]+48,moved[1]+20);pointer('move',moved[0]+60,moved[1]+30);await raf();pointer('cancel',moved[0]+60,moved[1]+30);await settled();check(sameCommit(before,state()),'drag cancel committed');
    const final=await cold();check(state().activeContacts===0,'contacts leaked');check(errors.length===0,'SDK errors: '+errors.join(';'));
    return {phases,frames,errors,final,state:state()};
  },dump);},pull(kind,offset,count){const bytes=coldBytes[kind].subarray(offset,offset+count);let s='';for(let i=0;i<bytes.length;i+=8192)s+=String.fromCharCode(...bytes.subarray(i,i+8192));return btoa(s);},async close(){await sdk.close();return{closed:!document.querySelector('#mount canvas'),errors};}};
}

async function main(){
  const {chromium}=await import('../../node_modules/playwright/index.mjs');
  const {compareSnapshotPixels}=await import('./ur0/pixel_oracle.mjs');
  const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..');
  const [sdkArg,outArg,oracle]=process.argv.slice(2),sdkRoot=path.resolve(sdkArg),out=path.resolve(outArg);
  await fs.mkdir(out,{recursive:true});
  const hash=b=>createHash('sha256').update(b).digest('hex');
  const write=(name,v)=>fs.writeFile(path.join(out,name),JSON.stringify(v,null,2)+'\n');
  const artifactHashes=async()=>{const entries={};for(const name of (await fs.readdir(sdkRoot,{recursive:true})).sort()){
    const p=path.join(sdkRoot,name);if((await fs.stat(p)).isFile())entries[name]=hash(await fs.readFile(p));}return entries;};
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(root,'verification/runners/g5/transient_reference_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const fixturePath=path.join(root,'docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json'),literal=await fs.readFile(fixturePath);
  if(hash(literal)!=='b604377ff64982570de61bc9d085f1d9c891e89406af715b2aa6711a25572f69')throw Error('literal fixture identity mismatch');
  const result={schema:'g5-ur1a-local-worker-v1',status:'STARTED',source_before:source(),artifacts_before:await artifactHashes(),fixture_sha256:hash(literal)};
  let browser,server,page;
  try{
    server=http.createServer(async(req,res)=>{try{
      const url=new URL(req.url,'http://127.0.0.1');
      if(url.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">window.ur1aDiagnosticRun='+runWithDiagnostics.toString()+';('+browserConsumer.toString()+')();</script>');return;}
      const dir=url.pathname.startsWith('/g5/')?path.join(root,'verification/runners/g5'):sdkRoot;
      const suffix=url.pathname.startsWith('/g5/')?url.pathname.slice(4):url.pathname.slice(1),file=path.resolve(dir,suffix);
      if(!file.startsWith(dir+path.sep))throw Error('path');
      res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
    }catch(e){res.statusCode=404;res.end(String(e));}});
    await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
    browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
    result.browser={version:browser.version(),headless:true,flags:'Playwright defaults, GPU enabled'};
    page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
    page.on('pageerror',e=>{(result.page_errors??=[]).push(String(e));});
    page.on('console',m=>{if(m.text().startsWith('UR1A_STAGE '))console.log(m.text().slice('UR1A_STAGE '.length));});
    await page.addInitScript(()=>{let word=0x12345678;crypto.getRandomValues=a=>{const b=new Uint8Array(a.buffer,a.byteOffset,a.byteLength);for(let i=0;i<b.length;++i){word^=word<<13;word^=word>>>17;word^=word<<5;b[i]=word&255;}return a;};});
    await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur1a);
    console.log(JSON.stringify({stage:'init'}));result.init=await page.evaluate(()=>ur1a.init());await write('init.json',result.init);
    console.log(JSON.stringify({stage:'setup'}));result.setup=await page.evaluate(f=>ur1a.setup(f),JSON.parse(literal));await write('setup.json',result.setup);
    console.log(JSON.stringify({stage:'measured-actions'}));const completed=await page.evaluate(head=>ur1a.run(head),result.setup.head);await write('observed.json',completed);
    if(completed.status==='FAILED'){result.failure=completed.failure;throw Error(completed.failure.message);}
    const observed=completed.value;
    result.metrics=observed.phases.map(p=>({id:p.id,...summarizePhase(p),up_handler_ms:p.up_handler_ms??null,next_down_handler_ms:p.next_down_handler_ms??null}));
    result.final=observed.final;
    for(const [kind,length] of [['snapshot',observed.final.snapshot_bytes],['rgba',observed.final.rgba_bytes]]){
      const file=await fs.open(path.join(out,'final.'+(kind==='snapshot'?'axsnap':'rgba')),'w');try{
        for(let offset=0;offset<length;offset+=65536){const b=Buffer.from(await page.evaluate(({kind,offset})=>ur1a.pull(kind,offset,65536),{kind,offset}),'base64');await file.writeFile(b);}
      }finally{await file.close();}
    }
    if(hash(await fs.readFile(path.join(out,'final.axsnap')))!==observed.final.snapshot_sha256||hash(await fs.readFile(path.join(out,'final.rgba')))!==observed.final.rgba_sha256)throw Error('cold artifact transfer identity');
    console.log(JSON.stringify({stage:'cold-independent-reference'}));
    result.pixels=await compareSnapshotPixels({oracle,snapshot:path.join(out,'final.axsnap'),actual:await fs.readFile(path.join(out,'final.rgba')),
      request:{width:observed.final.width,height:observed.final.height,logical_width:800,logical_height:500,camera:observed.final.camera,hud_bounds:[]},prefix:path.join(out,'reference')});
    if(result.pixels.status!=='QUALIFIED')throw Error('PRODUCT_CHECK: final canonical pixels differ');
    result.close=await page.evaluate(()=>ur1a.close());result.status='OBSERVED';
  }catch(error){result.status='FAILED';result.error=String(error.stack??error);
    if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.ur1a?.dump());await write('partial.json',result.partial);}catch(e){result.partial_export_error=String(e);}}
  finally{if(browser)await browser.close();if(server)await new Promise(resolve=>server.close(resolve));
    result.source_after=source();result.artifacts_after=await artifactHashes();result.source_stable=JSON.stringify(result.source_before)===JSON.stringify(result.source_after);result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(!result.source_stable||!result.artifacts_stable){result.status='FAILED';result.identity_failure=true;}
    await write('worker-result.json',result);}
  console.log(JSON.stringify({status:result.status,error:result.error??null}));if(result.status!=='OBSERVED')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))await main();
