// UR1c1 public checks: matched saved fixtures; settled erase/navigation, no immediate UR2 handoff.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {runWithDiagnostics,summarizePhase} from './transient_reference.mjs';
import {runFunctionalErase,boundedDisplayOpportunity} from './prepared_body_index_reverify_pacing.mjs';
import {createReferenceQualifier} from './prepared_body_index_reverify_reference.mjs';
const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..');
const HERE=path.dirname(fileURLToPath(import.meta.url));
const sha=b=>createHash('sha256').update(b).digest('hex');
export const CASES=['S0-canonical','S0-drag','S0-erase-navigation','S2-canonical','S2-drag','S2-erase-navigation'];
import {retainColdStages} from './render_execution.mjs';
export {retainColdStages};
export const historyCase=id=>!id.endsWith('canonical');
export async function restoreCamera(sdk,initial){
  // The public contract takes an absolute scale, not a multiplicative factor.
  await sdk.zoomAt(400,250,initial.scale);
  const camera=sdk.readState().camera;
  await sdk.panBy(initial.x-camera.x,initial.y-camera.y);
}

export function summarizePrepared(records){
  const navigation=new Map(),sequences=[],invalid=[];
  const fields=['lookups','index_queries','builds','identity_hits','fingerprint_bytes','content_comparisons','candidate_checks','navigation_queries','ink_queries','source_footprint_evaluations','source_known_footprint_hits','aliases','bodies','index_reservation_bytes','resident_accounted_bytes'];
  for(const [index,r] of records.entries()){
    if(typeof r.service!=='string'||!r.service||fields.some(f=>!Number.isSafeInteger(r[f])||r[f]<0)){invalid.push(index);continue;}
    if(r.navigation_queries>0)navigation.set(r.service,index);
    if(r.ink_queries>0&&r.builds===0&&r.bodies>0&&(r.identity_hits>0||r.content_comparisons>0)&&navigation.has(r.service))
      sequences.push({service:r.service,navigation_record:navigation.get(r.service),ink_record:index,ink_identity_hits:r.identity_hits,ink_verified_comparisons:r.content_comparisons});
  }
  return {status:!invalid.length&&sequences.length?'OBSERVED':'MISSING',records:records.length,shared_sequences:sequences,invalid_records:invalid,
    scope:'ordered actual navigation/retained-ink calls sharing the existing renderer service; not a performance or all-canonical preparation verdict'};
}
export async function consumer({functionalPacing=false}={}){
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
  const settled=async()=>{const end=performance.now()+30000;for(;;){await raf();const s=state();
    if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return s;
    if(performance.now()>end)throw Error('PRODUCT_CHECK: canonical completion timeout');}};
  const phase=async(id,fn)=>{
    const p={id,status:'STARTED',started:performance.now(),before:snapshotState(),schedule:[]};phases.push(p);activePhase=p;observer.begin(p);
    console.info('UR1C1_STAGE '+JSON.stringify({stage:'phase-begin',id,before:p.before}));
    try{await fn(p);p.inputEnded=performance.now();p.status='INPUT_COMPLETED';}
    catch(e){p.failure={...dto(e),predicate:e.predicate??null};p.status='FAILED';throw e;}
    finally{p.inputEnded??=performance.now();p.after=snapshotState();p.ended=performance.now();observer.end(p);activePhase=null;
      console.info('UR1C1_STAGE '+JSON.stringify({stage:'phase-end',id,status:p.status,after:p.after,failure:p.failure??null,errors}));}
    return p;
  };
  const moves=async(p,count,position)=>{const start=performance.now();const generation=state().generation;
    for(let i=1;i<=count;++i){const planned=start+i*16.666667;await until(planned);const actual=performance.now();pointer('move',...position(i));p.schedule.push({event:'move',i,planned,actual});check(state().generation===generation,'Move mutated document',state().generation,generation);}};
  const digest=async bytes=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))).map(x=>x.toString(16).padStart(2,'0')).join('');
  const init=async fixture=>{
    const {createRuntime}=await import('/axiom.mjs');const bytes=new Uint8Array(await(await fetch('/fixtures/'+fixture.name+'.axsnap')).arrayBuffer());
    check(await digest(bytes)===fixture.sha256,'fixture byte identity');base=fixture;
    sdk=await createRuntime({mount:document.getElementById('mount'),documentId:fixture.state.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes,metadata:fixture.metadata},onError:e=>errors.push(dto(e)),onFrame:f=>frames.push({at:performance.now(),...f})});
    sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;await settled();initialLive=state().generation;
    check(sdk.resources.list().binding===fixture.metadata.resourceBinding,'restored caller binding',sdk.resources.list().binding,fixture.metadata.resourceBinding);
    check(state().objects===16&&state().activeContacts===0,'restored fixture state',state());
    await sdk.selectTool('select');
    const gl=sdk.canvas.getContext('webgl2'),debug=gl.getExtension('WEBGL_debug_renderer_info');
    return {context:{api:'webgl2',version:gl.getParameter(gl.VERSION),renderer:debug?gl.getParameter(debug.UNMASKED_RENDERER_WEBGL):gl.getParameter(gl.RENDERER),limit:'driver information does not prove absence of Skia CPU preparation'},state:state(),config:sdk.readBrushConfig(),binding:sdk.resources.list().binding,fixture_sha256:fixture.sha256,
      source_generation:fixture.state.generation,checkpoint_revision:fixture.metadata.documentRevision,live_initial_generation:initialLive,build:sdk.readBuildInfo(),dpr:devicePixelRatio};
  };
  const run=async({id,head})=>window.functionalRun(async()=>{
    const initial=state().generation;
    if(id.endsWith('drag')){
      pointer('down',...head);pointer('up',...head);await settled();
      check(sdk.readSelection().count===1,'cold point selection',sdk.readSelection().count,1);
      check(state().generation===initial,'selection mutated document');
      await phase('selected-drag-commit',async p=>{
        pointer('down',...head);check(state().activeContacts===1,'selected drag Down');
        await moves(p,48,i=>[head[0]+i,head[1]+20*i/48]);pointer('up',head[0]+48,head[1]+20);p.afterUp=snapshotState();
        check(BigInt(state().generation)===BigInt(initial)+1n,'drag Up exactly once',state().generation,(BigInt(initial)+1n).toString());
      });
      await settled();
      await phase('selected-drag-cancel',async p=>{
        const generation=state().generation;pointer('down',head[0]+48,head[1]+20);check(state().activeContacts===1,'selected cancel Down');
        await moves(p,48,i=>[head[0]+48-i*.5,head[1]+20-i/4]);pointer('cancel',head[0]+24,head[1]+8);
        check(state().generation===generation,'cancel mutated document');
      });
    }else if(id.endsWith('erase-navigation')){
      // Real wheel input keeps navigation feedback active long enough to use
      // existing prepared consumers. Exact completion is awaited before erase.
      const initialCamera={...state().camera};
      await phase('settled-navigation-prepare',async p=>{
        for(let i=0;i<24;++i){inputClock+=4000000n;const rect=sdk.canvas.getBoundingClientRect();
          const event=new WheelEvent('wheel',{clientX:rect.left+400,clientY:rect.top+250,ctrlKey:true,deltaY:-4,bubbles:true,cancelable:true});
          Object.defineProperty(event,'timeStamp',{value:Number(inputClock)/1e6});observer.dispatch(sdk.canvas,event);p.schedule.push({event:'wheel',i,at:performance.now()});await raf();
          check(state().generation===initial,'navigation mutated document');
        }
      });
      await settled();
      // Restore using public SDK camera commands, never private Runtime state.
      await window.restoreCamera(sdk,initialCamera);await settled();
      await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');await settled();
      const point=i=>[390+70*Math.sin(i*.13),280+55*Math.cos(i*.17)];
      await phase('settled-single-partial-erase',async p=>{
        if(functionalPacing){
          p.pacing={mode:'bounded-functional-display-opportunities',budget_ms:15000,opportunities:[]};
          await window.runFunctionalErase({
            down:()=>{pointer('down',...point(0));check(state().activeContacts===1,'settled erase Down');},
            move:(x,y)=>{pointer('move',x,y);check(state().generation===initial,'Move mutated document',state().generation,initial);},
            up:()=>pointer('up',...point(32)),cancel:()=>{if(state().activeContacts)pointer('cancel',...point(0));},
            position:point,schedule:p.schedule,opportunities:p.pacing.opportunities,now:()=>performance.now(),until,display:window.boundedDisplayOpportunity});
        }else{
          pointer('down',...point(0));check(state().activeContacts===1,'settled erase Down');
          await moves(p,32,point);pointer('up',...point(32));
        }
        p.afterUp=snapshotState();
        check(BigInt(state().generation)===BigInt(initial)+1n,'single erase Up exactly once',state().generation,(BigInt(initial)+1n).toString());
      });
      await settled();
      await sdk.panBy(-60,0);await settled();await sdk.panBy(60,0);await settled();
      check(BigInt(state().generation)===BigInt(initial)+1n,'settled navigation mutated document');
    }else{check(state().generation===initial&&state().objects===16,'canonical restore changed document');}
    const waiting=performance.now();await settled();const ended=performance.now();
    check(state().activeContacts===0,'contacts leaked',state().activeContacts,0);check(errors.length===0,'SDK errors',errors,[]);
    return {after:state(),canonical_wait:{started:waiting,ended,ms:ended-waiting},scope:'functional prepared consumers after settled canonical; no immediate-nextDown/performance claim'};
  },dump);
  const pixels=()=>{const gl=sdk.canvas.getContext('webgl2'),w=sdk.canvas.width,h=sdk.canvas.height,raw=new Uint8Array(w*h*4),top=new Uint8Array(raw.length);gl.readPixels(0,0,w,h,gl.RGBA,gl.UNSIGNED_BYTE,raw);for(let y=0;y<h;++y)top.set(raw.subarray((h-1-y)*w*4,(h-y)*w*4),y*w*4);return {bytes:top,width:w,height:h};};
  const cold=async(history,failed=false)=>{
    check(activePhase===null,'cold work entered timed phase');
    coldBytes={};const captured=[];
    const capture=async stage=>{
      const current=state(),image=pixels(),key=stage+'_rgba';coldBytes[key]=image.bytes;
      const documentRevision=(BigInt(base.metadata.documentRevision)+BigInt(current.generation)-BigInt(initialLive)).toString();
      const metadata={generation:current.generation,documentRevision,recoveryFrontier:base.metadata.recoveryFrontier};
      const row={stage,generation:current.generation,state:current,selection:sdk.readSelection(),camera:current.camera,activity:observer.activity(),
        rgba_key:key,rgba_bytes:image.bytes.length,rgba_sha256:await digest(image.bytes),width:image.width,height:image.height,
        checkpoint_request:metadata,checkpoint_mapping:{source:base.metadata.documentRevision,live_initial:initialLive,live_capture:current.generation,external_revision:documentRevision}};
      captured.push(row);
      try{const snapshot=await sdk.captureSnapshot(metadata);coldBytes[stage+'_snapshot']=snapshot.bytes;
        Object.assign(row,{snapshot_key:stage+'_snapshot',snapshot_bytes:snapshot.bytes.length,snapshot_sha256:await digest(snapshot.bytes),checkpoint_digest:snapshot.checkpointDigest});
      }catch(e){row.snapshot_error=dto(e);}
      return row;
    };
    if(failed){await capture('failure');return {status:'FAILED',stages:captured,predicates:[],available_stages:captured,scope:'failed input diagnostics retained before qualification'};}
    try{
      await sdk.selectTool('select');pointer('down',8,8);pointer('up',8,8);await settled();check(sdk.readSelection().count===0,'cold HUD deselection');
    }catch(e){await capture('failure');return {status:'FAILED',stages:captured,predicates:[],failure:dto(e),available_stages:captured};}
    if(!history){const value=await capture('before');return {status:'COMPLETED',stages:[value],predicates:[],available_stages:captured};}
    const result=await window.retainColdStages({capture,undo:async()=>{await sdk.undo();await settled();},redo:async()=>{await sdk.redo();await settled();},save:async()=>{},check});
    return {...result,available_stages:captured,scope:'cold correctness only; all available buffers are transferred before qualification'};
  };
  window.preparedBodyIndex={init,run,cold,dump,pull(kind,off,count){const b=coldBytes[kind].subarray(off,off+count);let s='';for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);},
    async close(){if(state()?.activeContacts)pointer('cancel',0,0);await sdk?.close();observer.restore();return {closed:!document.querySelector('#mount canvas'),errors};}};
}

async function artifactIdentity(root){const files={};for(const name of(await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())files[name]=sha(await fs.readFile(p));}return files;}
export async function runWorker({sdkArg,outArg,variant,oracle,cases=CASES,identityRunner='prepared_body_index_runner.py',functionalPacing=false,referenceCatalog=null}){
  const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
  const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,identityRunner),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const result={schema:'g5-prepared-body-index-functional-worker-v1',status:'STARTED',variant,source_before:source(),artifacts_before:await artifactIdentity(sdk),cases:[],page_errors:[]};
  result.functional_pacing=functionalPacing;
  if(referenceCatalog)result.reference_catalog_before=sha(await fs.readFile(referenceCatalog));
  const write=(name,value)=>fs.writeFile(path.join(out,name),JSON.stringify(value,null,2)+'\n');let server,browser,page;
  const {chromium}=await import('../../node_modules/playwright/index.mjs');const {compareSnapshotPixels,comparePixels}=await import('./ur0/pixel_oracle.mjs');
  try{
    const qualify=referenceCatalog?await createReferenceQualifier(JSON.parse(await fs.readFile(referenceCatalog)),oracle):compareSnapshotPixels;
    for(const n of ['S0','S2'])if(sha(await fs.readFile(path.join(fixtures,n+'.axsnap')))!==manifest[n].sha256)throw Error('fixture identity mismatch');
    server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://127.0.0.1');if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">window.functionalRun='+runWithDiagnostics.toString()+';window.retainColdStages='+retainColdStages.toString()+';window.restoreCamera='+restoreCamera.toString()+';window.runFunctionalErase='+runFunctionalErase.toString()+';window.boundedDisplayOpportunity='+boundedDisplayOpportunity.toString()+';await ('+consumer.toString()+')('+JSON.stringify({functionalPacing})+');</script>');return;}
      const [base,suffix]=u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];const file=path.resolve(base,suffix);if(!file.startsWith(base+path.sep))throw Error('path');
      res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});
    await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
    for(const id of cases){const fixtureName=id.slice(0,2),fixture=manifest[fixtureName];const row={id,fixture:fixtureName,status:'STARTED',execution_records:[],prepared_records:[]};result.cases.push(row);
      page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
      page.on('console',m=>{const text=m.text();if(text.startsWith('UR1C1_STAGE '))console.log(text.slice('UR1C1_STAGE '.length));if(text.startsWith('AXIOM_PREPARED_BODY '))try{row.prepared_records.push(JSON.parse(text.slice('AXIOM_PREPARED_BODY '.length)));}catch(e){result.page_errors.push('prepared record invalid: '+String(e));}if(text.startsWith('AXIOM_RENDER_EXECUTION '))try{row.execution_records.push(JSON.parse(text.slice('AXIOM_RENDER_EXECUTION '.length)));}catch(e){result.page_errors.push('execution record invalid: '+String(e));}});
      await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.preparedBodyIndex);
      row.init=await page.evaluate(f=>preparedBodyIndex.init(f),fixture);await write(id+'-init.json',row.init);
      row.observed=await page.evaluate(value=>preparedBodyIndex.run(value),{id,head:manifest.head});await write(id+'-observed.json',row.observed);row.status=row.observed.status;
      try{
        row.cold=await page.evaluate(({history,failed})=>preparedBodyIndex.cold(history,failed),{history:historyCase(id),failed:row.status!=='COMPLETED'});await write(id+'-cold.json',row.cold);
        row.cold_oracles=[];
        for(const stage of row.cold.available_stages){
          for(const [kind,length,ext] of [[stage.rgba_key,stage.rgba_bytes,'rgba'],[stage.snapshot_key,stage.snapshot_bytes,'axsnap']]){
            if(!kind)continue;const filename=path.join(out,id+'-'+stage.stage+'.'+ext),f=await fs.open(filename,'w');
            try{for(let off=0;off<length;off+=65536)await f.writeFile(Buffer.from(await page.evaluate(({kind,off})=>preparedBodyIndex.pull(kind,off,65536),{kind,off}),'base64'));}finally{await f.close();}
            if(sha(await fs.readFile(filename))!==(ext==='rgba'?stage.rgba_sha256:stage.snapshot_sha256))throw Error('cold transfer identity');
          }
        }
        // Transfer every stage first, including all data preceding a failed predicate.
        for(const stage of row.cold.available_stages){
          if(!stage.snapshot_key){row.status='FAILED';continue;}
          const prefix=path.join(out,id+'-'+stage.stage+'-reference');
          const oracleResult=await qualify({oracle,snapshot:path.join(out,id+'-'+stage.stage+'.axsnap'),actual:await fs.readFile(path.join(out,id+'-'+stage.stage+'.rgba')),
            request:{width:stage.width,height:stage.height,logical_width:800,logical_height:500,camera:stage.camera,hud_bounds:[]},prefix});
          row.cold_oracles.push({stage:stage.stage,...oracleResult});if(oracleResult.status!=='QUALIFIED')row.status='FAILED';
        }
        if(row.cold.available_stages.length!==(historyCase(id)?3:1)||row.cold.predicates.some(p=>!p.passed&&p.label!=='Undo/Redo pixel roundtrip'))row.status='FAILED';
        if(historyCase(id)){const beforeRef=path.join(out,id+'-before-reference.rgba'),redoRef=path.join(out,id+'-redo-reference.rgba');
        if(row.cold.available_stages.length===3&&await fs.readFile(beforeRef).then(b=>sha(b))===await fs.readFile(redoRef).then(b=>sha(b))){
          row.roundtrip=comparePixels(await fs.readFile(path.join(out,id+'-redo.rgba')),await fs.readFile(path.join(out,id+'-before.rgba')),await fs.readFile(path.join(out,id+'-before-reference.edge')));
          if(row.roundtrip.status!=='QUALIFIED')row.status='FAILED';
        }else{row.status='FAILED';row.roundtrip_error='before/redo independent canonical pixel references differ or are missing';}
        }
        row.pixel_qualification='original independent CPU geometry AA boundary, <=2 interior channel levels; raw hash predicates retained unchanged';
      }catch(e){row.status='FAILED';row.cold_error=String(e.stack??e);}
      row.final=await page.evaluate(()=>preparedBodyIndex.dump());await write(id+'-final.json',row.final);row.close=await page.evaluate(()=>preparedBodyIndex.close());if(!row.close.closed)throw Error('cleanup failure');await page.close();page=null;
      row.metrics=row.observed.diagnostics.phases.map(p=>({id:p.id,status:p.status,...summarizePhase(p),canonical_wait_ms:null,scope:'input phase only; actual canonical wait is separately recorded'}));
      await write(id+'-result.json',row);console.log(JSON.stringify({stage:'case-closed',id,status:row.status,errors:row.final.errors}));
    }
    const records=result.cases.flatMap(c=>c.execution_records);
    result.actual_route_facts={canonical_gpu_scopes:records.filter(r=>r.purpose===0&&r.gpu_intent&&r.actual_gpu_tiles>0).length,transient_gpu_scopes:records.filter(r=>r.purpose===1&&r.gpu_intent&&r.actual_gpu_tiles>0).length,transient_reuse_scopes:records.filter(r=>r.purpose===1&&r.reuse>0).length,records:records.length,scope:'actual Skia recordingContext coverage; Skia internal CPU preparation remains UNKNOWN'};
    result.actual_prepared_facts=result.cases.filter(c=>c.id.endsWith('erase-navigation')).map(c=>({id:c.id,...summarizePrepared(c.prepared_records)}));
    const qualified=variant!=='qualification'||result.actual_route_facts.canonical_gpu_scopes>0&&(result.actual_route_facts.transient_gpu_scopes>0||result.actual_route_facts.transient_reuse_scopes>0);
    result.status=result.cases.every(c=>c.status==='COMPLETED')&&qualified&&(variant!=='qualification'||result.actual_prepared_facts.every(r=>r.status==='OBSERVED'))&&result.page_errors.length===0?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
    if(variant==='normal'&&(records.length||result.cases.some(c=>c.prepared_records.length))){result.status='PRODUCT_FAILURE_OBSERVED';result.normal_trace_leak=true;}
  }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>preparedBodyIndex.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
  finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.source_after=source();result.artifacts_after=await artifactIdentity(sdk);
    result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(referenceCatalog){result.reference_catalog_after=sha(await fs.readFile(referenceCatalog));if(result.reference_catalog_after!==result.reference_catalog_before)result.source_stable=false;}
    if(!result.source_stable||!result.artifacts_stable||result.crash){result.status='HARD_FAILURE';result.identity_or_crash=true;}await write('worker-result.json',result);}
  console.log(JSON.stringify({stage:'worker-complete',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [sdkArg,outArg,variant,oracle]=process.argv.slice(2);await runWorker({sdkArg,outArg,variant,oracle});
}
