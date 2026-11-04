// Root SDK investigation, kept separate from the frozen cold/warm matrix.
// Usage: node .../navigation_lifecycle.mjs SDK_DIR OUTPUT [idle_ms=60000] [cycles=6]
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {gzipSync} from 'node:zlib';
import {summarizePhase} from './navigation_metrics.mjs';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const idleMs=Number(process.argv[4]??60000),cycles=Number(process.argv[5]??6);
const selected=process.env.AXIOM_NAV_SELECTED!=='0',modes=(process.env.AXIOM_NAV_MODES??'wheel,touch').split(',');
const zoomDelta=Number(process.env.AXIOM_NAV_ZOOM_DELTA??2),qualityEvery=Number(process.env.AXIOM_NAV_QUALITY_EVERY??0),panStep=Number(process.env.AXIOM_NAV_PAN_STEP??8);
const editSamples=Number(process.env.AXIOM_NAV_EDIT_SAMPLES??0);
if(!Number.isSafeInteger(editSamples)||editSamples<0||editSamples>8)throw Error('invalid edit samples');
const fixtureOptions=JSON.parse(process.env.AXIOM_NAV_OPTIONS??'{}'),panOnly=process.env.AXIOM_NAV_PAN_ONLY!=='0';
if(!modes.length||modes.some(m=>!['wheel','touch'].includes(m)))throw Error('invalid modes');
if(!Number.isSafeInteger(idleMs)||idleMs<0||!Number.isSafeInteger(cycles)||cycles<1)throw Error('invalid duration/cycles');
const hash=b=>createHash('sha256').update(b).digest('hex');
async function hashes(dir,prefix=''){
  const result={};for(const e of (await fs.readdir(path.join(dir,prefix),{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){
    const name=path.posix.join(prefix,e.name);if(e.isDirectory())Object.assign(result,await hashes(dir,name));else if(e.isFile())result[name]=hash(await fs.readFile(path.join(dir,name)));
  }return result;
}
const identity=()=>JSON.parse(execFileSync('python3.12',['-c','import json; from verification.tools.g5_preflight import source_identity, ROOT; print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:8*1024*1024}));
await fs.mkdir(output,{recursive:true});
const before=identity(),artifacts=await hashes(sdk),temp=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-g5-lifecycle-'));
await fs.cp(sdk,temp,{recursive:true});
const server=http.createServer(async(req,res)=>{try{
  const pathname=new URL(req.url,'http://local').pathname;
  const fixture=['/navigation_sampling.html','/navigation_metrics.mjs','/browser_metrics.mjs'].includes(pathname);
  const file=fixture?new URL('.'+pathname,import.meta.url):path.resolve(temp,'.'+pathname);
  if(!fixture&&!file.startsWith(temp+path.sep))throw Error('path');
  res.setHeader('Content-Type',pathname.endsWith('.html')?'text/html':pathname.endsWith('.mjs')?'text/javascript':pathname.endsWith('.wasm')?'application/wasm':'application/octet-stream');
  res.end(await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const results=[];let browser;
try{
  browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
  for(const mode of modes){
    const context=await browser.newContext({viewport:{width:900,height:1050},deviceScaleFactor:2});
    await context.addInitScript(()=>{
      let uuid=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(uuid++).toString(16).padStart(12,'0'));
      const stats=window.lifecycleStats={requests:0,callbacks:0,callbackMs:0,maxCallbackMs:0,draws:0,events:[]};
      const raw=requestAnimationFrame;
      window.requestAnimationFrame=fn=>{++stats.requests;return raw(at=>{const start=performance.now();try{fn(at);}finally{const ms=performance.now()-start;++stats.callbacks;stats.callbackMs+=ms;stats.maxCallbackMs=Math.max(stats.maxCallbackMs,ms);}});};
      // Historical `draws` field counts color submissions, including blank
      // clears/blits. Idle must not conceal a clear-only render loop either.
      for(const name of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','clear','blitFramebuffer','clearBufferfv','clearBufferiv','clearBufferuiv']){
        const raw=WebGL2RenderingContext.prototype[name];WebGL2RenderingContext.prototype[name]=function(...a){
          const color=name==='clear'?!!(a[0]&this.COLOR_BUFFER_BIT):name==='blitFramebuffer'?!!(a[8]&this.COLOR_BUFFER_BIT):name.startsWith('clearBuffer')?a[0]===this.COLOR:true;
          if(color)++stats.draws;return raw.apply(this,a);
        };
      }
      for(const kind of ['visibilitychange','freeze','resume','webglcontextlost','webglcontextrestored'])document.addEventListener(kind,()=>stats.events.push({kind,at:performance.now(),visibility:document.visibilityState}),true);
    });
    const page=await context.newPage(),rows=[],logs=[],exceptions=[];let current=null,segment=null;
    page.setDefaultTimeout(180000);
    page.on('console',m=>{const text=m.text();logs.push({type:m.type(),text});
      if(text.startsWith('G5_SAMPLE_MARK ')){const mark=JSON.parse(text.slice(15));current=mark.kind==='end'?null:mark.id;segment=mark.kind==='begin'?'input':'settle';return;}
      for(const [prefix,kind] of [['AXIOM_NAVIGATION_TIMING ','navigation'],['AXIOM_FRAME_TIMING ','frame'],['AXIOM_SCENE_TIMING ','scene']])if(text.startsWith(prefix)){rows.push({phase:current,segment,kind,value:JSON.parse(text.slice(prefix.length))});break;}
    });
    page.on('pageerror',e=>exceptions.push(String(e)));
    const record={mode,selected,panOnly,options:{width:636,height:914,points:mode==='wheel'?4096:16384,lines:mode==='wheel'?6:2,span:570,lineSpacing:65,uiReads:true,...fixtureOptions},phases:[]};
    try{
      await page.goto(`http://127.0.0.1:${server.address().port}/navigation_sampling.html`);
      record.setup=await page.evaluate(({options,selected})=>probe.setup('mixed',selected,'round-handdrawn',4,options),{options:record.options,selected});
      record.edits=[];for(let i=0;i<editSamples;++i)record.edits.push(await page.evaluate(i=>probe.commitStroke(i),i));
      const measure=async(id,interrupt=false)=>{
        const options={mode,burst:mode==='wheel'?4:1,panOnly,zoomDelta,qualityEvery,panStep};
        const raw=await page.evaluate(({id,options,interrupt})=>interrupt?probe.interruptSelection(options):probe.measure(id,options),{id,options,interrupt});
        const phaseRows=rows.filter(r=>r.phase===raw.id);
        record.phases.push({id:raw.id,raw,summary:summarizePhase(raw,phaseRows.filter(r=>r.kind==='navigation').map(r=>r.value),phaseRows.filter(r=>r.kind==='frame').map(r=>r.value))});
        console.log(JSON.stringify({mode,id:raw.id,summary:record.phases.at(-1).summary}));
        if(qualityEvery)await page.screenshot({path:path.join(output,mode+'-'+raw.id+'.png')});
      };
      if(selected)await measure('interrupted-selection',true);
      for(let i=0;i<cycles;++i)await measure('repeat-'+i);
      const cdp=await context.newCDPSession(page);await cdp.send('Performance.enable');
      const snapshot=async()=>({browser:await cdp.send('Performance.getMetrics'),page:await page.evaluate(()=>({at:performance.now(),stats:structuredClone(lifecycleStats),visibility:document.visibilityState}))});
      record.sceneRows=rows.filter(r=>r.kind==='scene').map(r=>({phase:r.phase,value:r.value}));
      record.idleBefore=await snapshot();
      // Idle has no sampler RAF/timer in the page; observe SDK activity itself.
      await page.waitForTimeout(idleMs);record.idleAfter=await snapshot();
      await measure('after-idle');
      await cdp.send('Page.enable');await cdp.send('Page.setWebLifecycleState',{state:'frozen'});
      await page.waitForTimeout(5000);await cdp.send('Page.setWebLifecycleState',{state:'active'});
      await measure('after-freeze');
      record.lifecycle=await snapshot();record.uiReadTimes=await page.evaluate(()=>probe.uiReadTimes);
      record.close=await page.evaluate(()=>probe.close());
      record.checks={closed:record.close.closed,noErrors:!record.setup.errors.length&&!record.close.errors.length&&!exceptions.length,
        editsCommitted:record.edits.every(e=>Object.values(e.checks).every(Boolean)),
        idleRafStopped:record.idleBefore.page.stats.callbacks===record.idleAfter.page.stats.callbacks,
        idleDrawStopped:record.idleBefore.page.stats.draws===record.idleAfter.page.stats.draws,
        semanticAndCamera:record.phases.every(p=>['documentUnchanged','selectionUnchanged','operationUnchanged','cameraReturned'].every(k=>p.raw.checks[k])),
        receiptAfterInput:record.phases.every(p=>p.summary.settle_ms!==null&&p.summary.settle_ms>=0),
        nonblockingFences:record.phases.every(p=>p.summary.maximum_fence_timeout===0)};
      record.status=Object.values(record.checks).every(Boolean)?'MEASURED':'BLOCKED_EVIDENCE';
    }catch(error){record.error=String(error);record.status='BLOCKED_EVIDENCE';}
    record.exceptions=exceptions;record.rows=rows;results.push(record);
    await fs.writeFile(path.join(output,mode+'.json.gz'),gzipSync(JSON.stringify(record)));
    await fs.writeFile(path.join(output,mode+'.console.json.gz'),gzipSync(JSON.stringify(logs)));
    await context.close();
  }
}finally{
  if(browser)await browser.close();await new Promise(r=>server.close(r));await fs.rm(temp,{recursive:true,force:true});
  const after=identity(),sourceStable=before.tree_sha256===after.tree_sha256&&before.commit===after.commit,artifactStable=JSON.stringify(artifacts)===JSON.stringify(await hashes(sdk));
  const summary={schema:'g5-web-navigation-lifecycle-v1',status:sourceStable&&artifactStable&&results.length===modes.length&&results.every(r=>r.status==='MEASURED')?'MEASURED':'BLOCKED_EVIDENCE',
    sourceBefore:before,sourceAfter:after,sourceStable,artifactStable,artifacts,browserVersion:browser?.version(),idleMs,cycles,editSamples,selected,modes,zoomDelta,qualityEvery,panStep,intrusiveOptical:qualityEvery>0,
    scope:'Synthetic root SDK input; real Chrome WebGL2; callback wrappers forward calls. UI cost covers public readbacks, not full Demo DOM. CDP frozen/active is not physical sleep, GPU eviction or OS backgrounding. Post-input onFrame is coarse completion, not camera-qualified receipt. No physical input-to-photon or long-term leak claim.',
    results:results.map(({phases,rows,uiReadTimes,...r})=>({...r,uiReadMaxMs:Math.max(...(uiReadTimes??[])),phases:phases.map(p=>({id:p.id,summary:p.summary,checks:p.raw.checks,pixels:p.raw.pixels}))}))};
  await fs.writeFile(path.join(output,'summary.json'),JSON.stringify(summary,null,2));
  await fs.writeFile(path.join(output,'sha256.json'),JSON.stringify(await hashes(output),null,2));
  console.log(JSON.stringify({status:summary.status,sourceStable,artifactStable}));if(summary.status!=='MEASURED')process.exitCode=1;
}
