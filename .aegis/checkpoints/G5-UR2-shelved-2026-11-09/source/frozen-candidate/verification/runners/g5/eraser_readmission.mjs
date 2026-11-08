// Fixed candidate-only public functional checks. Not another UR1a comparison.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {runWithDiagnostics,summarizePhase} from './transient_reference.mjs';
const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..');
const HERE=path.dirname(fileURLToPath(import.meta.url));
const sha=b=>createHash('sha256').update(b).digest('hex');
export const CASES=['reerase-cancel-navigation','pan','pinch','preselected-drag'];
export async function consumer(){
  const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
  let sdk,inputClock=31004000000n,base=null,initialLive=null,coldBytes={},activePhase=null;
  const phases=[],frames=[],errors=[],observer=installNavigationObserver(()=>sdk?.canvas);
  const dto=e=>({code:e.code??null,message:String(e.message??e),stack:String(e.stack??''),...e});
  const state=()=>sdk?.readState()??null;
  const dump=()=>({phases,frames,errors,state:state(),selection:sdk?.readSelection()??null,activity:observer.activity()});
  const check=(ok,label,actual=null,expected=null)=>{if(!ok)throw Object.assign(Error('PRODUCT_CHECK: '+label),{predicate:{actual,expected}});};
  const raf=()=>new Promise(resolve=>{resolve.g5SamplerRaf=true;requestAnimationFrame(resolve);});
  const until=async t=>{while(performance.now()<t)await new Promise(r=>setTimeout(r,Math.max(0,t-performance.now())));};
  const snapshotState=()=>({at:performance.now(),state:state(),selection:sdk.readSelection()});
  const pointer=(phase,x,y)=>{
    inputClock+=4000000n;const r=sdk.canvas.getBoundingClientRect();
    const e=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+x,clientY:r.top+y,button:phase==='move'?-1:0,buttons:['up','cancel'].includes(phase)?0:1,pressure:phase==='up'?0:.5,bubbles:true,cancelable:true});
    Object.defineProperty(e,'timeStamp',{value:Number(inputClock)/1e6});observer.dispatch(sdk.canvas,e);
  };
  const wheel=(pinch=false)=>{const r=sdk.canvas.getBoundingClientRect();observer.dispatch(sdk.canvas,new WheelEvent('wheel',{clientX:r.left+400,clientY:r.top+250,deltaX:pinch?0:8,deltaY:pinch?-1:2,ctrlKey:pinch,bubbles:true,cancelable:true}));};
  const settled=async()=>{const end=performance.now()+30000;for(;;){await raf();const s=state();
    if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;
    if(performance.now()>end)throw Error('PRODUCT_CHECK: canonical completion timeout');}};
  const phase=async(id,fn)=>{
    const p={id,status:'STARTED',started:performance.now(),before:snapshotState(),schedule:[]};phases.push(p);activePhase=p;observer.begin(p);
    console.info('UR2H_STAGE '+JSON.stringify({stage:'phase-begin',id,before:p.before}));
    try{await fn(p);p.inputEnded=performance.now();p.status='INPUT_COMPLETED';}
    catch(e){p.failure={...dto(e),predicate:e.predicate??null};p.status='FAILED';throw e;}
    finally{p.inputEnded??=performance.now();p.after=snapshotState();p.ended=performance.now();observer.end(p);activePhase=null;
      console.info('UR2H_STAGE '+JSON.stringify({stage:'phase-end',id,status:p.status,after:p.after,failure:p.failure??null,errors}));}
    return p;
  };
  const moves=async(p,count,position)=>{const start=performance.now();const generation=state().generation;
    for(let i=1;i<=count;++i){const planned=start+i*16.666667;await until(planned);const actual=performance.now();pointer('move',...position(i));p.schedule.push({event:'move',i,planned,actual});check(state().generation===generation,'Move mutated document',state().generation,generation);}};
  const point=(n,i)=>[n===0?160:n===1?230:560,140+260*i/32];
  const nextDown=async(p,n)=>{const planned=p.afterUp.at+4;await until(planned);const actual=performance.now();pointer('down',...point(n,0));p.nextDown={planned,actual,after:snapshotState()};
    check(state().activeContacts===1,'next Down rejected',state().activeContacts,1);};
  const erase=async(n,start=true,again=false)=>{
    if(start){pointer('down',...point(n,0));check(state().activeContacts===1,'initial eraser Down',state().activeContacts,1);}
    return phase('erase-'+n,async p=>{await moves(p,32,i=>point(n,i));const generation=state().generation;pointer('up',...point(n,32));p.afterUp=snapshotState();
      check(BigInt(state().generation)===BigInt(generation)+1n,'non-noop erase Up must commit once',state().generation,(BigInt(generation)+1n).toString());
      check(state().activeContacts===0,'Up leaked contact',state().activeContacts,0);if(again)await nextDown(p,n+1);});
  };
  const navigate=async(p,pinch,firstPlanned)=>{
    const generation=state().generation,before=state().camera;
    for(let i=0;i<20;++i){const planned=firstPlanned+i*16.666667;await until(planned);const actual=performance.now();wheel(pinch);p.schedule.push({event:pinch?'ctrl-wheel':'wheel',i,planned,actual});
      if(i===0){p.firstCameraInput=snapshotState();check(JSON.stringify(state().camera)!==JSON.stringify(before),'camera input not applied',state().camera,before);}
      check(state().generation===generation,'navigation changed document',state().generation,generation);}
  };
  const digest=async bytes=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))).map(x=>x.toString(16).padStart(2,'0')).join('');
  const init=async fixture=>{
    const {createRuntime}=await import('/axiom.mjs');const bytes=new Uint8Array(await(await fetch('/fixtures/'+fixture.name+'.axsnap')).arrayBuffer());
    check(await digest(bytes)===fixture.sha256,'fixture byte identity');base=fixture;
    sdk=await createRuntime({mount:document.getElementById('mount'),documentId:fixture.state.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes,metadata:fixture.metadata},onError:e=>errors.push(dto(e)),onFrame:f=>frames.push({at:performance.now(),...f})});
    sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settled();initialLive=state().generation;
    check(sdk.resources.list().binding===fixture.metadata.resourceBinding,'restored caller binding',sdk.resources.list().binding,fixture.metadata.resourceBinding);
    check(state().objects===16&&state().activeContacts===0,'restored fixture state',state());
    await sdk.selectBrush('round-handdrawn',4);await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');
    return {state:state(),config:sdk.readBrushConfig(),binding:sdk.resources.list().binding,fixture_sha256:fixture.sha256,
      source_generation:fixture.state.generation,checkpoint_revision:fixture.metadata.documentRevision,live_initial_generation:initialLive,build:sdk.readBuildInfo(),dpr:devicePixelRatio};
  };
  const run=async({id,head})=>window.functionalRun(async()=>{
    if(id==='reerase-cancel-navigation'){
      await erase(0,true,true);await erase(1,false,true);const last=await erase(2,false,false);
      await phase('cancel-then-pan',async p=>{const generation=state().generation;p.afterUp=last.afterUp;await nextDown(p,0);pointer('move',160,150);pointer('cancel',160,150);p.afterCancel=snapshotState();check(state().generation===generation,'cancel committed');await navigate(p,false,p.afterCancel.at+4);});
    }else if(id==='pan'||id==='pinch'){
      const p=await erase(0);await phase('post-Up-'+id,async next=>navigate(next,id==='pinch',p.afterUp.at+4));
    }else{
      await sdk.selectTool('select');const generation=state().generation;pointer('down',...head);pointer('up',...head);await settled();
      check(sdk.readSelection().count===1,'cold point selection',sdk.readSelection().count,1);check(state().generation===generation,'point selection mutated document');
      await phase('preselected-drag',async p=>{pointer('down',...head);check(state().activeContacts===1,'drag Down');await moves(p,48,i=>[head[0]+i,head[1]+20*i/48]);pointer('up',head[0]+48,head[1]+20);p.afterUp=snapshotState();check(BigInt(state().generation)===BigInt(generation)+1n,'drag Up commit',state().generation,(BigInt(generation)+1n).toString());});
    }
    const waiting=performance.now();await settled();const ended=performance.now();await Promise.resolve();
    check(state().activeContacts===0,'contacts leaked',state().activeContacts,0);check(errors.length===0,'SDK errors',errors,[]);
    return {after:state(),canonical_wait:{started:waiting,ended,ms:ended-waiting},scope:'public functional completion; no input-to-photon or GPU leaf timing'};
  },dump);
  const pixels=()=>{const gl=sdk.canvas.getContext('webgl2'),w=sdk.canvas.width,h=sdk.canvas.height,raw=new Uint8Array(w*h*4),top=new Uint8Array(raw.length);gl.readPixels(0,0,w,h,gl.RGBA,gl.UNSIGNED_BYTE,raw);for(let y=0;y<h;++y)top.set(raw.subarray((h-1-y)*w*4,(h-y)*w*4),y*w*4);return {bytes:top,width:w,height:h};};
  const cold=async()=>{
    check(activePhase===null,'cold work entered timed phase');await sdk.selectTool('select');pointer('down',8,8);pointer('up',8,8);await settled();check(sdk.readSelection().count===0,'cold HUD deselection');
    const before=state(),original=pixels(),hash=await digest(original.bytes);await sdk.undo();await settled();const undo=state(),undoHash=await digest(pixels().bytes);
    await sdk.redo();await settled();const redo=state(),image=pixels(),redoHash=await digest(image.bytes);
    check(BigInt(undo.generation)===BigInt(before.generation)+1n&&BigInt(redo.generation)===BigInt(undo.generation)+1n,'Undo/Redo relative generation');
    check(redoHash===hash,'Undo/Redo pixel roundtrip',redoHash,hash);check(undoHash!==hash,'Undo had no visible effect',undoHash,hash);
    const documentRevision=(BigInt(base.metadata.documentRevision)+BigInt(redo.generation)-BigInt(initialLive)).toString();
    const metadata={generation:redo.generation,documentRevision,recoveryFrontier:base.metadata.recoveryFrontier};const snapshot=await sdk.captureSnapshot(metadata);
    coldBytes={snapshot:snapshot.bytes,rgba:image.bytes};return {before,undo,redo,undo_redo_pixels_equal:true,
      checkpoint_request:metadata,checkpoint_mapping:{source:base.metadata.documentRevision,live_initial:initialLive,live_capture:redo.generation,external_revision:documentRevision},
      checkpoint_digest:snapshot.checkpointDigest,snapshot_bytes:snapshot.bytes.length,snapshot_sha256:await digest(snapshot.bytes),
      rgba_bytes:image.bytes.length,rgba_sha256:redoHash,width:image.width,height:image.height,camera:redo.camera};
  };
  window.ur2={init,run,cold,dump,pull(kind,off,count){const b=coldBytes[kind].subarray(off,off+count);let s='';for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);},
    async close(){if(state()?.activeContacts)pointer('cancel',0,0);await sdk?.close();observer.restore();return {closed:!document.querySelector('#mount canvas'),errors};}};
}

async function artifactIdentity(root){const files={};for(const name of(await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())files[name]=sha(await fs.readFile(p));}return files;}
async function main(){
  const [sdkArg,outArg,fixtureName,oracle]=process.argv.slice(2),sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
  const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json'))),fixture=manifest[fixtureName];
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,'eraser_readmission_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const result={schema:'g5-post-up-functional-worker-v1',status:'STARTED',fixture:fixtureName,source_before:source(),artifacts_before:await artifactIdentity(sdk),cases:[],page_errors:[]};
  const write=(name,value)=>fs.writeFile(path.join(out,name),JSON.stringify(value,null,2)+'\n');let server,browser,page;
  const {chromium}=await import('../../node_modules/playwright/index.mjs');const {compareSnapshotPixels}=await import('./ur0/pixel_oracle.mjs');
  try{
    if(sha(await fs.readFile(path.join(fixtures,fixtureName+'.axsnap')))!==fixture.sha256)throw Error('fixture identity mismatch');
    server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://127.0.0.1');if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">window.functionalRun='+runWithDiagnostics.toString()+';await ('+consumer.toString()+')();</script>');return;}
      const [base,suffix]=u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];const file=path.resolve(base,suffix);if(!file.startsWith(base+path.sep))throw Error('path');
      res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});
    await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
    for(const id of CASES){const row={id,status:'STARTED'};result.cases.push(row);
      page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
      page.on('console',m=>{if(m.text().startsWith('UR2H_STAGE '))console.log(m.text().slice(11));});
      await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur2);
      row.init=await page.evaluate(f=>ur2.init(f),fixture);await write(id+'-init.json',row.init);
      row.observed=await page.evaluate(value=>ur2.run(value),{id,head:manifest.head});await write(id+'-observed.json',row.observed);row.status=row.observed.status;
      if(row.status==='COMPLETED')try{
        row.cold=await page.evaluate(()=>ur2.cold());await write(id+'-cold.json',row.cold);
        for(const [kind,length,ext] of [['snapshot',row.cold.snapshot_bytes,'axsnap'],['rgba',row.cold.rgba_bytes,'rgba']]){const f=await fs.open(path.join(out,id+'.'+ext),'w');try{for(let off=0;off<length;off+=65536)await f.writeFile(Buffer.from(await page.evaluate(({kind,off})=>ur2.pull(kind,off,65536),{kind,off}),'base64'));}finally{await f.close();}}
        if(sha(await fs.readFile(path.join(out,id+'.axsnap')))!==row.cold.snapshot_sha256||sha(await fs.readFile(path.join(out,id+'.rgba')))!==row.cold.rgba_sha256)throw Error('cold transfer identity');
        row.pixels=await compareSnapshotPixels({oracle,snapshot:path.join(out,id+'.axsnap'),actual:await fs.readFile(path.join(out,id+'.rgba')),request:{width:row.cold.width,height:row.cold.height,logical_width:800,logical_height:500,camera:row.cold.camera,hud_bounds:[]},prefix:path.join(out,id+'-reference')});
        if(row.pixels.status!=='QUALIFIED'){row.status='FAILED';row.cold_error='PRODUCT_CHECK: native pixel oracle mismatch';}
      }catch(e){row.status='FAILED';row.cold_error=String(e.stack??e);}
      row.final=await page.evaluate(()=>ur2.dump());await write(id+'-final.json',row.final);row.close=await page.evaluate(()=>ur2.close());if(!row.close.closed)throw Error('cleanup failure');await page.close();page=null;
      row.metrics=row.observed.diagnostics.phases.map(p=>({id:p.id,status:p.status,...summarizePhase(p),canonical_wait_ms:null,scope:'input phase only; actual canonical wait is separately recorded'}));
      await write(id+'-result.json',row);console.log(JSON.stringify({stage:'case-closed',id,status:row.status,errors:row.final.errors}));
    }
    result.status=result.cases.every(c=>c.status==='COMPLETED')?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
  }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>ur2.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
  finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.source_after=source();result.artifacts_after=await artifactIdentity(sdk);
    result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(!result.source_stable||!result.artifacts_stable||result.crash){result.status='HARD_FAILURE';result.identity_or_crash=true;}await write('worker-result.json',result);}
  console.log(JSON.stringify({stage:'worker-complete',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))await main();
