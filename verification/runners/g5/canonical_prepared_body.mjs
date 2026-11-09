// UR1c2: original public consumer/pacing/oracle, new leaf/work qualification.
import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {fileURLToPath} from 'node:url';import {createHash} from 'node:crypto';import {execFileSync} from 'node:child_process';
import {consumer,CASES,historyCase,restoreCamera,summarizePrepared,retainColdStages} from './prepared_body_index.mjs';
import {runWithDiagnostics,summarizePhase} from './transient_reference.mjs';
import {runFunctionalErase,boundedDisplayOpportunity} from './prepared_body_index_reverify_pacing.mjs';
import {createReferenceQualifier} from './prepared_body_index_reverify_reference.mjs';
const ROOT=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../..'),HERE=path.dirname(fileURLToPath(import.meta.url));
const sha=b=>createHash('sha256').update(b).digest('hex');
export function summarizeCanonicalPrepared(work,execution){
  const required=['canonical_queries','direct_queries','job_admissions','advances','support_knots','distance_inputs','compile_inputs','path_primitives','max_advance_knots'];
  const invalid=work.filter(r=>!r.service||required.some(k=>!Number.isSafeInteger(r[k])||r[k]<0)||r.max_advance_knots>32);
  const roots=[];
  for(const e of execution){
    const purpose=e.purpose===0?'canonical':e.purpose===1?'edit':null;if(!purpose)continue;
    const matching=work.filter(w=>w.purpose===purpose&&w.event_index<e.event_index).at(-1);if(!matching)continue;
    roots.push({service:matching.service,purpose,work:matching,canonical_draws:e.canonical_prepared_draws??0,direct_draws:e.direct_prepared_draws??0,actual_gpu_tiles:e.actual_gpu_tiles??0,actual_cpu_tiles:e.actual_cpu_tiles??0});
  }
  const canonical=new Map(),shared=[];
  for(const r of roots){
    if(r.purpose==='canonical'&&r.canonical_draws>0&&r.work.canonical_queries>0&&r.actual_gpu_tiles>0)canonical.set(r.service,r);
    if(r.direct_draws>0&&r.work.direct_queries>0&&r.actual_gpu_tiles>0&&canonical.has(r.service))shared.push({service:r.service,canonical:canonical.get(r.service),direct:r});
  }
  return {status:!invalid.length&&shared.length?'OBSERVED':'MISSING',invalid_records:invalid,shared_sequences:shared,canonical_roots:roots.filter(r=>r.canonical_draws>0&&r.work.canonical_queries>0&&r.actual_gpu_tiles>0),
    work_totals:Object.fromEntries(required.map(k=>[k,k==='max_advance_knots'?Math.max(0,...work.map(r=>r[k]??0)):work.reduce((n,r)=>n+(r[k]??0),0)])),
    scope:'actual canonical raster and Direct prepared draw on one existing service; unique source work excludes separately counted visible replay and first allocation validation'};
}
export function qualifyCaseConsumers(id,work,execution){
  const facts=summarizeCanonicalPrepared(work,execution),canonicalOnly=id.endsWith('canonical');
  return {...facts,required_consumers:canonicalOnly?'canonical':'canonical-and-direct',
    status:canonicalOnly&&!facts.invalid_records.length&&facts.canonical_roots.length?'OBSERVED':facts.status};
}
async function artifactIdentity(root){const files={};for(const name of(await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())files[name]=sha(await fs.readFile(p));}return files;}
export async function runWorker({sdkArg,outArg,variant,oracle,cases=CASES,identityRunner='canonical_prepared_body_runner.py',functionalPacing=true,referenceCatalog=null}){
  const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
  const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures'),manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
  const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,identityRunner),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
  const result={schema:'g5-canonical-prepared-body-worker-v1',status:'STARTED',variant,source_before:source(),artifacts_before:await artifactIdentity(sdk),cases:[],page_errors:[]};
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
    for(const id of cases){const fixtureName=id.slice(0,2),fixture=manifest[fixtureName];const row={id,fixture:fixtureName,status:'STARTED',execution_records:[],prepared_records:[],preparation_records:[]};result.cases.push(row);
      page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>result.page_errors.push(String(e)));page.on('crash',()=>{result.crash=true;});
      let eventIndex=0;page.on('console',m=>{const text=m.text();const event_index=eventIndex++;if(text.startsWith('AXIOM_PREPARED_BODY_WORK '))try{row.preparation_records.push({...JSON.parse(text.slice('AXIOM_PREPARED_BODY_WORK '.length)),event_index});}catch(e){result.page_errors.push('body work invalid: '+String(e));}if(text.startsWith('UR1C1_STAGE '))console.log(text.slice('UR1C1_STAGE '.length));if(text.startsWith('AXIOM_PREPARED_BODY '))try{row.prepared_records.push(JSON.parse(text.slice('AXIOM_PREPARED_BODY '.length)));}catch(e){result.page_errors.push('prepared record invalid: '+String(e));}if(text.startsWith('AXIOM_RENDER_EXECUTION '))try{row.execution_records.push({...JSON.parse(text.slice('AXIOM_RENDER_EXECUTION '.length)),event_index});}catch(e){result.page_errors.push('execution record invalid: '+String(e));}});
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
    result.canonical_prepared_facts=result.cases.map(c=>({id:c.id,...qualifyCaseConsumers(c.id,c.preparation_records,c.execution_records)}));
    const qualified=variant!=='qualification'||result.actual_route_facts.canonical_gpu_scopes>0&&(result.actual_route_facts.transient_gpu_scopes>0||result.actual_route_facts.transient_reuse_scopes>0);
    result.status=result.cases.every(c=>c.status==='COMPLETED')&&qualified&&(variant!=='qualification'||result.actual_prepared_facts.every(r=>r.status==='OBSERVED'))&&result.page_errors.length===0&&(variant!=='qualification'||result.canonical_prepared_facts.every(r=>r.status==='OBSERVED'))?'OBSERVED':'PRODUCT_FAILURE_OBSERVED';
    if(variant==='normal'&&(records.length||result.cases.some(c=>c.prepared_records.length||c.preparation_records.length))){result.status='PRODUCT_FAILURE_OBSERVED';result.normal_trace_leak=true;}
  }catch(e){result.status='HARD_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>preparedBodyIndex.dump());await write('partial.json',result.partial);}catch(x){result.partial_error=String(x);}}
  finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.source_after=source();result.artifacts_after=await artifactIdentity(sdk);
    result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
    if(referenceCatalog){result.reference_catalog_after=sha(await fs.readFile(referenceCatalog));if(result.reference_catalog_after!==result.reference_catalog_before)result.source_stable=false;}
    if(!result.source_stable||!result.artifacts_stable||result.crash){result.status='HARD_FAILURE';result.identity_or_crash=true;}await write('worker-result.json',result);}
  console.log(JSON.stringify({stage:'worker-complete',status:result.status,error:result.error??null}));if(result.status==='HARD_FAILURE')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [sdkArg,outArg,variant,oracle,referenceCatalog]=process.argv.slice(2);await runWorker({sdkArg,outArg,variant,oracle,referenceCatalog});
}
