// Actual Chrome + matching root SDK consumer. Not a physical-device Gate runner.
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {spawn} from 'node:child_process';
import {stroke, statistics, accumulationSummary} from './browser_metrics.mjs';

const [sdkArg, chromeArg, outputArg, ...options] = process.argv.slice(2);
const option = (name, fallback) => {const i=options.indexOf(name);return i<0?fallback:options[i+1];};
const renderer = option('--renderer','default');
const counts = option('--samples','64,256,1024,4096').split(',').map(Number);
const repeats = Number(option('--repeats','5'));
const profile=option('--profile','vector-solid-v1');
const accumulated=Number(option('--accumulate','0'));
if(!['default','swiftshader'].includes(renderer)||!Number.isInteger(repeats)||repeats<1||repeats>100||counts.some(n=>!Number.isInteger(n)||n<2||n>65536))throw Error('invalid workload configuration');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-g5-perf-'));
const files=path.join(root,'consumer'), output=path.resolve(outputArg);
let browser,server,socket;
const observation={schema_version:'g5-preparation-v1',classification:'BLOCKED_EVIDENCE',
  physical:false,official_gate_pass_claimed:false,renderer_requested:renderer,checks:{},sdk_files:{},exceptions:[]};
try {
  await fs.mkdir(files);
  for(const name of ['axiom.mjs','axiom.d.mts','package.json','axiom.wasm','axiom.data','internal/axiom.mjs','pointer_input.mjs','text_input.mjs']) {
    const dest=path.join(files,name);await fs.mkdir(path.dirname(dest),{recursive:true});
    await fs.copyFile(path.join(path.resolve(sdkArg),name),dest);observation.sdk_files[name]=hash(await fs.readFile(dest));
  }
  // Current Web SDK roots keep the shared facade under api/**. Preserve the
  // package layout instead of importing private deep paths or silently
  // falling back to an older wrapper shape.
  const apiRoot=path.join(path.resolve(sdkArg),'api');
  await fs.cp(apiRoot,path.join(files,'api'),{recursive:true});
  const apiEntries=await fs.readdir(apiRoot,{recursive:true});
  for(const entry of apiEntries){
    const source=path.join(apiRoot,entry);
    const stat=await fs.stat(source);
    if(stat.isFile()) observation.sdk_files[path.posix.join('api',entry.split(path.sep).join('/'))]=hash(await fs.readFile(source));
  }
  for(const [source,name] of [['browser.html','index.html'],['browser_metrics.mjs','browser_metrics.mjs']])
    await fs.copyFile(new URL(source,import.meta.url),path.join(files,name));
  server=http.createServer(async(req,res)=>{
    try {
      const url=new URL(req.url,'http://localhost');
      const file=path.resolve(files,'.'+decodeURIComponent(url.pathname).replace(/\/$/,'/index.html'));
      if(!file.startsWith(files+path.sep))throw Error('invalid path');
      res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.html')?'text/html':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');
      res.end(await fs.readFile(file));
    }catch{res.statusCode=404;res.end();}
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const chromeProfile=path.join(root,'profile');await fs.mkdir(chromeProfile);
  const chromeArgs=['--headless=new','--no-first-run','--no-default-browser-check','--remote-debugging-port=0',`--user-data-dir=${chromeProfile}`];
  if(renderer==='swiftshader')chromeArgs.push('--enable-unsafe-swiftshader','--use-angle=swiftshader');
  chromeArgs.push('about:blank');
  observation.browser_args=chromeArgs.map(s=>s.startsWith('--user-data-dir=')?'--user-data-dir=<isolated temporary profile>':s);
  browser=spawn(path.resolve(chromeArg),chromeArgs,{stdio:['ignore','ignore','pipe']});
  let stderr='',spawnError;browser.stderr.on('data',b=>{stderr=(stderr+b).slice(-16384);});
  browser.on('error',e=>{spawnError=String(e);});
  let port;const deadline=Date.now()+45000;
  while(!port){
    if(spawnError||browser.exitCode!==null||Date.now()>deadline)throw Error(`Chrome startup failed: ${spawnError||stderr}`);
    try{port=(await fs.readFile(path.join(chromeProfile,'DevToolsActivePort'),'utf8')).split('\n')[0];}catch{}
    if(!port)await delay(50);
  }
  const page=await fetch(`http://127.0.0.1:${port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());
  socket=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  let serial=0;const pending=new Map();
  socket.addEventListener('message',event=>{
    const msg=JSON.parse(event.data);
    if(msg.id){const p=pending.get(msg.id);if(!p)return;pending.delete(msg.id);clearTimeout(p.timer);msg.error?p.reject(Error(JSON.stringify(msg.error))):p.resolve(msg.result);}
    else if(msg.method==='Runtime.exceptionThrown')observation.exceptions.push(msg.params);
  });
  const send=(method,params={})=>new Promise((resolve,reject)=>{
    const id=++serial,timer=setTimeout(()=>{pending.delete(id);reject(Error(`CDP timeout: ${method}`));},90000);
    pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));
  });
  const evaluate=async expression=>{
    const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});
    if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;
  };
  observation.browser_version=await send('Browser.getVersion');
  await send('Runtime.enable');await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride',{width:900,height:650,deviceScaleFactor:1,mobile:false});
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/?profile=${encodeURIComponent(profile)}`});
  for(let n=0;!await evaluate('Boolean(window.probe)');++n){if(n>600)throw Error('consumer initialization timeout');await delay(50);}
  await evaluate('probe.ready');observation.environment=await evaluate('probe.info()');
  const draw=async(count,label,warmup=false)=>{
    const origin=await evaluate(`probe.begin(${count},${JSON.stringify(label)},${warmup})`);
    const points=stroke(count,42);const cdpDurations=[];
    // No sample dropping, resampling, or synthetic JS pointer bypass. CDP
    // native mouse input exercises capture, DOM adapter and root SDK transport.
    for(let i=0;i<points.length;++i){
      const p=points[i],start=performance.now();
      await send('Input.dispatchMouseEvent',{type:p.phase==='down'?'mousePressed':p.phase==='up'?'mouseReleased':'mouseMoved',
        x:origin.x+p.x,y:origin.y+p.y,button:'left',buttons:p.phase==='up'?0:1,clickCount:p.phase==='move'?0:1});
      cdpDurations.push(performance.now()-start);
      if(i%8===7&&i<count-1)await evaluate('probe.waitFrame()');
    }
    const result=await evaluate('probe.end()');result.cdp_roundtrip_ms=cdpDurations;
    return result;
  };
  observation.workload={seed:42,counts,repeats,profile,accumulated,source:'native Chrome CDP mouse events',
    pacing:'await each CDP acknowledgement; one RAF per eight samples',
    timestamp_policy:'native browser event timestamps (fixture ordering only); 4ms ideal corpus interval is not imposed',
    pressure:'mouse pressure unavailable; selected package simulated pressure',warmup_samples:32};
  observation.warmup=await draw(32,'warmup',true);
  observation.runs=[];
  if(accumulated){
    if(!Number.isInteger(accumulated)||accumulated<1||accumulated>1024)throw Error('invalid accumulation count');
    for(let i=0;i<accumulated;++i){
      observation.runs.push(await draw(counts[0],`stroke-${i+1}`));
      if(i%8===7)console.log(JSON.stringify({progress:i+1,objects:observation.runs.at(-1).after.objects}));
    }
  }else for(const count of counts)for(let repeat=0;repeat<repeats;++repeat)
    observation.runs.push(await draw(count,`${count}-${repeat}`));
  observation.idle=await evaluate('probe.idle()');
  const consumer=await evaluate('probe.result()');observation.runtime_errors=consumer.errors;
  observation.summary=counts.map(count=>{
    const runs=observation.runs.filter(r=>r.count===count);
    return {samples:count,repeats:runs.length,
      pointer_move_dispatch_ms:statistics(runs.flatMap(r=>r.events.filter(e=>e.type==='pointermove').map(e=>e.duration))),
      pointer_up_commit_dispatch_ms:statistics(runs.flatMap(r=>r.events.filter(e=>e.type==='pointerup').map(e=>e.duration))),
      raf_interval_ms:statistics(runs.flatMap(r=>r.raf.slice(1).map((t,i)=>t-r.raf[i]))),
      up_to_canonical_callback_ms:statistics(runs.map(r=>r.metrics.up_to_canonical_callback_ms).filter(x=>x!==null)),
      total_long_tasks:runs.reduce((n,r)=>n+(r.longTasks?.length||0),0)};
  });
  if(accumulated)observation.accumulation=accumulationSummary(observation.runs);
  observation.checks={all_strokes_valid:[observation.warmup,...observation.runs].every(r=>Object.values(r.checks).every(Boolean)),
    no_runtime_errors:consumer.errors.length===0,no_uncaught_exceptions:observation.exceptions.length===0,
    idle_canonical_callback_stable:observation.idle.canonical_callback_count===0,
    independent_targets:observation.environment.targets.length===2&&observation.environment.targets[0].id!==observation.environment.targets[1].id};
  // Pixel capture is intentionally outside every timed stroke and idle window.
  const screenshot=await send('Page.captureScreenshot',{format:'png'});
  await fs.mkdir(path.dirname(output),{recursive:true});
  await fs.writeFile(output.replace(/\.json$/, '')+'-after.png',Buffer.from(screenshot.data,'base64'));
  observation.checks.clean_close=await evaluate('probe.close()');
  observation.classification=Object.values(observation.checks).every(Boolean)?'MEASURED':'BLOCKED_EVIDENCE';
}catch(e){observation.error=String(e.stack||e);}
finally{
  socket?.close();
  if(browser){const exited=new Promise(resolve=>browser.exitCode!==null||browser.signalCode!==null?resolve():browser.once('exit',resolve));browser.kill();await Promise.race([exited,delay(5000)]);}
  if(server)await new Promise(resolve=>server.close(resolve));
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(observation,null,2)+'\n');
  console.log(JSON.stringify({classification:observation.classification,checks:observation.checks,error:observation.error,output}));
  await fs.rm(root,{recursive:true,force:true,maxRetries:6,retryDelay:100});
}
process.exitCode=observation.classification==='MEASURED'?0:2;
