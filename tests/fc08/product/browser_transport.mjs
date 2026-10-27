// Actual Chrome/WASM transport only. No hooks, synthetic SUT or expected values.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';

const args=Object.fromEntries(process.argv.slice(2).reduce((a,v,i,all)=>i%2?a:[...a,[v.replace(/^--/,''),all[i+1]]],[]));
const output=path.resolve(args.output),programs=path.resolve(args.build,'tests/fc08');
await fs.mkdir(output,{recursive:false});
const requests=JSON.parse(await fs.readFile(args.requests,'utf8'));
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const profile=path.resolve('out/fc08-bootstrap-003',`chrome-${Date.now()}`);
await fs.mkdir(profile,{recursive:true});
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
class CDP {
  constructor(ws){this.ws=ws;this.n=0;this.pending=new Map();ws.addEventListener('message',event=>{const value=JSON.parse(event.data);if(!value.id)return;const p=this.pending.get(value.id);if(!p)return;clearTimeout(p.timer);this.pending.delete(value.id);value.error?p.reject(new Error(JSON.stringify(value.error))):p.resolve(value.result);});}
  static async open(url){const ws=new WebSocket(url);await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});return new CDP(ws);}
  send(method,params={}){const id=++this.n;return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{this.pending.delete(id);reject(new Error('Actual browser timeout: '+method));},60000);this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));});}
  close(){this.ws.close();}
}
const html=`<!doctype html><meta charset="utf-8"><title>FC08 actual baseline runner</title>
<script src="/wasm/axiom_fc08_baseline_probe.js"></script><script src="/wasm/axiom_fc08_headless_probe.js"></script>
<script>window.runActual=async(program,argv,url)=>{
 const stdout=[],stderr=[];const factory=program==='baseline'?FC08Probe:FC08Headless;
 const m=await factory({noInitialRun:true,noExitRuntime:true,locateFile:f=>'/wasm/'+f,print:s=>stdout.push(s),printErr:s=>stderr.push(s)});
 m.FS.writeFile('/fixture.pb',new Uint8Array(await(await fetch(url)).arrayBuffer()));
 let exit_code=null,error=null;try{exit_code=m.callMain(argv)??0;}catch(e){if(e?.name==='ExitStatus')exit_code=e.status;else error=String(e);}
 return {exit_code,stdout:stdout.join('\\n'),stderr:stderr.join('\\n'),error};
};</script>`;
let browser,server,page,browserCDP;
try {
  // Verify every fixture before the browser can read it.
  for(const request of requests){const data=await fs.readFile(request.path);if(hash(data)!==request.sha256)throw new Error('Frozen fixture changed: '+request.id);}
  server=http.createServer(async(req,res)=>{try{
    const url=new URL(req.url,'http://127.0.0.1');
    if(url.pathname==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);return;}
    if(url.pathname.startsWith('/fixture/')){const request=requests[Number(url.pathname.slice(9))];if(!request)throw new Error('Unknown fixture');const data=await fs.readFile(request.path);if(hash(data)!==request.sha256)throw new Error('Fixture changed');res.end(data);return;}
    if(!url.pathname.startsWith('/wasm/'))throw new Error('Unknown resource');
    const file=path.resolve(programs,decodeURIComponent(url.pathname.slice(6)));if(!file.startsWith(programs+path.sep))throw new Error('Path escape');
    res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':'text/javascript');res.end(await fs.readFile(file));
  }catch(error){res.statusCode=404;res.end(String(error));}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const url=`http://127.0.0.1:${server.address().port}/`;
  const argv=['--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  browser=spawn(args.chrome,argv,{windowsHide:true,stdio:'ignore'});
  let port;for(let i=0;i<100;++i){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await wait(100);}}
  if(!port)throw new Error('Chrome did not expose its actual DevTools endpoint');
  const version=await(await fetch(`http://127.0.0.1:${port}/json/version`)).json();
  browserCDP=await CDP.open(version.webSocketDebuggerUrl);
  const target=await browserCDP.send('Target.createTarget',{url});
  let tab;for(let i=0;i<100;++i){tab=(await(await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find(t=>t.id===target.targetId);if(tab)break;await wait(50);}
  if(!tab)throw new Error('Created browser target unavailable');
  page=await CDP.open(tab.webSocketDebuggerUrl);await page.send('Runtime.enable');
  let ready=false;for(let i=0;i<100;++i){const r=await page.send('Runtime.evaluate',{expression:'typeof runActual==="function" && typeof FC08Probe==="function" && typeof FC08Headless==="function"',returnByValue:true});if(r.result.value){ready=true;break;}await wait(100);}
  if(!ready)throw new Error('Actual WASM program factories not loaded');
  const binaries=[];for(const stem of ['baseline_probe','headless_probe'])for(const ext of ['js','wasm']){const file=path.join(programs,`axiom_fc08_${stem}.${ext}`);const bytes=await fs.readFile(file);binaries.push({path:file,sha256:hash(bytes),size:bytes.length});}
  const invocations=[];
  for(let i=0;i<requests.length;++i){const request=requests[i];const argv=request.program==='headless'?[request.mode,request.root,'/fixture.pb']:[request.root,'/fixture.pb'];
    const expression=`window.runActual(${JSON.stringify(request.program)},${JSON.stringify(argv)},${JSON.stringify('/fixture/'+i)})`;
    const started_at=new Date().toISOString();const response=await page.send('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});
    if(response.exceptionDetails)throw new Error(JSON.stringify(response.exceptionDetails));const result=response.result.value;
    const raw={...result,program:request.program,argv,input:request,browser_url:url,started_at,ended_at:new Date().toISOString()};
    await fs.writeFile(path.join(output,request.id+'.json'),JSON.stringify(raw,null,2)+'\n');
    invocations.push({id:request.id,argv,input_sha256:request.sha256,exit_code:result.exit_code});
    if(result.error || result.exit_code!==0)throw new Error('Actual WASM call failed: '+request.id);
  }
  await fs.writeFile(path.join(output,'environment.json'),JSON.stringify({runtime:'ACTUAL_CHROME_WASM',browser:version,chrome_executable:args.chrome,chrome_pid:browser.pid,chrome_argv:argv,node:process.version,binaries,invocations,physical_observation_claimed:false},null,2)+'\n');
  process.stdout.write(`Actual Chrome/WASM calls: ${invocations.length}\n`);
}catch(error){await fs.writeFile(path.join(output,'failure.json'),JSON.stringify({error:String(error)},null,2)+'\n');process.stderr.write(String(error)+'\n');process.exitCode=1;}
finally{if(page)page.close();if(browserCDP){try{await browserCDP.send('Browser.close');}catch{}browserCDP.close();}if(browser&&browser.exitCode===null)browser.kill();if(server)await new Promise(resolve=>server.close(resolve));}
