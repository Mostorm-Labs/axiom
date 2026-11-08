// Own browser/process only. Run under bounded_worker.py; the parent remains
// responsive when a synchronous WASM call wedges this page.
import {chromium} from '../../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {calibrate} from './fixture_plan.mjs';
import {copySnapshot} from './stream_transfer.mjs';
import {compareSnapshotPixels} from './pixel_oracle.mjs';
import {copyCorrectnessBytes,createStageRecorder} from './correctness_transfer.mjs';
import {validateLiveSubject} from './correctness_contract.mjs';
const here=path.dirname(fileURLToPath(import.meta.url)),repo=path.resolve(here,'../../../..');
const [sdkArg,outArg,mode='fixture',branch='masked_drag',toggle='ON']=process.argv.slice(2),sdkRoot=path.resolve(sdkArg),out=path.resolve(outArg);
await fs.mkdir(out,{recursive:true});
const hash=b=>createHash('sha256').update(b).digest('hex');
const inputs=JSON.parse(await fs.readFile(path.join(repo,'docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json')));
const actions=JSON.parse(await fs.readFile(path.join(repo,'docs/planning/g5-unified-render/ur0a/fixtures/immediate-actions-v1.json')));
const stage=object=>console.log(JSON.stringify(object));
const write=(name,data)=>fs.writeFile(path.join(out,name),JSON.stringify(data,null,2)+'\n');
let phaseTimer;const phases=[];
const arm=(phase,limit_ms)=>{clearTimeout(phaseTimer);phases.push({phase,limit_ms,node_monotonic_ns:String(process.hrtime.bigint())});phaseTimer=setTimeout(()=>{stage({stage:'phase-timeout',phase,limit_ms});process.exit(124);},limit_ms);};
arm('setup',mode==='realtime'?40000:45000);
async function artifactHashes(){const files={};for(const n of (await fs.readdir(sdkRoot,{recursive:true})).sort()){const p=path.join(sdkRoot,n);if((await fs.stat(p)).isFile())files[n]=hash(await fs.readFile(p));}return files;}
const source=()=>JSON.parse(execFileSync('python3.12',[path.join(here,'source_identity.py'),repo],{encoding:'utf8',maxBuffer:64*1024*1024}));
let browser,server;const result={schema:'g5-ur0b-worker-v1',mode,branch,toggle,errors:[],status:'STARTED',artifacts_before:await artifactHashes(),source_before:source()};
await write('run-inputs.json',{...result,fixture_sha256:hash(await fs.readFile(path.join(repo,'docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json'))),actions_sha256:hash(await fs.readFile(path.join(repo,'docs/planning/g5-unified-render/ur0a/fixtures/immediate-actions-v1.json')))});
try{
 server=http.createServer(async(req,res)=>{try{const url=new URL(req.url,'http://127.0.0.1'),name=decodeURIComponent(url.pathname);let file;
  if(name==='/probe.html')file=path.join(here,'consumer.html');else if(name.startsWith('/ur0/'))file=path.resolve(here,name.slice(5));else file=path.resolve(sdkRoot,'.'+name);
  if(!file.startsWith(here+path.sep)&&!file.startsWith(sdkRoot+path.sep))throw Error('path escape');
  res.setHeader('Content-Type',name.endsWith('.mjs')?'text/javascript':name.endsWith('.html')?'text/html':name.endsWith('.wasm')?'application/wasm':'application/octet-stream');
  if(name==='/internal/axiom.mjs'&&toggle!=='NORMAL'){
   // Only this isolated verification server wraps the generated private
   // factory. The public root wrapper/types and business calls remain intact.
   const shim=`import factory from '/internal/ur0-original.mjs';export default async function(options){const m=await factory(options);const read=()=>new TextDecoder().decode(m.HEAPU8.slice(m._axiom_g5_trace_bytes(),m._axiom_g5_trace_bytes()+m._axiom_g5_trace_size()));const control=(op,v=0)=>{const r=m._axiom_g5_trace_control(op,v);if(r!==0)throw Error('private control '+op+':'+r);};globalThis.__ur0diag=Object.freeze({control,clock(){control(3);return JSON.parse(read());},drain(){control(4);return JSON.parse(read());}});return m;}`;
   result.loader_sha256=hash(shim);res.end(shim);
  }else res.end(await fs.readFile(name==='/internal/ur0-original.mjs'?path.join(sdkRoot,'internal/axiom.mjs'):file));
 }catch(error){res.statusCode=404;res.end(String(error));}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const executable='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
 browser=await chromium.launch({executablePath:executable,headless:true});
 result.browser={version:browser.version(),executable,flags:'Playwright defaults; no disable-gpu; headless true'};
 const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
 page.on('pageerror',error=>result.errors.push(String(error)));
 page.on('console',message=>{if(message.text().startsWith('UR0_STAGE '))stage(JSON.parse(message.text().slice(10)));});
 await page.addInitScript(()=>{let word=0x12345678;crypto.getRandomValues=a=>{const b=new Uint8Array(a.buffer,a.byteOffset,a.byteLength);for(let i=0;i<b.length;++i){word^=word<<13;word^=word>>>17;word^=word<<5;b[i]=word&255;}return a;};});
 await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');
 await page.waitForFunction(()=>!!window.ur0);stage({stage:'initializing'});
 result.init=await page.evaluate(()=>ur0.init());await write('init.json',result.init);stage({stage:'setup'});
 const correctnessStages=mode==='correctness'?createStageRecorder({domain:'node',clock:()=>String(process.hrtime.bigint()),emit:stage}):null;
 const coldRelease=(channel,cancel)=>page.evaluate(({channel,cancel})=>channel==='snapshot'?ur0.snapshotRelease(cancel):ur0.pixelRelease(cancel),{channel,cancel});
 const coldCopy=(channel,length,source_sha256,file)=>copyCorrectnessBytes({length,source_sha256,
  pullBatch:offset=>page.evaluate(async({channel,length,offset})=>{
   const {encodeChunkBatch}=await import('/ur0/correctness_transfer.mjs');
   return encodeChunkBatch({length,offset,pull:(o,n)=>channel==='snapshot'?ur0.snapshotChunk(o,n):ur0.pixelChunk(o,n)});
  },{channel,length,offset}),write:chunk=>file.writeFile(chunk),finish:()=>coldRelease(channel,false),cancel:()=>coldRelease(channel,true)});
 const correctnessCapture=async label=>{
  const row=await correctnessStages.run('snapshot acquire '+label,()=>page.evaluate(label=>ur0.checkpoint(label),label));
  const file=await fs.open(path.join(out,label+'.axsnap'),'wx').catch(async error=>{await coldRelease('snapshot',true);throw error;});let copied;
  try{copied=await correctnessStages.run('snapshot transfer '+label,()=>coldCopy('snapshot',row.snapshot.byte_length,row.snapshot.source_sha256,file));}finally{await file.close();}
  row.snapshot={...row.snapshot,path:label+'.axsnap',sha256:copied.received_sha256,transport:'cold base64; sequential <=32768-byte units, <=8 units/262144 decoded bytes per evaluate; source/received SHA256 identical'};
  await write(label+'.json',row);return row;
 };
 const capture=async label=>{
  if(mode==='correctness')return correctnessCapture(label);
  stage({stage:'cold-snapshot-'+label+'-begin'});const row=await page.evaluate(label=>ur0.checkpoint(label),label);
  const file=await fs.open(path.join(out,label+'.axsnap'),'wx'),digest=createHash('sha256');
  try{await copySnapshot({length:row.snapshot.byte_length,pull:(offset,count)=>page.evaluate(({offset,count})=>ur0.snapshotChunk(offset,count),{offset,count}),write:async chunk=>{await file.writeFile(chunk);digest.update(chunk);}});}
  finally{await file.close();}
  const received_sha256=digest.digest('hex');await page.evaluate(()=>ur0.snapshotRelease(false));
  if(received_sha256!==row.snapshot.source_sha256)throw Error('public snapshot bytes changed in transport');
  row.snapshot={...row.snapshot,path:label+'.axsnap',sha256:received_sha256,transport:'sequential chunks <=32768 bytes; source/received SHA256 identical'};
  await write(label+'.json',row);stage({stage:'cold-snapshot-'+label+'-complete',bytes:row.snapshot.byte_length});return row;
 };
 if(mode==='correctness'){
  result.correctness=[];result.correctness_scope='separate cold correctness worker, no primary timing';
  const cdp=await page.context().newCDPSession(page),oracle=path.resolve(process.env.UR0_FIXTURE_ORACLE??path.join(repo,'out/host/runtime/render/axiom_ur0_fixture_oracle'));
  result.correctness_oracle_sha256=hash(await fs.readFile(oracle));
  result.setup=await page.evaluate(kind=>ur0.correctnessSetup(kind,false),branch);arm('correctness-only',30000);
  for(const pairedToggle of ['OFF','ON']){
   if(pairedToggle==='ON'){
    await correctnessStages.run('close OFF',()=>page.evaluate(()=>ur0.close()));
    await correctnessStages.run('metrics override/reload',async()=>{await cdp.send('Emulation.setDeviceMetricsOverride',{width:900,height:650,deviceScaleFactor:2,mobile:false});await page.reload();await page.waitForFunction(()=>!!window.ur0);});
    await correctnessStages.run('ON init/setup',async()=>{result.paired_init=await page.evaluate(()=>ur0.init());await page.evaluate(kind=>ur0.correctnessSetup(kind,true),branch);});
   }
   for(let n=0;n<2;++n){
    const label=pairedToggle.toLowerCase()+'-'+n,before=await capture(label+'-before');
    if(branch==='dpr_switch'||branch==='warm_miss_pressure'&&n===1)await correctnessStages.run('metrics override step',async()=>{await cdp.send('Emulation.setDeviceMetricsOverride',{width:900,height:650,deviceScaleFactor:n?4:2,mobile:false});await page.waitForTimeout(100);});
    const pixels=await correctnessStages.run('correctnessNext/render/settle',()=>page.evaluate(()=>ur0.correctnessNext()));
    const file=await fs.open(path.join(out,label+'.candidate.rgba'),'wx').catch(async error=>{await coldRelease('pixel',true);throw error;});let transferred;
    try{transferred=await correctnessStages.run('pixel transfer',()=>coldCopy('pixel',pixels.byte_length,pixels.source_sha256,file));}finally{await file.close();}
    const after=await capture(label+'-after');
    const reference=pixels.reference==='before'?before:after,actual=await fs.readFile(path.join(out,label+'.candidate.rgba'));
    let applicability=null;
    if(branch==='lower_z_translucent_drag'&&n===0)applicability=await correctnessStages.run('native subject decode',async()=>{
     const decode=row=>JSON.parse(execFileSync(oracle,[path.join(out,row.snapshot.path)],{encoding:'utf8',timeout:10000,maxBuffer:1024*1024}));
     const nativeBefore=decode(before),nativeAfter=decode(after);
     if(pixels.subject?.before.state.generation!==before.state.generation||pixels.subject?.after.state.generation!==after.state.generation)throw Error('public capture/reference generation changed');
     const proof=validateLiveSubject({...pixels.subject,nativeBefore,nativeAfter,capturedBefore:before,capturedAfter:after});
     await write(label+'.subject.json',{proof,nativeBefore,nativeAfter,before_snapshot_sha256:before.snapshot.sha256,after_snapshot_sha256:after.snapshot.sha256});return proof;
    });
    const compared=await correctnessStages.run('native reference/compare',()=>compareSnapshotPixels({oracle,snapshot:path.join(out,reference.snapshot.path),request:pixels.request,actual,prefix:path.join(out,label+'.reference')}));
    const entry={case_id:branch,toggle:pairedToggle,step:n,...pixels,applicability,transport:transferred,comparison:compared,candidate_sha256:hash(actual),reference_snapshot_sha256:reference.snapshot.sha256};
    result.correctness.push(entry);await write(label+'.correctness.json',entry);
   }
  }
  result.correctness_status=result.correctness.every(c=>c.comparison.status==='QUALIFIED')?'QUALIFIED':
   [0,1].every(n=>{const off=result.correctness.find(c=>c.toggle==='OFF'&&c.step===n),on=result.correctness.find(c=>c.toggle==='ON'&&c.step===n);return off.comparison.status===on.comparison.status&&(off.comparison.status==='QUALIFIED'||off.candidate_sha256===on.candidate_sha256&&off.reference_snapshot_sha256===on.reference_snapshot_sha256);})?'PRODUCT_RED_IN_BOTH_OFF_ON':'BLOCKED_EVIDENCE';
  arm('cleanup',15000);
 }
 else if(mode==='realtime'){arm('measurement',35000);result.measurement=await page.evaluate(({fixture,hz,on})=>ur0.realtime({fixture,hz,on}),{fixture:inputs,hz:Number(branch),on:toggle==='ON'});arm('cleanup',15000);await write('measurement.json',result.measurement);result.final=await capture('final');}
 else{
  result.setup=mode==='legacy'?await page.evaluate(kind=>ur0.setupLegacy(kind),branch):await page.evaluate(fixture=>ur0.setup({fixture}),inputs);await write('setup.json',result.setup);
  result.initial=await capture('initial');
  if(mode!=='setup'){
   const prepared=mode==='legacy'?{measurement_schedule:'LEGACY_CLOSED_LOOP',kind:branch,source_references:['verification/runners/g5/drag_copies.mjs','verification/runners/g5/cumulative_erase.mjs'],fixed_rounds:branch==='legacy16_erase'?10:1}:await page.evaluate(events=>ur0.prepareMeasurement(events),actions.plans[branch].events);await write('prepared-plan.json',prepared);
   stage({stage:'measurement',branch});
   const limit=mode==='main'||mode==='legacy'?120000:30000;arm('measurement',limit);
   result.measurement=mode==='legacy'?await page.evaluate(({kind,on,start})=>ur0.measureLegacy({kind,on,start}),{kind:branch,on:toggle==='ON',start:result.setup.legacy_drag_start}):await page.evaluate(({prepared,on,limit})=>ur0.measure({prepared,on,deadlineMs:limit}),{prepared,on:toggle==='ON',limit});
   arm('cleanup',15000);
   await write('measurement.json',result.measurement);result.final=await capture('final');
  }else arm('cleanup',15000);
 }
 if(result.measurement?.clock_before)result.calibration=calibrate(result.measurement.clock_before,result.measurement.clock_after);
 if(toggle!=='NORMAL'){
  stage({stage:'cold-trace-begin'});const stream=await fs.open(path.join(out,'raw-trace.jsonl'),'wx'),pages=[];let total=0,last;
  try{for(let i=0;i<4096;++i){const p=await page.evaluate(()=>ur0.drain());last=p;total+=p.records.length;
   await stream.writeFile(p.records.map(r=>JSON.stringify(r)+'\n').join(''));const {records,...metadata}=p;pages.push({...metadata,count:records.length});
   if(p.finished)break;if(i===4095)throw Error('bounded drain exhausted');}}
  finally{await stream.close();}
  result.trace={loss:last.loss,pending_gpu:last.pending_gpu,records:total};await write('trace-pages.json',pages);stage({stage:'cold-trace-complete',records:total});
  if(mode==='correctness'&&branch==='warm_miss_pressure'){
   // This proof inspects already-drained observations, never adjusts quota or
   // forces eviction merely to make a correctness case appear exercised.
   const rows=(await fs.readFile(path.join(out,'raw-trace.jsonl'),'utf8')).trim().split('\n').filter(Boolean).map(JSON.parse);
   const pressure=rows.filter(r=>r.kind===10&&r.tag===43&&r.values[0]==='7'&&r.values[8]==='0'&&BigInt(r.values[4])<=BigInt(r.values[3])&&BigInt(r.values[2])<8n&&(BigInt(r.values[3])+BigInt(r.values[5])>BigInt(r.values[6])));
   result.correctness_pressure_proof={status:pressure.length?'OBSERVED_QUOTA_PRESSURE':'MISSING',record_sequences:pressure.map(r=>r.seq)};
   if(!pressure.length)result.correctness_status='BLOCKED_EVIDENCE';
  }
 }
 result.close=correctnessStages?await correctnessStages.run('close final',()=>page.evaluate(()=>ur0.close())):await page.evaluate(()=>ur0.close());if(!result.close.closed)throw Error('Runtime did not close');
 if(toggle!=='NORMAL'){
  const stream=await fs.open(path.join(out,'raw-cleanup.jsonl'),'wx');let total=0,last;
  try{for(let i=0;i<4096;++i){const p=await page.evaluate(()=>ur0.drain());last=p;total+=p.records.length;await stream.writeFile(p.records.map(r=>JSON.stringify(r)+'\n').join(''));if(p.finished)break;if(i===4095)throw Error('bounded cleanup drain exhausted');}}
  finally{await stream.close();}
  result.cleanup_trace={loss:last.loss,pending_gpu:last.pending_gpu,records:total,scope:'after measurement, during Runtime close; never a primary completion'};
 }
 result.status='OBSERVED';
}catch(error){result.status='BLOCKED_EVIDENCE';result.error=String(error.stack??error);process.exitCode=1;}
finally{
 if(phases.at(-1)?.phase!=='cleanup')arm('cleanup',15000);
 if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 result.artifacts_after=await artifactHashes();result.artifacts_stable=JSON.stringify(result.artifacts_before)===JSON.stringify(result.artifacts_after);
 result.source_after=source();result.source_stable=result.source_before.source_tree_sha256===result.source_after.source_tree_sha256;
 if(!result.artifacts_stable||!result.source_stable||result.errors.length){result.status='BLOCKED_EVIDENCE';process.exitCode=1;}
 result.phase_guards=phases;await write('worker-result.json',result);stage({stage:'complete',status:result.status,error:result.error,trace:result.trace});clearTimeout(phaseTimer);
}
