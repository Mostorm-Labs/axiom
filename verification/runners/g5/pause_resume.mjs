import {chromium} from '../../node_modules/playwright/index.mjs';
import {execFileSync}from'node:child_process';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';import {gzipSync}from'node:zlib';import{createHash}from'node:crypto';
import{readFixtureModule,waitForNavigationProbe}from'./fixture_modules.mjs';
import{summarizeInputFeedback,summarizeRuntimeSubmits}from'./feedback_metrics.mjs';
const [sdkArg,dest,kind='zoom']=process.argv.slice(2),sdk=path.resolve(sdkArg);await fs.mkdir(dest,{recursive:true});
const hash=b=>createHash('sha256').update(b).digest('hex');let html=await fs.readFile('verification/runners/g5/navigation_sampling.html','utf8');
const pinch=Number(process.env.G5_PINCH_DELTA??4);if(![1,4].includes(pinch)||!['zoom','pan'].includes(kind))throw Error('invalid gesture');
const userCorpus=process.env.G5_USER_CORPUS==='1';
const identity=()=>JSON.parse(execFileSync('python3.12',['-c','import json;from verification.tools.g5_preflight import source_identity,ROOT;print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:16*1024*1024}));
const artifacts=async()=>{const files={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())files[n]=hash(await fs.readFile(p));}return files;};
const sourceBefore=identity(),artifactBefore=await artifacts();
const result={sourceBefore,artifactBefore,pinchDelta:pinch,userCorpus,schema:'g5-pause-resume-diagnostic',kind,scope:'Synthetic root SDK wheel pinch;1000ms timer pause without waiting for exact convergence;not physical trackpad or formal performance acceptance',wasmSha256:hash(await fs.readFile(path.join(sdk,'axiom.wasm'))),rows:[],errors:[]};
const server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname;const m=await readFixtureModule(n);res.setHeader('Content-Type',n.endsWith('.html')?'text/html':n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');const file=path.resolve(sdk,'.'+n);if(!m&&n!=='/sample.html'&&!file.startsWith(sdk+path.sep))throw Error('path');res.end(m??(n==='/sample.html'?html:await fs.readFile(file)));}catch{res.statusCode=404;res.end();}});let browser,page;
try{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
 const context=await browser.newContext({viewport:{width:900,height:1050},deviceScaleFactor:2});
 await context.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});
 page=await context.newPage();page.setDefaultTimeout(60000);page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{if(m.text().startsWith('AXIOM_'))result.rows.push(m.text());});
 await page.goto('http://127.0.0.1:'+server.address().port+'/sample.html');await waitForNavigationProbe(page);
 result.setup=await page.evaluate(user=>probe.setup('mixed',false,'round-handdrawn',4,{width:636,height:923,points:2048,lines:5,shapes:user?3:2,shapeKinds:user?['rectangle','triangle','ellipse']:undefined,noisyImage:user,scribble:true,scribblePhase:.63,zoom:1.59,drawLast:true,uiReads:true}),userCorpus);
 await page.waitForFunction(()=>{const a=probe.failureState().activity;return a.pending===0&&a.at-a.lastSdkActivity>200;});
 if(process.env.G5_PROFILE==='1'){const cdp=await context.newCDPSession(page);await cdp.send('Profiler.enable');await cdp.send('Profiler.start');result.raw=await page.evaluate(({kind,pinch})=>probe.pauseResume(kind,{delta:pinch}),{kind,pinch});result.profile=(await cdp.send('Profiler.stop')).profile;result.intrusiveProfiler=true;}else result.raw=await page.evaluate(({kind,pinch})=>probe.pauseResume(kind,{delta:pinch}),{kind,pinch});
 const phase=result.raw,resume=phase.marks.find(x=>x.kind==='resume').at;
 const v={...phase,started:resume,inputEvents:phase.inputEvents.filter(e=>e.started>=resume),nativeRecords:phase.nativeRecords.filter(e=>e.at>=resume),colorOps:phase.colorOps.filter(e=>e.started>=resume)};
 result.fixtureImageBytes=await page.evaluate(()=>probe.fixtureImageBytes);result.feedback=summarizeInputFeedback(v);result.runtime=summarizeRuntimeSubmits(v);result.close=await page.evaluate(()=>probe.close());
 result.checks={noErrors:!result.errors.length&&!result.setup.errors.length&&!result.close.errors.length&&result.close.closed,documentUnchanged:phase.before.generation===phase.after.generation,operationUnchanged:phase.before.lastCommit.operationId===phase.after.lastCommit.operationId,selectionUnchanged:JSON.stringify(phase.before.selected)===JSON.stringify(phase.after.selected),eventCount:phase.inputEvents.length===144};
 result.status=Object.values(result.checks).every(Boolean)?'MEASURED':'BLOCKED_EVIDENCE';
 if(process.env.G5_EXPECT_RESPONSIVE==='1'){result.responseGuard={boundMs:150,actualMs:result.runtime.firstAcceptedSubmitMs,pass:result.runtime.status==='OBSERVED'&&result.runtime.firstAcceptedSubmitMs<=150};if(!result.responseGuard.pass){result.status='FAILED_RESPONSE_GUARD';}}
}catch(error){result.status='BLOCKED_EVIDENCE';result.error=String(error.stack??error);if(page)result.failure=await page.evaluate(()=>probe.failureState()).catch(e=>({error:String(e)}));}finally{
 if(browser)await browser.close();await new Promise(r=>server.close(r));result.sourceAfter=identity();result.sourceStable=result.sourceBefore.commit===result.sourceAfter.commit&&result.sourceBefore.tree_sha256===result.sourceAfter.tree_sha256;result.artifactStable=JSON.stringify(result.artifactBefore)===JSON.stringify(await artifacts());if(!result.sourceStable||!result.artifactStable)result.status='BLOCKED_EVIDENCE';await fs.writeFile(path.join(dest,'result.json.gz'),gzipSync(JSON.stringify(result)));console.log(JSON.stringify({kind,status:result.status,build:result.setup?.build?.sourceRevision,firstResponse:result.feedback?.firstContentResponseMs,maxGap:result.feedback?.contentGapMs.max,lastReceipt:result.feedback?.lastInputToLastCoarseReceiptMs,runtimeGap:result.runtime?.acceptedGapMs.max,error:result.error}));if(result.status!=='MEASURED')process.exitCode=1;
}
