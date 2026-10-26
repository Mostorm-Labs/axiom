// Real Chrome/WASM execution. Node only transports bytes and records browser
// observations; it never executes or substitutes the Axiom WASM program.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';

const args=Object.fromEntries(process.argv.slice(2).reduce((a,v,i,all)=>i%2?a:[...a,[v.replace(/^--/,''),all[i+1]]],[]));
const repo=path.resolve(args.repo??'.');
const build=path.resolve(args.build??'out/fc08/v02-on/web-wasm');
const output=path.resolve(args.output??'.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01/browser');
const chrome=args.chrome??'C:/Program Files/Google/Chrome/Application/chrome.exe';
const programs=path.join(build,'tests/fc08');
const hash=b=>createHash('sha256').update(b).digest('hex');
await fs.mkdir(output,{recursive:true});
const profile=path.join(repo,'out/fc08',`chrome-${Date.now()}`);
await fs.mkdir(profile,{recursive:true});
const invocations=[];
const hooksOn=args.mode!=='off';
const fixtureRequests=args.requests?JSON.parse(await fs.readFile(path.resolve(args.requests),'utf8')):[];
let browser,server;
const html=`<!doctype html><meta charset="utf-8"><title>FC08 verification WASM transport</title>
<script src="/wasm/axiom_fc08_baseline_probe.js"></script>
<script src="/wasm/axiom_fc08_hook_probe.js"></script>
<script src="/wasm/axiom_fc08_headless_probe.js"></script>
<script src="/wasm/axiom_fc08_selection_regression.js"></script>
<script src="/wasm/axiom_fc08_connector_regression.js"></script>
<script src="/wasm/axiom_fc08_control_regression.js"></script>
<script>
window.FC08Run=async function(program,argv,input) {
 const stdout=[],stderr=[];
 const factory={hooks:FC08Hooks,baseline:FC08Probe,headless:FC08Headless,
   selection:FC08Selection,connector:FC08Connector,control:FC08Control}[program];
 const m=await factory({noInitialRun:true,noExitRuntime:true,
   locateFile:f=>'/wasm/'+f, print:s=>stdout.push(s),printErr:s=>stderr.push(s)});
 if(input) m.FS.writeFile('/fixture.pb',typeof input==='string'?new Uint8Array(await(await fetch(input)).arrayBuffer()):Uint8Array.from(input));
 let callError=null,exitCode=null;
 try {exitCode=m.callMain(argv)??0;} catch(e) { if(e?.name==='ExitStatus')exitCode=e.status;else callError=String(e); }
 return {stdout,stderr,callError,exitCode};
};
</script>`;

class CDP {
  constructor(ws) {
    this.ws=ws;this.next=0;this.pending=new Map();
    ws.addEventListener('message',e=>{const m=JSON.parse(e.data);if(!m.id)return;
      const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);clearTimeout(p.timer);
      m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);});
  }
  static async connect(url) {
    const ws=new WebSocket(url);
    await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});
    return new CDP(ws);
  }
  send(method,params={}) {
    const id=++this.next;
    return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{this.pending.delete(id);reject(new Error('CDP timeout: '+method));},60000);
      this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));});
  }
  close(){this.ws.close();}
}
const wait=ms=>new Promise(r=>setTimeout(r,ms));
let browserCDP,page;
try {
  server=http.createServer(async(req,res)=>{
    try {
      const url=new URL(req.url,'http://127.0.0.1');
      if(url.pathname==='/') {res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);return;}
      if(url.pathname.startsWith('/fixture/')) {
        const index=Number(url.pathname.slice(9));const fixture=fixtureRequests[index];
        if(!fixture)throw new Error('fixture not in explicit request manifest');
        const bytes=await fs.readFile(fixture.path);
        if(hash(bytes)!==fixture.sha256)throw new Error('fixture content changed');
        res.setHeader('Content-Type','application/octet-stream');res.end(bytes);return;
      }
      if(!url.pathname.startsWith('/wasm/')) {res.statusCode=404;res.end();return;}
      const file=path.resolve(programs,decodeURIComponent(url.pathname.slice(6)));
      if(!file.startsWith(programs+path.sep)) throw new Error('path outside test build');
      const data=await fs.readFile(file);
      res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':'text/javascript');res.end(data);
    } catch {res.statusCode=404;res.end();}
  });
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const base=`http://127.0.0.1:${server.address().port}/`;
  const argv=['--headless=new','--disable-gpu','--no-first-run','--no-default-browser-check',
    '--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  browser=spawn(chrome,argv,{windowsHide:true,stdio:'ignore'});
  let port;
  for(let i=0;i<100;++i){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);break;}catch{await wait(100);}}
  if(!port)throw new Error('Chrome did not expose its own DevTools endpoint');
  const version=await(await fetch(`http://127.0.0.1:${port}/json/version`)).json();
  browserCDP=await CDP.connect(version.webSocketDebuggerUrl);
  const target=await browserCDP.send('Target.createTarget',{url:base});
  let tab;
  for(let i=0;i<100;++i){const list=await(await fetch(`http://127.0.0.1:${port}/json/list`)).json();tab=list.find(t=>t.id===target.targetId);if(tab)break;await wait(50);}
  if(!tab)throw new Error('created browser page not discoverable');
  page=await CDP.connect(tab.webSocketDebuggerUrl);
  await page.send('Runtime.enable');
  let ready=false;
  for(let i=0;i<100;++i){const r=await page.send('Runtime.evaluate',{expression:'typeof window.FC08Run === "function" && typeof FC08Hooks === "function" && typeof FC08Probe === "function"',returnByValue:true});if(r.result.value){ready=true;break;}await wait(100);}
  if(!ready)throw new Error('real WASM test programs did not load');
  const identities=[];
  for(const stem of ['baseline_probe','hook_probe','headless_probe','selection_regression','connector_regression','control_regression'])for(const ext of ['js','wasm']){
    const name=`axiom_fc08_${stem}.${ext}`;
    const file=path.join(programs,name);const bytes=await fs.readFile(file);identities.push({path:file,sha256:hash(bytes),size:bytes.length});
  }
  const execute=async(id,program,argv,input=null)=>{
    const expression=`window.FC08Run(${JSON.stringify(program)},${JSON.stringify(argv)},${JSON.stringify(input)})`;
    const started=new Date().toISOString();
    const response=await page.send('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});
    if(response.exceptionDetails)throw new Error(JSON.stringify(response.exceptionDetails));
    const raw=response.result.value;
    await fs.writeFile(path.join(output,id+'.raw.json'),JSON.stringify(raw,null,2)+'\n');
    if(raw.callError)throw new Error(id+': '+raw.callError);
    const line=raw.stdout.findLast(s=>s.startsWith('{'));
    if(!line && !['selection','connector','control'].includes(program))throw new Error(id+': actual program produced no JSON observation');
    const observation=line?JSON.parse(line):{exit_code:raw.exitCode,stdout:raw.stdout};
    if(raw.exitCode!==0)throw new Error(id+': actual program failed with '+raw.exitCode);
    await fs.writeFile(path.join(output,id+'.json'),JSON.stringify(observation,null,2)+'\n');
    invocations.push({id,program,argv,started_at:started,ended_at:new Date().toISOString(),
      raw_artifact:path.join(output,id+'.raw.json'),observed_artifact:path.join(output,id+'.json')});
    process.stdout.write(id+' browser execution recorded\n');
    return observation;
  };
  const discovery=await execute('discovery','baseline',['--discover']);
  if(!discovery.roots?.length)throw new Error('empty actual browser discovery');
  const hooks=await execute(hooksOn?'live-hooks':'passive-off','hooks',[]);
  if(hooksOn && (!hooks.binding || !hooks.phases?.length))throw new Error('qualification browser binding not reached');
  if(!hooksOn && hooks.binding)throw new Error('normal build unexpectedly exposes qualification controls');
  await execute('inherited-selection','selection',[]);
  await execute('inherited-connector','connector',[]);
  await execute('inherited-control','control',[]);
  const probes=JSON.parse(await fs.readFile(path.join(repo,'.aegis/materialized/FC-08/r01-inputs.json'),'utf8'));
  for(const probe of probes){
    const bytes=await fs.readFile(path.join(repo,probe.path));
    if(hash(bytes)!==probe.sha256)throw new Error('probe input hash mismatch');
    await execute(probe.id,'baseline',[probe.root,'/fixture.pb'],Array.from(bytes));
    if(probe.id.startsWith('legacy-snapshot'))
      await execute(probe.id+'-import','headless',['LegacyImport','DocumentSnapshot','/fixture.pb'],Array.from(bytes));
    if(probe.id==='legacy-insert')
      await execute('legacy-replay','headless',['Replay','Operation','/fixture.pb'],Array.from(bytes));
  }
  for(let i=0;i<fixtureRequests.length;++i) {
    const f=fixtureRequests[i];
    const argv=f.program==='headless'?[f.mode,f.root,'/fixture.pb']:[f.root,'/fixture.pb'];
    await execute(f.id,f.program,argv,`/fixture/${i}`);
  }
  await fs.writeFile(path.join(output,'profile.json'),JSON.stringify({
    status:'ACTUAL_BROWSER_WASM_EXECUTION_RECORDED',browser:version,chrome_executable:chrome,
    chrome_argv:argv,chrome_pid:browser.pid,node_version:process.version,base_url:base,
    programs:identities,invocations,physical_PASS:false,product_PASS:false},null,2)+'\n');
  process.stdout.write('Actual browser/WASM observations saved; no FC08 product PASS claimed\n');
} catch(error) {
  await fs.writeFile(path.join(output,'failure.json'),JSON.stringify({error:String(error),invocations,product_PASS:false},null,2)+'\n');
  process.exitCode=1;process.stderr.write(String(error)+'\n');
} finally {
  if(page)page.close();
  if(browserCDP){try{await browserCDP.send('Browser.close');}catch{}browserCDP.close();}
  if(browser && browser.exitCode===null)browser.kill();
  if(server)await new Promise(r=>server.close(r));
}
