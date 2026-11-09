import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {runWorker as coreWorker} from './canonical_prepared_body_working_set.mjs';
import {createReferenceQualifier} from './prepared_body_index_reverify_reference.mjs';

const HERE=path.dirname(fileURLToPath(import.meta.url)),ROOT=path.resolve(HERE,'../../..');
const sha=b=>createHash('sha256').update(b).digest('hex');
export function canonicalIdentity(s){
  return {documentId:s.documentId,generation:s.generation,objects:s.objects,
    operationId:s.lastCommit.operationId,ordinal:s.lastCommit.ordinal,runtimeEpoch:s.lastCommit.runtimeEpoch};
}
export function qualifyQueues(rows){
  const owners=new Map();
  for(const row of rows){
    const admitted=BigInt(row.admitted),retired=BigInt(row.ordered_retired);
    const before=owners.get(row.owner);
    if(retired>admitted||admitted-retired!==BigInt(row.queued)||
       before&&(admitted<BigInt(before.admitted)||retired<BigInt(before.ordered_retired)))return {status:'INVALID',row};
    owners.set(row.owner,row);
  }
  return {status:rows.length&&[...owners.values()].every(r=>!r.needs_service)?'OBSERVED':'MISSING',
    final:[...owners.values()],scope:'owner-observed service/ordered retirement; not pure GPU time or physical display'};
}

// Serialized into the verification page. Only public root SDK commands edit or
// navigate. GL wrappers withhold observations, never fabricate GPU completion.
export async function recoveryConsumer(){
  const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');
  let sdk,fixture,initialGeneration;
  const frames=[],errors=[],operations=[],events=[],cold=[],buffers={};
  const observer=installNavigationObserver(()=>sdk?.canvas);
  const raf=()=>new Promise(resolve=>{resolve.g5SamplerRaf=true;requestAnimationFrame(resolve);});
  const state=()=>sdk.readState();
  const check=(ok,label)=>{if(!ok)throw Error('PRODUCT_CHECK: '+label);};
  const digest=async b=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',b)),v=>v.toString(16).padStart(2,'0')).join('');
  const snapshot=()=>({at:performance.now(),state:state(),frames:frames.length,operations:operations.length,activity:observer.activity()});
  const settled=async()=>{
    const end=performance.now()+30000;
    do{await raf();const s=state();if(s.surface.available&&s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&observer.activity().pending===0)return snapshot();}
    while(performance.now()<end);
    throw Error('PRODUCT_CHECK: resource/canonical idle completion timeout');
  };
  const init=async f=>{
    fixture=f;const bytes=new Uint8Array(await(await fetch('/fixtures/'+f.name+'.axsnap')).arrayBuffer());
    check(await digest(bytes)===f.sha256,'fixture bytes');
    const {createRuntime}=await import('/axiom.mjs');
    sdk=await createRuntime({mount:document.getElementById('mount'),documentId:f.state.documentId,
      builtinFonts:false,permission:'localSingleUser',restore:{bytes,metadata:f.metadata},
      onFrame:value=>frames.push({at:performance.now(),...value}),onError:e=>errors.push({code:e.code,message:String(e.message),at:performance.now()}),
      onOperation:b=>operations.push({at:performance.now(),bytes:b.length})});
    initialGeneration=state().generation;await settled();
    check(state().objects===16&&state().activeContacts===0,'restored canonical state');
    return {snapshot:snapshot(),build:sdk.readBuildInfo(),fixture_sha256:f.sha256,resource_binding:sdk.resources.list().binding,dpr:devicePixelRatio};
  };
  const capture=async stage=>{
    const current=state(),gl=sdk.canvas.getContext('webgl2'),width=sdk.canvas.width,height=sdk.canvas.height;
    const raw=new Uint8Array(width*height*4),rgba=new Uint8Array(raw.length);
    gl.readPixels(0,0,width,height,gl.RGBA,gl.UNSIGNED_BYTE,raw);
    for(let y=0;y<height;++y)rgba.set(raw.subarray((height-y-1)*width*4,(height-y)*width*4),y*width*4);
    const row={stage,state:current,selection:sdk.readSelection(),activity:observer.activity(),camera:current.camera,width,height,
      rgba_key:stage+'-rgba',rgba_bytes:rgba.length,rgba_sha256:await digest(rgba),resource_binding:sdk.resources.list().binding};
    buffers[row.rgba_key]=rgba;cold.push(row);
    try{
      const documentRevision=(BigInt(fixture.metadata.documentRevision)+BigInt(current.generation)-BigInt(initialGeneration)).toString();
      const request={generation:current.generation,documentRevision,recoveryFrontier:fixture.metadata.recoveryFrontier};
      const value=await sdk.captureSnapshot(request);
      Object.assign(row,{snapshot_key:stage+'-snapshot',snapshot_bytes:value.bytes.length,snapshot_sha256:await digest(value.bytes),checkpoint_digest:value.checkpointDigest,checkpoint_request:request});
      buffers[row.snapshot_key]=value.bytes;
    }catch(e){row.snapshot_error=String(e.stack??e);}
    return row;
  };
  const withhold=async()=>{
    const contexts=[sdk.canvas,...document.querySelectorAll('#mount canvas')].filter((c,i,a)=>a.indexOf(c)===i).map(c=>c.getContext('webgl2')).filter(Boolean);
    const saved=[],calls=[];
    for(const gl of contexts)for(const name of ['clientWaitSync','getSyncParameter']){
      const fn=gl[name];saved.push({gl,name,fn});
      gl[name]=function(...args){
        if(name==='clientWaitSync'){calls.push({name,at:performance.now()});return gl.TIMEOUT_EXPIRED;}
        if(args[1]===gl.SYNC_STATUS){calls.push({name,at:performance.now()});return gl.UNSIGNALED;}
        return Reflect.apply(fn,this,args);
      };
    }
    const before=snapshot(),opportunities=[];
    try{
      await sdk.panBy(1,0);
      for(let i=0;i<8;++i){await raf();opportunities.push(snapshot());}
    }finally{for(const {gl,name,fn} of saved)gl[name]=fn;}
    const released=snapshot();const drained=await settled();
    events.push({kind:'withheld-completion-observations',before,opportunities,calls,released,drained,
      injected:true,scope:'eight real RAF observation opportunities; does not assert physically pending GPU'});
    check(calls.length>0,'withheld wrapper reached actual GL completion observation');
    await sdk.panBy(-1,0);await settled();
    return events.at(-1);
  };
  const lose=async role=>{
    const canvas=role==='canonical'?sdk.canvas:document.querySelector('#mount canvas[id$="-preview"]');
    const gl=canvas?.getContext('webgl2'),extension=gl?.getExtension('WEBGL_lose_context');
    if(!extension)throw Error('ENVIRONMENT: WEBGL_lose_context unavailable');
    const before=snapshot();
    const lost=new Promise(resolve=>canvas.addEventListener('webglcontextlost',resolve,{once:true}));
    extension.loseContext();await lost;await raf();
    const unavailable=snapshot();check(!unavailable.state.surface.available,'lost surface lifecycle unavailable');
    check(unavailable.frames===before.frames,'lost context did not create presentation receipt');
    const restored=new Promise(resolve=>canvas.addEventListener('webglcontextrestored',resolve,{once:true}));
    extension.restoreContext();await restored;const after=await settled();
    events.push({kind:'real-webgl-loss-restore',role,before,unavailable,after,
      actual_extension:true,context_object_retained:true,physical_loss_distinct_from_logical_paired_loss:true});
    check(after.frames>before.frames,'new binding actually presented');
    check(BigInt(after.state.target.attachmentEpoch)>BigInt(before.state.target.attachmentEpoch),'new attachment generation; private trace binds full surface generation');
    return events.at(-1);
  };
  const dump=()=>({state:sdk?state():null,frames,errors,operations,events,cold,activity:observer.activity()});
  window.resourceRecovery={init,capture,withhold,lose,dump,
    pull(key,off,count){const b=buffers[key].subarray(off,off+count);let s='';for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);},
    async close(){await sdk?.close();observer.restore();return {closed:!document.querySelector('#mount canvas'),errors,operations};}};
}

async function artifactIdentity(root){const value={};for(const name of(await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())value[name]=sha(await fs.readFile(p));}return value;}
const qualificationFactory=`import factory from '/_engine/internal/axiom.mjs';
export default async function(args){const module=await factory(args);
if(module._axiom_g5_trace_control(2,101)!==0||module._axiom_g5_trace_control(1,0)!==0)throw Error('qualification trace initialization');
const drain=()=>{const chunks=[];for(let i=0;i<256;++i){if(module._axiom_g5_trace_control(4,0)!==0)throw Error('trace drain failed');const p=module._axiom_g5_trace_bytes(),n=module._axiom_g5_trace_size();const value=JSON.parse(new TextDecoder().decode(module.HEAPU8.subarray(p,p+n)));chunks.push(value);if(value.finished)return chunks;}throw Error('bounded trace drain exceeded');};
let collected=[];const timer=setInterval(()=>collected.push(...drain()),100);
window.resourceTrace=()=>{const result=[...collected,...drain()];collected=[];return result;};
window.resourceTraceClose=()=>{clearInterval(timer);return window.resourceTrace();};
return module;}`;

export async function recoveryWorker({sdkArg,outArg,variant,oracle,referenceCatalog,identityRunner='resource_recovery_runner.py'}){
  const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
  const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,identityRunner),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const write=(name,value)=>fs.writeFile(path.join(out,name),JSON.stringify(value,null,2)+'\n');
  const result={schema:'g5-ur1d-real-resource-worker-v1',status:'STARTED',variant,source_before:source(),artifacts_before:await artifactIdentity(sdk),cases:[],page_errors:[],rss_guard:'external owned-process-family guard'};
  let server,browser,page;
  const {chromium}=await import('../../node_modules/playwright/index.mjs');
  try{
    const qualify=await createReferenceQualifier(JSON.parse(await fs.readFile(referenceCatalog)),oracle);
    server=http.createServer(async(req,res)=>{try{
      const u=new URL(req.url,'http://127.0.0.1');
      if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">await ('+recoveryConsumer.toString()+')();</script>');return;}
      if(variant==='qualification'&&u.pathname==='/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(qualificationFactory);return;}
      const [base,suffix]=u.pathname.startsWith('/_engine/')?[sdk,u.pathname.slice(9)]:u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];
      const p=path.resolve(base,suffix);if(!p.startsWith(base+path.sep))throw Error('path');
      res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(p));
    }catch(e){res.statusCode=404;res.end(String(e));}});
    await new Promise(r=>server.listen(0,'127.0.0.1',r));
    browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser={version:browser.version(),headless:true};
    for(const name of ['S0','S2']){
      if(sha(await fs.readFile(path.join(fixtures,name+'.axsnap')))!==manifest[name].sha256)throw Error('fixture identity');
      const row={id:name+'-retirement-context-recovery',status:'STARTED',retirements:[],contexts:[],execution:[],cold:[],traces:[],failures:[]};result.cases.push(row);
      page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
      page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
      page.on('console',m=>{const text=m.text();for(const [prefix,key] of [['AXIOM_GPU_RETIREMENT ','retirements'],['AXIOM_GPU_CONTEXT ','contexts'],['AXIOM_RENDER_EXECUTION ','execution']])if(text.startsWith(prefix))try{row[key].push(JSON.parse(text.slice(prefix.length)));}catch(e){result.page_errors.push(String(e));}});
      await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.resourceRecovery);
      row.init=await page.evaluate(f=>resourceRecovery.init(f),manifest[name]);await write(name+'-init.json',row.init);
      const saveCold=async stage=>{
        const value=await page.evaluate(s=>resourceRecovery.capture(s),stage);row.cold.push(value);await write(name+'-cold.json',row.cold);
        for(const [key,count,ext] of [[value.rgba_key,value.rgba_bytes,'rgba'],[value.snapshot_key,value.snapshot_bytes,'axsnap']]){
          if(!key)continue;const f=await fs.open(path.join(out,name+'-'+stage+'.'+ext),'w');
          try{for(let off=0;off<count;off+=65536)await f.writeFile(Buffer.from(await page.evaluate(({key,off})=>resourceRecovery.pull(key,off,65536),{key,off}),'base64'));}finally{await f.close();}
        }
        if(variant==='qualification')row.traces.push({stage,chunks:await page.evaluate(()=>resourceTrace())});
        await write(name+'-trace.json',row.traces);
      };
      try{
        await saveCold('before-loss');
        row.withheld=await page.evaluate(()=>resourceRecovery.withhold());await write(name+'-withheld.json',row.withheld);
        row.canonical=await page.evaluate(()=>resourceRecovery.lose('canonical'));await saveCold('after-canonical-restore');
        row.preview=await page.evaluate(()=>resourceRecovery.lose('preview'));await saveCold('after-preview-restore');
      }catch(e){row.failures.push(String(e.stack??e));if(String(e).includes('ENVIRONMENT:'))throw e;}
      row.final=await page.evaluate(()=>resourceRecovery.dump());await write(name+'-final.json',row.final);
      // All obtained cold buffers are durable before any pixel/state predicate.
      for(const stage of row.cold){
        if(!stage.snapshot_key){row.failures.push('snapshot unavailable at '+stage.stage);continue;}
        const actual=await fs.readFile(path.join(out,name+'-'+stage.stage+'.rgba')),snapshot=path.join(out,name+'-'+stage.stage+'.axsnap');
        if(sha(actual)!==stage.rgba_sha256||sha(await fs.readFile(snapshot))!==stage.snapshot_sha256)throw Error('cold transfer identity');
        stage.oracle=await qualify({oracle,snapshot,actual,request:{width:stage.width,height:stage.height,logical_width:800,logical_height:500,camera:stage.camera,hud_bounds:[]},prefix:path.join(out,name+'-'+stage.stage+'-reference')});
        if(stage.oracle.status!=='QUALIFIED')row.failures.push('independent pixel mismatch at '+stage.stage);
      }
      row.document_unchanged=row.cold.length===3&&row.cold.every(s=>JSON.stringify(canonicalIdentity(s.state))===JSON.stringify(canonicalIdentity(row.cold[0].state))&&s.snapshot_sha256===row.cold[0].snapshot_sha256&&s.resource_binding===row.cold[0].resource_binding);
      if(!row.document_unchanged)row.failures.push('canonical/snapshot/resource identity changed across loss');
      row.queues=qualifyQueues(row.retirements);
      if(variant==='qualification'&&(row.queues.status!=='OBSERVED'||!row.contexts.some(x=>x.role==='canonical'&&x.event==='physical-loss')||!row.contexts.some(x=>x.role==='preview'&&x.event==='physical-loss')))row.failures.push('actual resource/context facts missing');
      if(variant==='normal'&&(row.retirements.length||row.contexts.length||row.execution.length||await page.evaluate(()=>typeof window.resourceTrace!=='undefined')))row.failures.push('normal diagnostic leak');
      row.close=await page.evaluate(()=>resourceRecovery.close());if(!row.close.closed)throw Error('owner close cleanup');
      if(variant==='qualification'){
        row.traces.push({stage:'owner-close',chunks:await page.evaluate(()=>resourceTraceClose())});
        await write(name+'-trace.json',row.traces);
        if(row.traces.some(t=>t.chunks.some(c=>c.loss.critical)))row.failures.push('critical resource observation loss');
      }
      if(row.final.errors.length||row.final.operations.length!==row.init.snapshot.operations)row.failures.push('unexpected SDK error or operation across recovery');
      row.status=row.failures.length?'FAILED':'COMPLETED';await write(name+'-result.json',row);
      await page.close();page=null;console.log(JSON.stringify({stage:'recovery-case-closed',id:row.id,status:row.status,failures:row.failures}));
    }
    result.status=result.cases.every(c=>c.status==='COMPLETED')&&!result.page_errors.length?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
  }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.resourceRecovery?.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
  finally{
    if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
    result.source_after=source();result.artifacts_after=await artifactIdentity(sdk);
    result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(!result.source_stable||!result.artifacts_stable||result.crash){result.status='HARD_FAILURE';result.identity_or_crash=true;}
    await write('worker-result.json',result);
  }
  console.log(JSON.stringify({stage:'worker-complete',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [kind,sdkArg,outArg,variant,oracle,referenceCatalog]=process.argv.slice(2);
  if(kind==='core')await coreWorker({sdkArg,outArg,variant,oracle,referenceCatalog,identityRunner:'resource_recovery_runner.py',functionalPacing:true});
  else if(kind==='recovery')await recoveryWorker({sdkArg,outArg,variant,oracle,referenceCatalog});
  else throw Error('explicit core/recovery kind required');
}
