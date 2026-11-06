import {chromium} from '../verification/node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';import {gzipSync}from'node:zlib';import{createHash}from'node:crypto';
import{readFixtureModule,waitForNavigationProbe}from'../verification/runners/g5/fixture_modules.mjs';
import{summarizeInputFeedback,summarizeRuntimeSubmits}from'../verification/runners/g5/feedback_metrics.mjs';
const [sdkArg,dest,kind='zoom']=process.argv.slice(2),sdk=path.resolve(sdkArg);await fs.mkdir(dest,{recursive:true});
const hash=b=>createHash('sha256').update(b).digest('hex');let html=await fs.readFile('verification/runners/g5/navigation_sampling.html','utf8');
html=html.replace('window.probe={',`window.probe={
 async pauseResume(kind){
  const before=sdk.readState(),prior=frames.length;
  const value={id:'pause-resume-'+kind,started:performance.now(),displayTimes:[],draws:0,colorClears:0,fences:[],longTasks:[],samples:[],marks:[],before};
  active=value;feedbackObserver.begin(value);mark('begin',value.id);
  const wheel=(ctrl,dy,dx=0)=>feedbackObserver.dispatch(sdk.canvas,new WheelEvent('wheel',{clientX:318,clientY:457,ctrlKey:ctrl,deltaX:dx,deltaY:dy,bubbles:true,cancelable:true}));
  value.marks.push({kind:'zoom-in',at:performance.now()});
  for(let i=0;i<18;++i){for(let b=0;b<4;++b)wheel(true,-4);await raf();}
  value.marks.push({kind:'pause-start',at:performance.now()});
  await new Promise(r=>setTimeout(r,1000));
  value.marks.push({kind:'resume',at:performance.now()});
  for(let i=0;i<18;++i){for(let b=0;b<4;++b)wheel(kind==='zoom',kind==='zoom'?4:2,kind==='zoom'?0:4);await raf();}
  value.inputEnded=performance.now();value.marks.push({kind:'input-end',at:value.inputEnded});
  await until(()=>frames.slice(prior).some(f=>f.at>=value.inputEnded),'post-resume receipt timeout');
  const at=performance.now();for(;;){const a=feedbackObserver.activity();if(a.pending===0&&a.at-a.lastSdkActivity>=200){value.quiescence=a;break;}if(performance.now()-at>20000)throw Error('quiescence timeout');await new Promise(r=>setTimeout(r,25));}
  value.ended=performance.now();feedbackObserver.end(value);active=null;mark('end',value.id);value.after=sdk.readState();value.receipts=frames.slice(prior);value.pixels=await pixels();return value;
 },`);
const pinch=Number(process.env.G5_PINCH_DELTA??4);if(pinch!==4)html=html.replace('wheel(true,-4)','wheel(true,-'+pinch+')').replace("kind==='zoom'?4:2","kind==='zoom'?"+pinch+":2");
const result={pinchDelta:pinch,schema:'g5-pause-resume-diagnostic',kind,scope:'Synthetic root SDK wheel pinch;1000ms timer pause without waiting for exact convergence;not physical trackpad or formal performance acceptance',wasmSha256:hash(await fs.readFile(path.join(sdk,'axiom.wasm'))),rows:[],errors:[]};
const server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname;const m=await readFixtureModule(n);res.setHeader('Content-Type',n.endsWith('.html')?'text/html':n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(m??(n==='/sample.html'?html:await fs.readFile(path.join(sdk,n))));}catch{res.statusCode=404;res.end();}});let browser;
try{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
 const context=await browser.newContext({viewport:{width:900,height:1050},deviceScaleFactor:2});
 await context.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});
 const page=await context.newPage();page.setDefaultTimeout(60000);page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{if(m.text().startsWith('AXIOM_'))result.rows.push(m.text());});
 await page.goto('http://127.0.0.1:'+server.address().port+'/sample.html');await waitForNavigationProbe(page);
 result.setup=await page.evaluate(()=>probe.setup('mixed',false,'round-handdrawn',4,{width:636,height:923,points:2048,lines:5,shapes:2,scribble:true,scribblePhase:.63,zoom:1.59,drawLast:true,uiReads:true}));
 await page.waitForFunction(()=>{const a=probe.failureState().activity;return a.pending===0&&a.at-a.lastSdkActivity>200;});
 if(process.env.G5_PROFILE==='1'){const cdp=await context.newCDPSession(page);await cdp.send('Profiler.enable');await cdp.send('Profiler.start');result.raw=await page.evaluate(k=>probe.pauseResume(k),kind);result.profile=(await cdp.send('Profiler.stop')).profile;result.intrusiveProfiler=true;}else result.raw=await page.evaluate(k=>probe.pauseResume(k),kind);
 const phase=result.raw,resume=phase.marks.find(x=>x.kind==='resume').at;
 const v={...phase,started:resume,inputEvents:phase.inputEvents.filter(e=>e.started>=resume),nativeRecords:phase.nativeRecords.filter(e=>e.at>=resume),colorOps:phase.colorOps.filter(e=>e.started>=resume)};
 result.feedback=summarizeInputFeedback(v);result.runtime=summarizeRuntimeSubmits(v);result.close=await page.evaluate(()=>probe.close());
 result.checks={noErrors:!result.errors.length&&!result.setup.errors.length&&result.close.closed,documentUnchanged:phase.before.generation===phase.after.generation,operationUnchanged:phase.before.lastCommit.operationId===phase.after.lastCommit.operationId,selectionUnchanged:JSON.stringify(phase.before.selected)===JSON.stringify(phase.after.selected),eventCount:phase.inputEvents.length===144};
 result.status=Object.values(result.checks).every(Boolean)?'MEASURED':'BLOCKED_EVIDENCE';
 if(process.env.G5_EXPECT_RESPONSIVE==='1'){result.responseGuard={boundMs:150,actualMs:result.runtime.firstAcceptedSubmitMs,pass:result.runtime.status==='OBSERVED'&&result.runtime.firstAcceptedSubmitMs<=150};if(!result.responseGuard.pass){result.status='FAILED_RESPONSE_GUARD';}}
}catch(error){result.status='BLOCKED_EVIDENCE';result.error=String(error.stack??error);}finally{
 if(browser)await browser.close();await new Promise(r=>server.close(r));await fs.writeFile(path.join(dest,'result.json.gz'),gzipSync(JSON.stringify(result)));console.log(JSON.stringify({kind,status:result.status,build:result.setup?.build?.sourceRevision,firstResponse:result.feedback?.firstContentResponseMs,maxGap:result.feedback?.contentGapMs.max,lastReceipt:result.feedback?.lastInputToLastCoarseReceiptMs,runtimeGap:result.runtime?.acceptedGapMs.max,error:result.error}));if(result.status!=='MEASURED')process.exitCode=1;
}
