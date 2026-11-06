// Integration baseline through the installed root SDK, never a consumer API.
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {gzipSync} from 'node:zlib';
import {readFixtureModule,waitForNavigationProbe} from './fixture_modules.mjs';
import {summarizeInputFeedback,summarizeRuntimeSubmits} from './feedback_metrics.mjs';
import {summarizeCache} from './complex_scale_metrics.mjs';

const [sdkArg,outArg,dprArg='2']=process.argv.slice(2),sdk=path.resolve(sdkArg),out=path.resolve(outArg),dpr=Number(dprArg);
if(![1,2,4].includes(dpr))throw Error('DPR must be1,2 or corroborative4');
const hash=b=>createHash('sha256').update(b).digest('hex');
const identity=()=>JSON.parse(execFileSync('python3.12',['-c','import json;from verification.tools.g5_preflight import source_identity,ROOT;print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:16*1024*1024}));
async function artifacts(){const v={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())v[n]=hash(await fs.readFile(p));}return v;}
await fs.mkdir(out,{recursive:true});
const result={schema:'g5-post-edit-navigation-v1',scope:'Local synthetic root SDK engineering baseline;not physical/statistical/Gate qualification',dpr,sourceBefore:identity(),artifactBefore:await artifacts(),errors:[],rows:[],edits:[],navigation:[]};
result.contextRestoreRequested=process.env.G5_CONTEXT_RESTORE==='1';
let browser,server,page;
const stage=(name,extra={})=>console.log(JSON.stringify({stage:name,...extra}));
function feedback(raw,resumed=false){
  const at=resumed?raw.marks.find(x=>x.kind==='resume').at:raw.started;
  const p={...raw,started:at,inputEvents:raw.inputEvents.filter(e=>e.started>=at),nativeRecords:raw.nativeRecords.filter(e=>e.at>=at),colorOps:raw.colorOps.filter(e=>e.started>=at)};
  return {feedback:summarizeInputFeedback(p),runtime:summarizeRuntimeSubmits(p)};
}
function unchanged(a,b){return a.generation===b.generation&&a.objects===b.objects&&a.lastCommit.operationId===b.lastCommit.operationId;}
try{
  server=http.createServer(async(req,res)=>{try{
    const n=new URL(req.url,'http://local').pathname,module=await readFixtureModule(n);
    const f=n==='/navigation_sampling.html'?path.resolve('verification/runners/g5/navigation_sampling.html'):path.resolve(sdk,'.'+n);
    if(!module&&n!=='/navigation_sampling.html'&&!f.startsWith(sdk+path.sep))throw Error('path');
    res.setHeader('Content-Type',n.endsWith('.html')?'text/html':n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(module??await fs.readFile(f));
  }catch{res.statusCode=404;res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
  const context=await browser.newContext({viewport:{width:900,height:650},deviceScaleFactor:dpr});
  await context.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});
  page=await context.newPage();page.setDefaultTimeout(60000);page.on('pageerror',e=>result.errors.push(String(e)));
  page.on('console',m=>{if(m.text().startsWith('AXIOM_'))result.rows.push({at:Date.now(),text:m.text()});});
  await page.goto(`http://127.0.0.1:${server.address().port}/navigation_sampling.html`);await waitForNavigationProbe(page);
  stage('setup');result.setup=await page.evaluate(()=>probe.setupEditing({largeImage:true}));
  async function checkpoint(label){
    const c=await page.evaluate(()=>probe.checkpoint()),bytes=new Uint8Array(c.snapshot.bytes);
    await fs.writeFile(path.join(out,label+'.axsnap'),bytes);
    c.snapshot={...c.snapshot,bytes:undefined,sha256:hash(bytes),byteLength:bytes.length,path:label+'.axsnap'};return c;
  }
  result.initial=await checkpoint('initial');
  for(const [i,kind]of ['copy','transform','erase','erase','erase','text','table','resource'].entries()){
    const id=`${i}-${kind}`;stage('edit',{id});
    const before=await checkpoint(id+'-before');
    const edit=await page.evaluate(({kind,i})=>probe.performEditing(kind,{eraseX:390+(i-2)*40}),{kind,i});
    const after=await checkpoint(id+'-after');
    const undo=[];for(let n=0;n<8;++n){await page.evaluate(()=>probe.historyStep('undo'));const c=await checkpoint(id+'-undo-'+n);undo.push(c);if(c.image.sha256===before.image.sha256)break;}
    const redo=[];for(let n=0;n<undo.length;++n){await page.evaluate(()=>probe.historyStep('redo'));redo.push(await checkpoint(id+'-redo-'+n));}
    const checks={...edit.checks,changedPixels:before.image.sha256!==after.image.sha256,undoPixels:undo.at(-1).image.sha256===before.image.sha256,redoPixels:redo.at(-1).image.sha256===after.image.sha256};
    result.edits.push({id,edit,before,after,undo,redo,checks});
    if(!Object.values(checks).every(Boolean))throw Error('edit/history checks '+id);
    stage('navigation',{id});const raw=await page.evaluate(id=>probe.measure(id,{mode:'wheel',burst:1,panOnly:false}),id);
    if(!unchanged(raw.before,raw.after)||!raw.checks.selectionUnchanged||!raw.checks.cameraReturned)throw Error('navigation changed state '+id);
    result.navigation.push({id,raw,...feedback(raw)});
  }
  for(const selected of [true,false]){
    await page.evaluate(selected=>probe.prepareEditingNavigation(selected),selected);
    for(const kind of ['zoom','pan']){
      const id=`idle-${selected?'selected':'unselected'}-${kind}`;stage('idle-reentry',{id});
      const before=await checkpoint(id+'-before');const raw=await page.evaluate(kind=>probe.pauseResume(kind,{delta:1,pauseMs:1000}),kind);
      await page.evaluate(camera=>probe.restoreEditingCamera(camera),before.state.camera);
      const after=await checkpoint(id+'-after');
      const checks={documentUnchanged:unchanged(raw.before,raw.after),selectionUnchanged:JSON.stringify(raw.before.selected)===JSON.stringify(raw.after.selected),pixels:before.image.sha256===after.image.sha256,selected:raw.before.selected.length===(selected?1:0)};
      result.navigation.push({id,raw,checks,...feedback(raw,true)});if(!Object.values(checks).every(Boolean))throw Error('idle reentry '+id);
    }
  }
  if(result.contextRestoreRequested){
    stage('context-restore');const before=await checkpoint('restore-before');
    const surfaceDiagnostic=()=>{for(const r of [...result.rows].reverse())if(r.text.startsWith('AXIOM_FRAME_TIMING ')){const f=JSON.parse(r.text.slice(19));if(typeof f.surface==='string')return f.surface;}return null;};
    const beforeSurface=surfaceDiagnostic();
    result.restore=await page.evaluate(()=>probe.restoreEditingContext());
    if(!result.restore.available)throw Error(result.restore.reason);
    const after=await checkpoint('restore-after');result.restore.checks={unavailableWhenLost:!result.restore.lost.surface.available,recovered:result.restore.after.surface.available,documentUnchanged:unchanged(before.state,after.state),pixels:before.image.sha256===after.image.sha256};
    result.restore.beforeCheckpoint=before;result.restore.afterCheckpoint=after;
    const afterSurface=surfaceDiagnostic();result.restore.surfaceDiagnostic={before:beforeSurface,after:afterSurface};
    if(beforeSurface!==null&&afterSurface!==null&&BigInt(afterSurface)<=BigInt(beforeSurface))throw Error('surface generation did not advance');
    if(!Object.values(result.restore.checks).every(Boolean))throw Error('context restore correctness');
    const raw=await page.evaluate(()=>probe.measure('after-context-restore',{mode:'wheel',burst:1,panOnly:false}));
    if(!unchanged(raw.before,raw.after)||!raw.checks.selectionUnchanged||!raw.checks.cameraReturned)throw Error('restored navigation changed state');
    result.navigation.push({id:'after-context-restore',raw,...feedback(raw)});
  }
  result.cache=summarizeCache(result.rows.filter(r=>r.text.startsWith('AXIOM_NAVIGATION_TIMING ')).map(r=>JSON.parse(r.text.slice(24))));
  result.final=await checkpoint('final');result.close=await page.evaluate(()=>probe.close());
  result.status=!result.errors.length&&result.close.closed&&!result.close.errors.length?'MEASURED':'BLOCKED_EVIDENCE';
}catch(error){result.status='BLOCKED_EVIDENCE';result.error=String(error.stack??error);}
finally{
  if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
  result.sourceAfter=identity();result.artifactAfter=await artifacts();
  result.sourceStable=result.sourceBefore.commit===result.sourceAfter.commit&&result.sourceBefore.tree_sha256===result.sourceAfter.tree_sha256;
  result.artifactStable=JSON.stringify(result.artifactBefore)===JSON.stringify(result.artifactAfter);
  if(!result.sourceStable||!result.artifactStable)result.status='BLOCKED_EVIDENCE';
  await fs.writeFile(path.join(out,'observation.json.gz'),gzipSync(JSON.stringify(result)));
  stage('complete',{status:result.status,edits:result.edits.length,phases:result.navigation.length,error:result.error});
  if(result.status!=='MEASURED')process.exitCode=1;
}
