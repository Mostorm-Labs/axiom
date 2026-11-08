// Five declared browser families. Public SDK only; shared fixture, isolated
// cases, original product assertions and failure-preserving diagnostics.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {runWithDiagnostics,summarizeDurations,summarizePhase} from './transient_reference.mjs';

const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..');
const HERE=path.dirname(fileURLToPath(import.meta.url));
const hash=b=>createHash('sha256').update(b).digest('hex');
export const CASES=['plain-drag','immediate-reerase','masked-drag'];
export function validateSnapshotInput(actual,expected){
  if(actual.sha256!==expected.sha256||JSON.stringify(actual.metadata)!==JSON.stringify(expected.metadata)||actual.name!==expected.name)
    throw Error('snapshot identity mismatch');
}
export function validateRestoredCase(init,s){
  if(init.state.documentId!==s.state.documentId||init.state.objects!==16||init.state.activeContacts!==0||JSON.stringify(init.state.camera)!==JSON.stringify(s.state.camera)||
     JSON.stringify(init.config)!==JSON.stringify(s.expected_config)||init.binding!==s.metadata.resourceBinding)throw Error('restored state/config/binding mismatch');
  for(const v of [init.state.generation,s.state.generation,s.metadata.documentRevision])if(!/^(0|[1-9][0-9]*)$/.test(v)||BigInt(v)>18446744073709551615n)throw Error('invalid clock');
  return {live_initial_semantic_generation:init.state.generation,source_capture_semantic_generation:s.state.generation,external_checkpoint_revision:s.metadata.documentRevision};
}
export function validateRestoreEnvelope(s){
  const c=s.restore_custody;
  if(!c||c.task!=='G5-UR1a-attribution-r2'||c.issuer!=='verification-fixture-data-owner'||c.documentId!==s.state.documentId||c.binding!==s.metadata.resourceBinding||
     !/^[1-9][0-9]*$/.test(c.binding)||BigInt(c.binding)>18446744073709551615n||c.snapshot_sha256[s.name]!==s.sha256||
     JSON.stringify({...s.metadata,resourceBinding:s.source_capture_metadata.resourceBinding})!==JSON.stringify(s.source_capture_metadata))throw Error('invalid caller restore envelope');
}
export function consumerModuleSource(){return 'window.ur1aDiagnosticRun='+runWithDiagnostics.toString()+';await ('+attributionConsumer.toString()+')();';}
export function phaseMetrics(p){
  if(!Number.isFinite(p.inputEnded)||!Number.isFinite(p.ended))return {id:p.id,status:p.status,metrics:'unavailable incomplete phase'};
  return {id:p.id,status:p.status,...summarizePhase(p),schedule_lateness_ms:summarizeDurations(p.schedule.map(x=>Math.max(0,x.actual-x.planned))),
    sdk_raf_work_ms:summarizeDurations(p.rafCallbacks.filter(x=>x.source==='sdk').map(x=>x.ended-x.started)),
    up_handler_ms:p.up_handler_ms??null,next_down_handler_ms:p.next_down_handler_ms??null,
    next_down_actual_after_up_ms:p.next_down_actual_after_up_ms??null,
    gpu_coverage_path:'unavailable from normal SDK',cpu_raster_work:'unavailable from normal SDK'};
}

export async function attributionConsumer(){
  const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
  const {observeExistingGpuCalls}=await import('/g5/transient_attribution_observer.mjs');
  let sdk,clock=31004000000n,coldBytes={},deadline=Infinity,restoredClock=null;
  const captured={};
  const phases=[],frames=[],errors=[],observer=installNavigationObserver(()=>sdk?.canvas),gpu=observeExistingGpuCalls();
  const copy=v=>JSON.parse(JSON.stringify(v));
  const state=()=>sdk?.readState()??null;
  const emit=v=>console.info('UR1ATTR_STAGE '+JSON.stringify(v));
  const errorDTO=e=>({at:performance.now(),name:e.name??null,code:e.code??null,message:String(e.message??e),stack:String(e.stack??''),...e});
  const check=(ok,label,actual=null,expected=null)=>{if(!ok)throw Object.assign(Error('PRODUCT_CHECK: '+label),{predicate:{actual,expected}});};
  const raf=()=>new Promise(resolve=>{resolve.g5SamplerRaf=true;requestAnimationFrame(resolve);});
  const same=(a,b)=>a.generation===b.generation&&a.lastCommit.operationId===b.lastCommit.operationId&&a.lastCommit.ordinal===b.lastCommit.ordinal;
  const snapshotState=()=>({state:state(),selection:sdk?.readSelection()??null,activity:observer.activity()});
  const pointer=(phase,x,y,time)=>{
    clock=time===undefined?clock+4000000n:BigInt(time);const r=sdk.canvas.getBoundingClientRect();
    const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+x,clientY:r.top+y,button:phase==='move'?-1:0,
      buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
    Object.defineProperty(e,'timeStamp',{value:Number(clock)/1e6});observer.dispatch(sdk.canvas,e);
  };
  const until=async t=>{while(performance.now()<t){await new Promise(r=>setTimeout(r,Math.max(0,t-performance.now())));}if(performance.now()>deadline)throw Error('PRODUCT_CHECK: case completion deadline');};
  const settle=async()=>{
    const limit=Math.min(deadline,performance.now()+30000);
    for(;;){await raf();const s=state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;
      if(performance.now()>limit)throw Error('PRODUCT_CHECK: public completion timeout');}
  };
  const phase=async(id,action,complete=true)=>{
    const p={id,started:performance.now(),before:snapshotState(),schedule:[],status:'STARTED'};phases.push(p);observer.begin(p);gpu.begin(p);
    emit({stage:'phase-begin',id,before:p.before});
    try{await action(p);p.inputEnded=performance.now();if(complete)await settle();p.status='COMPLETED';}
    catch(e){p.failure=errorDTO(e);p.status='FAILED';throw e;}
    finally{p.inputEnded??=performance.now();p.after=snapshotState();p.ended=performance.now();gpu.end(p);observer.end(p);
      emit({stage:'phase-end',id,status:p.status,before:p.before,afterUp:p.afterUp??null,afterNextDown:p.afterNextDown??null,after:p.after,failure:p.failure??null,errors:copy(errors)});}
    return p;
  };
  const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b))).map(x=>x.toString(16).padStart(2,'0')).join('');
  const init=async({variant='A',restore=null,manual=false}={})=>{
    const {createRuntime}=await import('/'+variant+'/axiom.mjs');
    let binding;
    if(restore){const bytes=new Uint8Array(await (await fetch('/fixtures/'+restore.name+'.axsnap')).arrayBuffer());
      check(await digest(bytes)===restore.sha256,'snapshot transfer digest',await digest(bytes),restore.sha256);binding={bytes,metadata:restore.metadata};}
    sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'99999999999999999999999999999999',permission:'localSingleUser',builtinFonts:false,
      ...(binding?{restore:binding}:{}),onFrame:f=>frames.push({at:performance.now(),...f}),onError:e=>errors.push(errorDTO(e))});
    if(restore){check(sdk.resources.list().binding===restore.metadata.resourceBinding,'restored resource binding',sdk.resources.list().binding,restore.metadata.resourceBinding);
      restoredClock={live_initial_semantic_generation:state().generation,source_capture_semantic_generation:restore.state.generation,external_checkpoint_revision:restore.metadata.documentRevision};}
    if(!manual){sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;}
    await settle();await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('select');
    const gl=sdk.canvas.getContext('webgl2'),ext=gl.getExtension('WEBGL_debug_renderer_info');
    return {state:state(),selection:sdk.readSelection(),config:sdk.readBrushConfig(),restore:restore?{name:restore.name,sha256:restore.sha256,metadata:restore.metadata}:null,
      binding:sdk.resources.list().binding,build:sdk.readBuildInfo(),dpr:devicePixelRatio,renderer:gl.getParameter(ext?ext.UNMASKED_RENDERER_WEBGL:gl.RENDERER),userAgent:navigator.userAgent};
  };
  const capture=async name=>{
    await settle();const s=state(),frontier=[8,1,16,1,24,1];
    const documentRevision=restoredClock?(BigInt(restoredClock.external_checkpoint_revision)+BigInt(s.generation)-BigInt(restoredClock.live_initial_semantic_generation)).toString():s.generation;
    const snapshot=await sdk.captureSnapshot({generation:s.generation,documentRevision,recoveryFrontier:frontier});
    coldBytes[name]=snapshot.bytes;
    return captured[name]={name,sha256:await digest(snapshot.bytes),byteLength:snapshot.bytes.length,state:s,
      checkpoint_mapping:{policy:restoredClock?'external base plus actual local semantic delta':'seed caller assigns source generation as external revision',...restoredClock,live_capture_semantic_generation:s.generation,chosen_external_revision:documentRevision},
      metadata:{documentRevision,recoveryFrontier:frontier,checkpointDigest:snapshot.checkpointDigest,resourceBinding:sdk.resources.list().binding},
      throughCommit:snapshot.throughCommit};
  };
  const erasePoint=(n,i)=>[390+70*Math.sin(i*.13+n*.7),280+55*Math.cos(i*.17+n*.4)];
  const eraseMoves=async(n,p)=>{
    const base=performance.now();for(let i=1;i<=32;++i){const planned=base+i*16.666667;await until(planned);const actual=performance.now();pointer('move',...erasePoint(n,i));p.schedule.push({event:'move',i,planned,actual});
      check(same(p.before.state,state()),'live erase changed document',state(),p.before.state);}
  };
  const seed=async fixture=>{
    check(fixture.samples.length===7200,'7200 literal samples',fixture.samples.length,7200);await sdk.selectTool('ink');
    for(let i=0;i<fixture.samples.length;++i){const s=fixture.samples[i];pointer(s[4],s[0]/1000,s[1]/1000,s[2]);if(i%16===0)await raf();}
    await settle();await sdk.selectTool('select');const first=fixture.samples[0].slice(0,2).map(x=>x/1000);pointer('down',...first);pointer('up',...first);await settle();
    check(sdk.readSelection().count===1,'head selects original',sdk.readSelection().count,1);
    for(let i=0;i<15;++i){await sdk.duplicateSelection({offset:[1,1]});await settle();emit({stage:'duplicate',count:i+1});}
    check(state().objects===16,'16 fixture objects',state().objects,16);
    const S0=await capture('S0');emit({stage:'snapshot-S0',snapshot:S0});
    await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');
    for(let n=0;n<2;++n){const before=state();pointer('down',...erasePoint(n,0));check(state().activeContacts===1,'seed eraser Down',state().activeContacts,1);
      await phase('seed-erase-'+n,async p=>{await eraseMoves(n,p);pointer('up',...erasePoint(n,32));p.afterUp=snapshotState();});
      check(BigInt(state().generation)===BigInt(before.generation)+1n,'seed erase commit',state().generation,(BigInt(before.generation)+1n).toString());}
    const S2=await capture('S2');emit({stage:'snapshot-S2',snapshot:S2});return {S0,S2,head:first.map(x=>x+15)};
  };
  const drag=async(id,head)=>{
    await sdk.selectTool('select');
    const p=await phase(id,async p=>{pointer('down',...head);p.afterDown=snapshotState();check(state().activeContacts===1,'drag Down rejected',state().activeContacts,1);
      const base=performance.now();for(let i=1;i<=48;++i){const planned=base+i*16.666667;await until(planned);const actual=performance.now();pointer('move',head[0]+i,head[1]+20*i/48);p.schedule.push({event:'move',i,planned,actual});
        check(same(p.before.state,state()),'drag Move changed document',state(),p.before.state);}
      const t=performance.now();pointer('up',head[0]+48,head[1]+20);p.up_handler_ms=performance.now()-t;p.afterUp=snapshotState();});
    check(BigInt(p.after.state.generation)===BigInt(p.before.state.generation)+1n,'drag did not commit exactly once',p.after.state.generation,(BigInt(p.before.state.generation)+1n).toString());
  };
  const erases=async()=>{
    await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');const before=state();
    pointer('down',...erasePoint(0,0));check(state().activeContacts===1,'initial eraser Down',state().activeContacts,1);
    for(let n=0;n<2;++n)await phase('erase-'+n,async p=>{
      await eraseMoves(n,p);let t=performance.now();pointer('up',...erasePoint(n,32));p.up_handler_ms=performance.now()-t;p.afterUp=snapshotState();
      const planned=performance.now()+4;await until(planned);const actual=performance.now();t=actual;pointer('down',...erasePoint(n+1,0));p.next_down_handler_ms=performance.now()-t;
      p.next_down_actual_after_up_ms=actual-(planned-4);p.schedule.push({event:'nextDown',planned,actual});p.afterNextDown=snapshotState();
      check(p.afterNextDown.state.activeContacts===1,'immediate next Down rejected',p.afterNextDown.state.activeContacts,1);
    },false);
    pointer('cancel',...erasePoint(2,0));await settle();check(BigInt(state().generation)===BigInt(before.generation)+2n,'two erases did not commit exactly twice',state().generation,(BigInt(before.generation)+2n).toString());
  };
  const pixels=()=>{const gl=sdk.canvas.getContext('webgl2'),w=sdk.canvas.width,h=sdk.canvas.height,raw=new Uint8Array(w*h*4),top=new Uint8Array(raw.length);
    gl.readPixels(0,0,w,h,gl.RGBA,gl.UNSIGNED_BYTE,raw);for(let y=0;y<h;++y)top.set(raw.subarray((h-1-y)*w*4,(h-y)*w*4),y*w*4);return {bytes:top,width:w,height:h};};
  const deselect=async()=>{await sdk.selectTool('select');pointer('down',8,8);pointer('up',8,8);await settle();check(sdk.readSelection().count===0,'cold deselection',sdk.readSelection().count,0);};
  const cold=async undoRedo=>{
    await deselect();const before=state(),original=pixels(),originalHash=await digest(original.bytes);let history=null;
    if(undoRedo){await sdk.undo();await settle();const undo=state(),undoHash=await digest(pixels().bytes);await sdk.redo();await settle();await deselect();const redo=state(),redoHash=await digest(pixels().bytes);
      history={before,undo,redo,before_sha256:originalHash,undo_sha256:undoHash,redo_sha256:redoHash};
      check(BigInt(undo.generation)===BigInt(before.generation)+1n,'Undo commit',undo.generation,(BigInt(before.generation)+1n).toString());
      check(BigInt(redo.generation)===BigInt(undo.generation)+1n,'Redo commit',redo.generation,(BigInt(undo.generation)+1n).toString());
      check(redoHash===originalHash,'Undo/Redo pixel roundtrip',redoHash,originalHash);check(undoHash!==originalHash,'Undo has no visible change',undoHash,originalHash);}
    const finalPixels=pixels();coldBytes.rgba=finalPixels.bytes;const snapshot=await capture('final');return {snapshot,width:finalPixels.width,height:finalPixels.height,camera:state().camera,
      rgba_bytes:finalPixels.bytes.length,rgba_sha256:await digest(finalPixels.bytes),history};
  };
  const dump=()=>({phases,frames,errors,state:snapshotState(),snapshots:captured,gpu_coverage_path:'unavailable',cpu_raster_work:'unavailable'});
  window.ur1attr={init,seed,capture,dump,async run(id,head){deadline=performance.now()+40000;
    const result=await window.ur1aDiagnosticRun(async()=>{if(id==='immediate-reerase')await erases();else await drag(id,head);
      check(state().activeContacts===0,'contacts leaked',state().activeContacts,0);await Promise.resolve();
      check(errors.length===0,'SDK errors',copy(errors),[]);return {state:state()};},dump);
    await Promise.resolve();result.diagnostics=dump();emit({stage:'case-outcome',id,status:result.status,failure:result.failure,state:snapshotState(),errors:copy(errors)});return result;
  },async cold(undoRedo){deadline=performance.now()+30000;return cold(undoRedo);},
  async coldFailure(){deadline=performance.now()+10000;if(state().activeContacts!==0)return {status:'MISSING',reason:'active contact; no invented canonical endpoint'};try{await settle();return {status:'CAPTURED',snapshot:await capture('failure')};}catch(e){return {status:'MISSING',reason:String(e)};}},
  pull(kind,offset,count){const b=coldBytes[kind].subarray(offset,offset+count);let s='';for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);},
  async control(name){if(name==='undo'||name==='redo')return sdk[name]();if(name==='partialEraser')await sdk.setEraser({mode:2,diameter:32});return sdk.selectTool(name);},
  async close(){deadline=performance.now()+10000;const before=snapshotState();if(state().activeContacts)pointer('cancel',0,0);await sdk.close();observer.restore();gpu.restore();return {before,closed:!document.querySelector('#mount canvas'),errors};}};
}

export async function createServer({a,b,fixtures}){
  const server=http.createServer(async(req,res)=>{try{
    const u=new URL(req.url,'http://127.0.0.1');let file;
    if(u.pathname==='/consumer.mjs'){res.setHeader('Content-Type','text/javascript');res.end(consumerModuleSource());return;}
    if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module" src="/consumer.mjs"></script>');return;}
    const dirs={'A':a,'B':b,'fixtures':fixtures,'g5':HERE},parts=u.pathname.split('/').filter(Boolean),dir=dirs[parts.shift()];
    if(u.pathname==='/'||u.pathname==='/compare.html')file=path.join(HERE,'transient_attribution.html');
    else{if(!dir)throw Error('route');file=path.resolve(dir,...parts);if(!file.startsWith(path.resolve(dir)+path.sep))throw Error('path');}
    res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':file.endsWith('.html')?'text/html':file.endsWith('.json')?'application/json':'application/octet-stream');res.end(await fs.readFile(file));
  }catch(e){res.statusCode=404;res.end(String(e));}});return server;
}
async function filesDigest(root){const out={};for(const name of (await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())out[name]={sha256:hash(await fs.readFile(p)),bytes:(await fs.stat(p)).size};}return out;}
async function worker({variant,sdk,out,fixtureDir,oracle,seed}){
  const {chromium}=await import('../../node_modules/playwright/index.mjs');
  const {compareSnapshotPixels}=await import('./ur0/pixel_oracle.mjs');
  await fs.mkdir(out,{recursive:true});const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,'transient_attribution_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const expected=JSON.parse(await fs.readFile(path.join(ROOT,'.aegis/results/G5-UR1a-transient-reference',variant+'-artifacts.json')));
  const result={schema:'g5-ur1a-attribution-worker-v1',variant,status:'STARTED',source_before:source(),artifacts_before:await filesDigest(sdk),cases:[],page_errors:[]};
  let browser,server,current=null;
  const transfer=async(page,kind,length,file)=>{const f=await fs.open(file,'w');try{for(let off=0;off<length;off+=65536)await f.writeFile(Buffer.from(await page.evaluate(({kind,off})=>ur1attr.pull(kind,off,65536),{kind,off}),'base64'));}finally{await f.close();}};
  const fresh=async()=>{const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
    page.on('pageerror',e=>{result.page_errors.push(String(e));console.log(JSON.stringify({stage:'page-error',error:String(e)}));});
    page.on('console',m=>{if(m.text().startsWith('UR1ATTR_STAGE '))console.log(m.text().slice(14));});
    page.on('crash',()=>{result.crash=true;});
    await page.addInitScript(()=>{let word=0x12345678;crypto.getRandomValues=a=>{const b=new Uint8Array(a.buffer,a.byteOffset,a.byteLength);for(let i=0;i<b.length;++i){word^=word<<13;word^=word>>>17;word^=word<<5;b[i]=word&255;}return a;};});
    await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur1attr);return page;};
  try{
    if(JSON.stringify(result.artifacts_before)!==JSON.stringify(expected.files))throw Error('artifact identity mismatch');
    server=await createServer({a:sdk,b:sdk,fixtures:fixtureDir});await new Promise(r=>server.listen(0,'127.0.0.1',r));
    browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
    if(seed){
      current=await fresh();result.init=await current.evaluate(v=>ur1attr.init({variant:v}),variant);await write('init.json',result.init);
      const literal=await fs.readFile(path.join(ROOT,'docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json'));
      if(hash(literal)!=='b604377ff64982570de61bc9d085f1d9c891e89406af715b2aa6711a25572f69')throw Error('fixture literal identity');
      result.seed_attempt=await current.evaluate(f=>ur1aDiagnosticRun(()=>ur1attr.seed(f),()=>ur1attr.dump()),JSON.parse(literal));await write('seed-observed.json',result.seed_attempt);
      result.seed=result.seed_attempt.value??{...result.seed_attempt.diagnostics.snapshots,head:JSON.parse(literal).samples[0].slice(0,2).map(x=>x/1000+15)};
      await fs.mkdir(fixtureDir,{recursive:true});
      for(const name of ['S0','S2']){const s=result.seed[name];if(!s)continue;await transfer(current,name,s.byteLength,path.join(fixtureDir,name+'.axsnap'));if(hash(await fs.readFile(path.join(fixtureDir,name+'.axsnap')))!==s.sha256)throw Error('fixture transfer identity');
        s.native=JSON.parse(execFileSync(oracle,[path.join(fixtureDir,name+'.axsnap')],{encoding:'utf8',timeout:10000,maxBuffer:2*1024*1024}));}
      await fs.writeFile(path.join(fixtureDir,'fixtures.json'),JSON.stringify(result.seed,null,2)+'\n');await write('seed.json',result.seed);
      if(!result.seed.S0)throw Error('fixture snapshot impossible under public contract');
      result.close=await current.evaluate(()=>ur1attr.close());await current.close();current=null;
    }else{
      const fixtures=JSON.parse(await fs.readFile(path.join(fixtureDir,'fixtures.json')));
      for(const id of CASES){const name=id==='masked-drag'?'S2':'S0',s=fixtures[name],row={id,status:'STARTED'};result.cases.push(row);
        if(!s){row.status='MISSING';row.reason='seed S2 unavailable';continue;}
        validateRestoreEnvelope(s);
        if(hash(await fs.readFile(path.join(fixtureDir,name+'.axsnap')))!==s.sha256)throw Error('snapshot identity mismatch');
        current=await fresh();row.init=await current.evaluate(o=>ur1attr.init(o),{variant,restore:s});validateSnapshotInput(row.init.restore,s);
        row.clocks=validateRestoredCase(row.init,s);
        await write(id+'-init.json',row.init);
        row.observed=await current.evaluate(({id,head})=>ur1attr.run(id,head),{id,head:fixtures.head});await write(id+'-observed.json',row.observed);
        row.status=row.observed.status;row.metrics=row.observed.diagnostics.phases.map(phaseMetrics);
        if(row.status==='COMPLETED'){
          try{row.cold=await current.evaluate(undo=>ur1attr.cold(undo),id!=='immediate-reerase');await write(id+'-cold.json',row.cold);
            for(const [kind,length,ext] of [['final',row.cold.snapshot.byteLength,'axsnap'],['rgba',row.cold.rgba_bytes,'rgba']])await transfer(current,kind,length,path.join(out,id+'.'+ext));
            if(hash(await fs.readFile(path.join(out,id+'.axsnap')))!==row.cold.snapshot.sha256||hash(await fs.readFile(path.join(out,id+'.rgba')))!==row.cold.rgba_sha256)throw Error('cold transfer identity');
            row.pixels=await compareSnapshotPixels({oracle,snapshot:path.join(out,id+'.axsnap'),actual:await fs.readFile(path.join(out,id+'.rgba')),
              request:{width:row.cold.width,height:row.cold.height,logical_width:800,logical_height:500,camera:row.cold.camera,hud_bounds:[]},prefix:path.join(out,id+'-reference')});
            if(row.pixels.status!=='QUALIFIED'){row.status='FAILED';row.cold_failure='PRODUCT_CHECK: final canonical pixels differ';}
          }catch(e){row.status='FAILED';row.cold_failure=String(e.stack??e);}
        }else{
          row.failure_cold=await current.evaluate(()=>ur1attr.coldFailure());
          if(row.failure_cold.status==='CAPTURED'){const s=row.failure_cold.snapshot;await transfer(current,'failure',s.byteLength,path.join(out,id+'-failure.axsnap'));}
        }
        row.final_dump=await current.evaluate(()=>ur1attr.dump());await write(id+'-final-dump.json',row.final_dump);
        row.close=await current.evaluate(()=>ur1attr.close());if(!row.close.closed)throw Error('cleanup failed');await current.close();current=null;
        await write(id+'-result.json',row);console.log(JSON.stringify({stage:'case-closed',id,status:row.status,metrics:row.metrics,errors:row.final_dump.errors}));
      }
    }
    result.status=result.cases.some(x=>x.status==='FAILED')||result.seed_attempt?.status==='FAILED'?'PRODUCT_FAILURE_OBSERVED':'OBSERVED';
  }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);
    if(current&&!current.isClosed())try{result.partial=await current.evaluate(()=>ur1attr.dump());await write('partial.json',result.partial);
      // S0 remains exportable even if S2 preparation fails. No fabricated S2.
      if(seed){const s0=result.partial;result.seed_failure_dump=s0;}}
    catch(x){result.partial_export_error=String(x);}}
  finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.source_after=source();result.artifacts_after=await filesDigest(sdk);
    result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;
    result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(!result.source_stable||!result.artifacts_stable||result.crash){result.status='HARD_FAILURE';result.identity_or_crash_failure=true;}await write('worker-result.json',result);}
  console.log(JSON.stringify({stage:'worker-closed',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}

async function main(){const args=process.argv.slice(2);if(args[0]==='--serve'){
  const [_,a,b,fixtures,port]=args,server=await createServer({a:path.resolve(a),b:path.resolve(b),fixtures:path.resolve(fixtures)});server.listen(Number(port),'127.0.0.1',()=>console.log('Serving matched comparison at http://127.0.0.1:'+port+'/compare.html'));return;}
  if(args.length!==6)throw Error('worker variant sdk out fixtures oracle seed|measured');
  await worker({variant:args[0],sdk:path.resolve(args[1]),out:path.resolve(args[2]),fixtureDir:path.resolve(args[3]),oracle:path.resolve(args[4]),seed:args[5]==='seed'});
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))await main();
