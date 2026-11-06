// Isolated, bounded worker. It owns its Chrome process, never a user's tab.
import {chromium} from '../../node_modules/playwright/index.mjs';
import {spawn,execFileSync} from 'node:child_process';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
import {createHash} from 'node:crypto';import {gzipSync} from 'node:zlib';
import {summarizePhase} from './navigation_metrics.mjs';import {summarizeCache,assessStage} from './complex_scale_metrics.mjs';
import {readFixtureModule,waitForNavigationProbe} from './fixture_modules.mjs';
const [sdkArg,outArg,kind,fixtureArg,dprArg='2',diagnosticsArg='required']=process.argv.slice(2),sdk=path.resolve(sdkArg),output=path.resolve(outArg),dpr=Number(dprArg);
if(!['transform','erase','text','table','resource','scale'].includes(kind)||![1,2].includes(dpr))throw Error('worker arguments');
const hash=b=>createHash('sha256').update(b).digest('hex');
async function hashes(root){const result={};for(const name of (await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,name);if((await fs.stat(p)).isFile())result[name]=hash(await fs.readFile(p));}return result;}
const identity=()=>JSON.parse(execFileSync('python3',['-c','import json;from verification.tools.g5_preflight import source_identity,ROOT;print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:8*1024*1024}));
let progressStage='initializing';
const emit=value=>{if(value.stage)progressStage=value.stage;console.log(JSON.stringify(value));};
await fs.mkdir(output,{recursive:true});const source=identity(),artifacts=await hashes(sdk),temp=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-g5-complex-'));
const result={stage:'browser_begin',status:'partial',scope:'root-sdk',kind,dpr,sourceBefore:source,artifacts,rows:[],errors:[],phases:[],browserEvents:[],consoleErrors:[]};
emit({stage:'browser_begin',kind,dpr});let chrome,browser,context,server,chromeLog,phase=null,closing=false;
const requireDiagnostics=diagnosticsArg!=='default';
const fixture=kind==='scale'?JSON.parse(await fs.readFile(fixtureArg,'utf8')):null;
try{
 const chromeProfile=path.join(temp,'profile');await fs.mkdir(chromeProfile);
 chromeLog=await fs.open(path.join(output,'chrome.stderr.log'),'w');
 chrome=spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--user-data-dir='+chromeProfile,'about:blank'],{stdio:['ignore',chromeLog.fd,chromeLog.fd]});
 chrome.on('exit',(code,signal)=>{result.chromeExit={code,signal,stage:progressStage,duringCleanup:closing};});
 let port;for(let i=0;i<450&&!port;++i){try{port=(await fs.readFile(path.join(chromeProfile,'DevToolsActivePort'),'utf8')).split('\n')[0];}catch{}if(!port)await new Promise(r=>setTimeout(r,100));}if(!port)throw Error('Chrome startup');
 browser=await chromium.connectOverCDP('http://127.0.0.1:'+port);result.browser=browser.version();
 browser.on('disconnected',()=>result.browserEvents.push({event:'disconnected',stage:progressStage,duringCleanup:closing}));
 context=await browser.newContext({viewport:{width:900,height:650},deviceScaleFactor:dpr});
 await context.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});
 const page=await context.newPage();page.setDefaultTimeout(60000);
 for(const event of ['crash','close'])page.on(event,()=>result.browserEvents.push({event,stage:progressStage,duringCleanup:closing}));
 page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{
  const text=m.text();if(m.type()==='error')result.consoleErrors.push({stage:progressStage,text});if(text.startsWith('G5_SAMPLE_MARK ')){const v=JSON.parse(text.slice(15));phase=v.kind==='end'?null:v.id;}
  for(const [prefix,kind] of [['AXIOM_NAVIGATION_TIMING ','navigation'],['AXIOM_FRAME_TIMING ','frame'],['AXIOM_SCENE_TIMING ','scene']])if(text.startsWith(prefix))result.rows.push({phase,kind,value:JSON.parse(text.slice(prefix.length))});
 });
 const local=['navigation_sampling.html','navigation_metrics.mjs','browser_metrics.mjs','editing_actions.mjs'];
 server=http.createServer(async(req,res)=>{try{
  const pathname=new URL(req.url,'http://local').pathname;
  const module=await readFixtureModule(pathname);if(module){res.setHeader('Content-Type','text/javascript');res.end(module);return;}
  const name=pathname.slice(1);let file;
  if(name==='fixture.axsnap'&&fixture)file=fixture.snapshot_path;
  else file=local.includes(name)?new URL('./'+name,import.meta.url):path.resolve(sdk,name);
  if(typeof file==='string'&&name!=='fixture.axsnap'&&!file.startsWith(sdk+path.sep))throw Error('path');
  res.setHeader('Content-Type',name.endsWith('.html')?'text/html':name.endsWith('.mjs')?'text/javascript':name.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 await page.goto('http://127.0.0.1:'+server.address().port+'/navigation_sampling.html');
 await waitForNavigationProbe(page);
 const checkpoint=async name=>{
  const r=await page.evaluate(()=>probe.checkpoint());const bytes=new Uint8Array(r.snapshot.bytes);await fs.writeFile(path.join(output,name+'.axsnap'),bytes);
  r.snapshot={...r.snapshot,bytes:undefined,sha256:hash(bytes),byteLength:bytes.length,path:name+'.axsnap'};return r;
 };
 if(kind==='scale'){
  emit({stage:'restore_begin',axis:fixture.axis,requested:fixture.count,snapshot_bytes:fixture.snapshot_bytes});
  result.setup=await page.evaluate(async meta=>probe.setupScale(meta,new Uint8Array(await (await fetch('/fixture.axsnap')).arrayBuffer())),fixture);
  result.objects=result.setup.state.objects;result.points=fixture.points;result.visible_points=fixture.visible_points;
  emit({stage:'restore_complete',objects:result.objects,points:result.points,loadMs:result.setup.loadMs});
  if(fixture.axis==='objects'){emit({stage:'scale_transform_begin',objects:result.objects});result.transform=await page.evaluate(()=>probe.scaleTransform());emit({stage:'scale_transform_complete',objects:result.objects});}
 }else{
  result.setup=await page.evaluate(()=>probe.setupEditing());result.before=await checkpoint('before');
  emit({stage:'edit_begin',kind});result.edit=await page.evaluate(kind=>probe.performEditing(kind),kind);result.after=await checkpoint('after');
  result.objects=result.after.state.objects;
  result.history={undo:[],redo:[]};
  for(let i=0;i<8;++i){await page.evaluate(()=>probe.historyStep('undo'));const c=await checkpoint('undo-'+i);result.history.undo.push(c);if(c.image.sha256===result.before.image.sha256)break;}
  for(let i=0;i<result.history.undo.length;++i){await page.evaluate(()=>probe.historyStep('redo'));result.history.redo.push(await checkpoint('redo-'+i));}
  result.history.checks={undoPixels:result.history.undo.at(-1).image.sha256===result.before.image.sha256,redoPixels:result.history.redo.at(-1).image.sha256===result.after.image.sha256};
  emit({stage:'edit_complete',kind,checks:{...result.edit.checks,...result.history.checks}});
 }
 for(const id of ['cold','warm']){
  emit({stage:'navigation_begin',id});const raw=await page.evaluate(id=>probe.measure(id,{mode:'wheel',burst:1,panOnly:true,panStep:1}),id);
  const rows=result.rows.filter(r=>r.phase===id);result.phases.push({id,raw,summary:summarizePhase(raw,rows.filter(r=>r.kind==='navigation').map(r=>r.value),rows.filter(r=>r.kind==='frame').map(r=>r.value))});
 }
 result.surface=await page.evaluate(()=>{const c=document.querySelector('#mount canvas');return {dpr:devicePixelRatio,logicalWidth:c.clientWidth,logicalHeight:c.clientHeight,physicalWidth:c.width,physicalHeight:c.height};});
 result.cache=summarizeCache(result.rows.filter(r=>r.kind==='navigation').map(r=>r.value));
 result.frameCounts=result.rows.filter(r=>r.kind==='frame'&&r.value.counts.encoded_bytes>0).map(r=>r.value.counts);
 result.canonicalPointsObserved=result.frameCounts.some(c=>Number.isSafeInteger(c.source_points))?Math.max(...result.frameCounts.map(c=>c.source_points??0)):null;
 result.close=await page.evaluate(()=>probe.close());
 result.checks={dpr:result.surface.dpr===dpr&&result.surface.physicalWidth===800*dpr&&result.surface.physicalHeight===500*dpr,
  diagnostics:!requireDiagnostics||(result.cache!==null&&result.canonicalPointsObserved!==null),noErrors:!result.errors.length&&!result.close.errors.length,closed:result.close.closed,
  navigation:result.phases.every(p=>['documentUnchanged','selectionUnchanged','operationUnchanged','cameraReturned'].every(k=>p.raw.checks[k])),
  receipt:result.phases.every(p=>p.summary.settle_ms!==null&&p.summary.settle_ms>=0),
  ...(kind==='scale'?{count:result.objects===fixture.objects,pointSource:!requireDiagnostics||fixture.axis!=='points'||result.canonicalPointsObserved===fixture.points,transform:!result.transform||BigInt(result.transform.after.generation)>BigInt(result.transform.before.generation)}:{...result.edit.checks,...result.history.checks,changedPixels:result.before.image.sha256!==result.after.image.sha256})};
 result.status=Object.values(result.checks).every(Boolean)?'completed':'failed';
 result.assessment=assessStage(result,{axis:fixture?.axis??'objects',count:fixture?.count??7,scope:'root-sdk',dpr});
 if(result.assessment.classification!=='MEASURED')result.status='failed';result.stage='browser_complete';
}catch(e){result.stage='browser_error';result.status='failed';result.error=String(e.stack||e);}
finally{
 closing=true;
 if(context)await context.close().catch(()=>{});if(browser)await browser.close().catch(()=>{});if(chrome&&chrome.exitCode===null){chrome.kill('SIGTERM');await Promise.race([new Promise(r=>chrome.once('exit',r)),new Promise(r=>setTimeout(r,500))]);}if(chromeLog)await chromeLog.close();if(server)await new Promise(r=>server.close(r));
 result.sourceAfter=identity();result.sourceStable=result.sourceAfter.tree_sha256===source.tree_sha256&&result.sourceAfter.commit===source.commit;
 result.artifactStable=JSON.stringify(artifacts)===JSON.stringify(await hashes(sdk));if(!result.sourceStable||!result.artifactStable)result.status='failed';
 await fs.writeFile(path.join(output,'observation.json.gz'),gzipSync(JSON.stringify(result)));await fs.rm(temp,{recursive:true,force:true,maxRetries:6,retryDelay:100});
 emit({stage:result.stage,status:result.status,scope:result.scope,objects:result.objects,points:result.points,checks:result.checks,error:result.error});
}
if(result.status!=='completed')process.exitCode=1;
