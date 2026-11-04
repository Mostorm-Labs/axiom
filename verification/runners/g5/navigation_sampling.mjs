// Root-SDK browser measurement. No private WASM/HEAP/control hooks.
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {summarizePhase} from './navigation_metrics.mjs';
const root=process.cwd(),sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const repeats=Number(process.argv[4]??3),workloads=(process.argv[5]??'long-line,image,table,sticky').split(',');
const chrome='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const expectTiming=process.env.AXIOM_EXPECT_TIMING!=='0';
const html=await fs.readFile(new URL('./navigation_sampling.html',import.meta.url));
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
async function hashes(dir,prefix=''){
  const result={};for(const e of (await fs.readdir(path.join(dir,prefix),{withFileTypes:true})).sort((a,b)=>a.name.localeCompare(b.name))){
    const name=path.posix.join(prefix,e.name);if(e.isDirectory())Object.assign(result,await hashes(dir,name));else if(e.isFile())result[name]=hash(await fs.readFile(path.join(dir,name)));
  }return result;
}
const source=()=>JSON.parse(execFileSync('python3.12',['-c','import json; from verification.tools.g5_preflight import source_identity, ROOT; print(json.dumps(source_identity(ROOT)))'],{cwd:root,encoding:'utf8',maxBuffer:8*1024*1024}));
await fs.mkdir(output,{recursive:true});
const sourceBefore=source(),artifactsBefore=await hashes(sdk);
await fs.writeFile(path.join(output,'source-before.json'),JSON.stringify(sourceBefore,null,2));
const temp=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-g5-navigation-')),consumer=path.join(temp,'sdk');
await fs.cp(sdk,consumer,{recursive:true});
if(JSON.stringify(await hashes(consumer))!==JSON.stringify(artifactsBefore))throw Error('consumer copy mismatch');
const server=http.createServer(async(req,res)=>{try{
  const pathname=new URL(req.url,'http://localhost').pathname;
  const file=path.resolve(consumer,'.'+pathname);if(!file.startsWith(consumer+path.sep))throw Error('path');
  res.setHeader('Content-Type',pathname==='/sample.html'?'text/html':file.endsWith('.wasm')?'application/wasm':file.endsWith('.mjs')?'text/javascript':'application/octet-stream');
  res.end(pathname==='/sample.html'?html:await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
let browser;const cases=[];
try{
  browser=await chromium.launch({executablePath:chrome,headless:true});
  casesLoop: for(let repeat=0;repeat<repeats;++repeat)for(const workload of workloads)for(const selected of [false,true]){
    const id=`r${repeat}-${workload}-${selected?'selected':'unselected'}`,rows=[],logs=[],exceptions=[];
    const context=await browser.newContext({viewport:{width:900,height:650},deviceScaleFactor:2});
    await context.addInitScript(()=>{let n=1;crypto.randomUUID=()=>('00000000-0000-4000-8000-'+(n++).toString(16).padStart(12,'0'));});
    const page=await context.newPage();page.setDefaultTimeout(120000);
    let current=null,segment=null;
    page.on('console',message=>{
      const text=message.text();logs.push({phase:current,type:message.type(),text});
      if(text.startsWith('G5_SAMPLE_MARK ')){const m=JSON.parse(text.slice(15));current=m.kind==='end'?null:m.id;segment=m.kind==='begin'?'input':'settle';return;}
      for(const [prefix,kind] of [['AXIOM_NAVIGATION_TIMING ','navigation'],['AXIOM_FRAME_TIMING ','frame']])
        if(text.startsWith(prefix)){rows.push({phase:current,segment,kind,value:JSON.parse(text.slice(prefix.length))});break;}
    });
    page.on('pageerror',error=>exceptions.push(String(error)));
    const record={id,repeat,workload,selected};
    try{
      await page.goto(`http://127.0.0.1:${server.address().port}/sample.html`);
      await page.waitForFunction(()=>window.probe);
      record.setup=await page.evaluate(({workload,selected})=>probe.setup(workload,selected),{workload,selected});
      record.raw=await page.evaluate(()=>probe.run());
      record.summary={};
      for(const phase of ['cold','warm'])record.summary[phase]=summarizePhase(record.raw[phase],rows.filter(r=>r.phase===phase&&r.kind==='navigation').map(r=>r.value),rows.filter(r=>r.phase===phase&&r.kind==='frame').map(r=>r.value));
      for(const phase of ['cold','warm'])record.summary[phase].inputNavigation=summarizePhase(record.raw[phase],rows.filter(r=>r.phase===phase&&r.segment==='input'&&r.kind==='navigation').map(r=>r.value),[]).navigation;
      record.close=await page.evaluate(()=>probe.close());
      if(expectTiming&&!rows.some(r=>r.kind==='navigation'&&r.value.version===2))throw Error('navigation diagnostic records missing');
      if(!expectTiming&&rows.length)throw Error('disabled build emitted diagnostics');
      if(record.raw.errors.length||exceptions.length||!record.close.closed||Object.values(record.raw.cold.checks).some(v=>!v)||Object.values(record.raw.warm.checks).some(v=>!v))throw Error('runtime/receipt/selection invariant failed');
      for(const row of rows.filter(r=>r.kind==='frame')){
        const s=row.value.stages,sum=Object.values(s).reduce((n,x)=>n+x.exclusive_ns,0);
        if(sum!==s.attempt.inclusive_ns||Object.values(s).some(x=>x.exclusive_ns>x.inclusive_ns))throw Error('invalid diagnostic accounting');
      }
      record.status='MEASURED';
    }catch(error){record.status='BLOCKED_EVIDENCE';record.error=String(error);}
    record.rows=rows;record.exceptions=exceptions;
    await fs.writeFile(path.join(output,id+'.json'),JSON.stringify(record,null,2));
    await fs.writeFile(path.join(output,id+'.console.json'),JSON.stringify(logs,null,2));
    cases.push(record);console.log(JSON.stringify({id,status:record.status,error:record.error,cold:record.summary?.cold,warm:record.summary?.warm}));
    await context.close();
    if(record.status!=='MEASURED')break casesLoop;
  }
}finally{
  if(browser)await browser.close();await new Promise(r=>server.close(r));
  const sourceAfter=source(),artifactsAfter=await hashes(sdk);
  const sourceStable=sourceBefore.tree_sha256===sourceAfter.tree_sha256&&sourceBefore.commit===sourceAfter.commit;
  const artifactStable=JSON.stringify(artifactsBefore)===JSON.stringify(artifactsAfter);
  const result={schema:'g5-web-cold-warm-v1',status:sourceStable&&artifactStable&&cases.length===repeats*workloads.length*2&&cases.every(r=>r.status==='MEASURED')?'MEASURED':'BLOCKED_EVIDENCE',
    sourceStable,artifactStable,sourceCommit:sourceBefore.commit,sourceTreeSha256:sourceBefore.tree_sha256,
    browserVersion:browser?.version(),expectTiming,scope:'Headless Chrome, real WebGL2 renderer recorded per case; root SDK synthetic DOM wheel; cold Runtime cache vs repeated warm route; no physical/GPU execution/SLO claim',
    artifacts:artifactsBefore,cases:cases.map(({rows,raw,...r})=>r),host:{platform:os.platform(),arch:os.arch(),cpus:os.cpus().length,loadavg:os.loadavg(),memory:os.totalmem()}};
  await fs.writeFile(path.join(output,'source-after.json'),JSON.stringify(sourceAfter,null,2));
  await fs.writeFile(path.join(output,'summary.json'),JSON.stringify(result,null,2));
  const entries=await hashes(output);await fs.writeFile(path.join(output,'sha256.json'),JSON.stringify(entries,null,2));
  console.log(JSON.stringify({status:result.status,cases:cases.length,sourceStable,artifactStable}));
  if(result.status!=='MEASURED')process.exitCode=1;
  // Temp is a concrete directory created by this invocation, never a workspace.
  await fs.rm(temp,{recursive:true,force:true});
}
