// Public-root-SDK engineering reproducer. No consumer private state or HEAP.
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {gzipSync} from 'node:zlib';
import {execFileSync} from 'node:child_process';
import {readFixtureModule,waitForNavigationProbe} from './fixture_modules.mjs';
import {summarizeInputFeedback,summarizeRuntimeSubmits} from './feedback_metrics.mjs';
const [sdkArg,outArg,dprArg='2',mode='candidate']=process.argv.slice(2),sdk=path.resolve(sdkArg),out=path.resolve(outArg),dpr=Number(dprArg);
if(![1,2,4].includes(dpr)||!['candidate','baseline'].includes(mode))throw Error('invalid corpus options');
const hash=b=>createHash('sha256').update(b).digest('hex');
const identity=()=>JSON.parse(execFileSync('python3.12',['-c','import json;from verification.tools.g5_preflight import source_identity,ROOT;print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:16*1024*1024}));
async function artifacts(){const v={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())v[n]=hash(await fs.readFile(p));}return v;}
// Same helper collector/observer as the navigation corpus; these methods call
// only the public SDK. Stream samples across real RAF opportunities.
const extension=String.raw`
Object.assign(window.probe,{
 async setupDense(){
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'99999999999999999999999999999999',permission:'localSingleUser',builtinFonts:false,onError:e=>errors.push(String(e)),onFrame:f=>frames.push({at:performance.now(),...f})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
  for(let line=0;line<6;++line){const prior=frames.length;
   for(let i=0;i<1500;++i){const a=i*Math.PI*2/55;pointer(i===0?'pointerdown':i===1499?'pointerup':'pointermove',256+140*Math.sin(a)+30*Math.sin(a*.19)+line*3,256+110*Math.cos(a*.67)+35*Math.sin(a*.13));if(i%4===0)await raf();}
   await settle(prior);
  }
  return {build:sdk.readBuildInfo(),state:sdk.readState(),inputSamples:9000,strokes:6,samplesPerRaf:4,timestampStepMs:4};
 },
 async denseSweep(n){
  const before=sdk.readState(),prior=frames.length;
  for(let i=0;i<59;++i){const a=i*Math.PI*2/17;pointer(i===0?'pointerdown':'pointermove',256+100*Math.sin(a)+n*3,256+90*Math.cos(a*.7));await raf();}
  const during=sdk.readState(),started=performance.now();pointer('pointerup',256,256);const upMs=performance.now()-started;
  const afterUp=sdk.readState();if(afterUp.activeContacts)throw Error('terminal stranded contact');
  await until(()=>sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation,'erase generation not presented');await settle(prior);
  const after=sdk.readState();return {n,before,during,afterUp,after,upMs,settledMs:performance.now()-started,
   checks:{transientOnly:before.generation===during.generation,terminalRetired:after.activeContacts===0,
    receipt:after.lastCommit.presentedGeneration===after.generation,objectsStable:before.objects===after.objects}};
 },
 async prepareDense(){await sdk.setEraser({mode:2,diameter:28});await sdk.selectTool('partialEraser');},
 async denseCancel(kind){
  const before=sdk.readState();pointer('pointerdown',250,220);pointer('pointermove',300,260);await raf();
  if(kind==='blur')window.dispatchEvent(new Event('blur'));
  else {const e=new PointerEvent('lostpointercapture',{pointerId:1,pointerType:'mouse',bubbles:true});Object.defineProperty(e,'timeStamp',{value:1000+(++sequence)*4});sdk.canvas.dispatchEvent(e);}
  for(let i=0;i<16;++i)await raf();const after=sdk.readState();
  return {before,after,checks:{terminalRetired:after.activeContacts===0,noMutation:after.generation===before.generation&&after.lastCommit.operationId===before.lastCommit.operationId}};
 },
 async denseRejectedRecovery(){
  const oldErrors=errors.splice(0);sdk.data.attach({capacity:1});await sdk.selectTool('ink');
  let prior=frames.length;pointer('pointerdown',600,50);pointer('pointermove',660,110);pointer('pointerup',660,110);await settle(prior);
  await sdk.setEraser({mode:2,diameter:28});await sdk.selectTool('partialEraser');
  const before=sdk.readState();pointer('pointerdown',620,40);pointer('pointermove',620,130);await raf();pointer('pointerup',620,130);await Promise.resolve();
  const rejected=sdk.readState(),failureErrors=errors.splice(0),packet=sdk.data.readNext();if(!packet)throw Error('capacity packet missing');sdk.data.release(packet);
  const checks={rejectedNotCommitted:rejected.generation===before.generation&&rejected.lastCommit.operationId===before.lastCommit.operationId,retired:rejected.activeContacts===0,errorReported:failureErrors.some(e=>e.includes('INPUT_REJECTED'))};
  pointer('pointerdown',625,40);await Promise.resolve();const retry=sdk.readState();checks.retryAccepted=retry.activeContacts===1&&errors.length===0;
  if(checks.retired&&checks.retryAccepted){pointer('pointermove',625,130);pointer('pointerup',625,130);for(let i=0;i<16;++i)await raf();const p=sdk.data.readNext();if(p)sdk.data.release(p);}
  const after=sdk.readState();checks.nextCommit=after.generation!==before.generation&&after.activeContacts===0;
  const retryErrors=errors.splice(0);return {before,rejected,retry,after,checks,oldErrors,failureErrors,retryErrors};
 },
 denseErrors(){return [...errors];}
});`;
await fs.mkdir(out,{recursive:true});
const result={schema:'g5-dense-erase-v1',scope:'Local synthetic engineering observation;not physical/scale/Gate qualification',mode,dpr,sourceBefore:identity(),artifactBefore:await artifacts(),errors:[],rows:[],sweeps:[]};
let browser,server,page;
try{
 const html=(await fs.readFile('verification/runners/g5/navigation_sampling.html','utf8')).replace('</script>',extension+'\n</script>');
 server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname,m=await readFixtureModule(n),p=path.resolve(sdk,'.'+n);if(!m&&n!=='/navigation_sampling.html'&&!p.startsWith(sdk+path.sep))throw Error('path');res.setHeader('Content-Type',n.endsWith('.html')?'text/html':n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(n==='/navigation_sampling.html'?html:m??await fs.readFile(p));}catch{res.statusCode=404;res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
 page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:dpr});await page.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{if(m.text().startsWith('AXIOM_'))result.rows.push({at:Date.now(),text:m.text()});});
 const cdp=await page.context().newCDPSession(page);await cdp.send('Performance.enable');const metrics=async()=>Object.fromEntries((await cdp.send('Performance.getMetrics')).metrics.filter(x=>['TaskDuration','ScriptDuration','JSHeapUsedSize','JSHeapTotalSize'].includes(x.name)).map(x=>[x.name,x.value]));
 await page.goto(`http://127.0.0.1:${server.address().port}/navigation_sampling.html`);await waitForNavigationProbe(page);
 console.log(JSON.stringify({stage:'setup'}));result.setup=await page.evaluate(()=>probe.setupDense());await page.evaluate(()=>probe.prepareDense());
 async function checkpoint(label){const c=await page.evaluate(()=>probe.checkpoint()),bytes=new Uint8Array(c.snapshot.bytes);await fs.writeFile(path.join(out,label+'.axsnap'),bytes);c.snapshot={...c.snapshot,bytes:undefined,sha256:hash(bytes),byteLength:bytes.length,path:label+'.axsnap'};return c;}
 result.initial=await checkpoint('initial');
 for(let n=0;n<6;++n){const before=await checkpoint('erase-'+n+'-before'),cpuBefore=await metrics(),s=await page.evaluate(n=>probe.denseSweep(n),n),cpuAfter=await metrics(),after=await checkpoint('erase-'+n+'-after');result.sweeps.push({...s,beforeCheckpoint:before,afterCheckpoint:after,cpuBefore,cpuAfter});console.log(JSON.stringify({stage:'erase',n,upMs:s.upMs,settledMs:s.settledMs,checks:s.checks}));if(!Object.values(s.checks).every(Boolean))throw Error('erase invariant');}
 const last=result.sweeps.at(-1);await page.evaluate(()=>probe.historyStep('undo'));result.undo=await checkpoint('undo');await page.evaluate(()=>probe.historyStep('redo'));result.redo=await checkpoint('redo');
 result.historyChecks={undo:result.undo.image.sha256===last.beforeCheckpoint.image.sha256,redo:result.redo.image.sha256===last.afterCheckpoint.image.sha256};if(!Object.values(result.historyChecks).every(Boolean))throw Error('history pixel roundtrip');
 result.noop=await page.evaluate(()=>probe.denseSweep(5));result.noopChecks={...result.noop.checks,noOperation:result.noop.before.generation===result.noop.after.generation&&result.noop.before.lastCommit.operationId===result.noop.after.lastCommit.operationId};if(!Object.values(result.noopChecks).every(Boolean))throw Error('repeated erase is not no-op');
 result.cancels=[];for(const kind of ['blur','capture']){const before=await checkpoint(kind+'-before'),r=await page.evaluate(kind=>probe.denseCancel(kind),kind),after=await checkpoint(kind+'-after');r.checks.pixels=before.image.sha256===after.image.sha256;result.cancels.push({kind,...r});if(!Object.values(r.checks).every(Boolean))throw Error('cancel '+kind);}
 result.navigation=await page.evaluate(()=>probe.measure('post-dense-erase',{mode:'wheel',burst:1}));result.feedback=summarizeInputFeedback(result.navigation);result.runtime=summarizeRuntimeSubmits(result.navigation);
 result.final=await checkpoint('final');result.recovery=await page.evaluate(()=>probe.denseRejectedRecovery());
 const fixed=Object.values(result.recovery.checks).every(Boolean)&&!result.recovery.oldErrors.length&&!result.recovery.retryErrors.length;
 if(mode==='candidate'&&!fixed)throw Error('rejected terminal recovery');
 result.inputErrors=await page.evaluate(()=>probe.denseErrors());if(result.inputErrors.length)throw Error('unexpected input errors');
 if(mode==='candidate')result.close=await page.evaluate(()=>probe.close());
 result.status=result.errors.length?'BLOCKED_EVIDENCE':fixed?'MEASURED':'REPRODUCED_REJECTION_BUG';
}catch(e){result.status='BLOCKED_EVIDENCE';result.error=String(e.stack??e);if(page)try{result.failure=await page.evaluate(()=>probe.failureState());}catch{}}
finally{
 if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.sourceAfter=identity();result.artifactAfter=await artifacts();result.sourceStable=result.sourceBefore.commit===result.sourceAfter.commit&&result.sourceBefore.tree_sha256===result.sourceAfter.tree_sha256;result.artifactStable=JSON.stringify(result.artifactBefore)===JSON.stringify(result.artifactAfter);if(!result.sourceStable||!result.artifactStable)result.status='BLOCKED_EVIDENCE';await fs.writeFile(path.join(out,'observation.json.gz'),gzipSync(JSON.stringify(result)));console.log(JSON.stringify({stage:'complete',status:result.status,error:result.error}));if(!['MEASURED','REPRODUCED_REJECTION_BUG'].includes(result.status))process.exitCode=1;
}
