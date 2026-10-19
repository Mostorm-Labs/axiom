import {spawn} from 'node:child_process';
import {readFileSync, writeFileSync, mkdirSync, openSync, closeSync, existsSync, copyFileSync, rmSync} from 'node:fs';
import {resolve} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const label = process.argv[2] || 'repair-real';
const app = process.argv[3] || 'axiom_canvas_demo';
const out = resolve('out/p32-web');
const profile = resolve(out, `chrome-real-regression-${process.pid}`);
mkdirSync(profile,{recursive:true});
const portFile = resolve(profile,'DevToolsActivePort');
if (existsSync(portFile)) rmSync(portFile);
const destination = resolve(out, 'build/apps', app, 'tests');
mkdirSync(destination,{recursive:true});
copyFileSync('apps/axiom_canvas_demo/tests/web_minimal_runtime_test.html',resolve(destination,'web_minimal_runtime_test.html'));
const log = openSync(resolve(out,label+'-stderr.log'),'w');
const url = `http://127.0.0.1:8765/${app}/tests/web_minimal_runtime_test.html`;
const child = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe',[
  '--headless=new','--no-first-run','--no-default-browser-check','--disable-background-networking',
  '--force-device-scale-factor=2',
  '--remote-debugging-port=0',`--user-data-dir=${profile}`,url],{stdio:['ignore','ignore',log]});
let ws;
try {
  for (let i=0; !existsSync(portFile) && i<200; ++i) await delay(50);
  const port = readFileSync(portFile,'utf8').split('\n')[0];
  let target;
  for (let i=0; i<100 && !target; ++i) {
    const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
    target = targets.find(t=>t.type==='page' && t.url===url);
    if (!target) await delay(50);
  }
  if (!target) throw new Error('Headless regression page missing');
  ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});
  let id=0; const pending = new Map();
  ws.addEventListener('message',event=>{const message=JSON.parse(event.data); if(message.id && pending.has(message.id)){ pending.get(message.id)(message); pending.delete(message.id); }});
  const send = (method,params={})=>new Promise(resolve=>{const next=++id;pending.set(next,resolve);ws.send(JSON.stringify({id:next,method,params}));});
  for (let i=0;i<200;++i) {
    const state=await send('Runtime.evaluate',{expression:'({ready:document.readyState,url:location.href})',returnByValue:true});
    const value=state.result?.result?.value;
    if(value?.ready==='complete' && value.url===url) break;
    await delay(50);
  }
  const response = await send('Runtime.evaluate',{awaitPromise:true,returnByValue:true,timeout:45000,
    expression:`new Promise(resolve=>{const start=performance.now();const timer=setInterval(()=>{const value=document.getElementById('result')?.textContent;if(value && value!=='PENDING'){clearInterval(timer);resolve(JSON.parse(value));}else if(performance.now()-start>35000){clearInterval(timer);const app=document.getElementById('app');resolve({status:'TIMEOUT',observation:app?.contentWindow.axiomMinimalHost?.observation(),statusText:app?.contentDocument.getElementById('status')?.textContent});}},50);})`});
  const result = response.result?.result?.value || response;
  if (result.status !== 'PASS') {
    const diagnostic = await send('Runtime.evaluate',{awaitPromise:true,returnByValue:true,expression:`(async()=>{
      const w=document.getElementById('app').contentWindow,h=w.axiomMinimalHost || await w.axiomMinimalReady;if(!h)return {noHost:true,title:w.document.title,body:w.document.body.innerText};
      const pixels=id=>{const c=w.document.getElementById(id),g=c.getContext('webgl2'),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);const b=new Uint8Array(c.width*c.height*4);g.readPixels(0,0,c.width,c.height,g.RGBA,g.UNSIGNED_BYTE,b);g.bindFramebuffer(g.FRAMEBUFFER,f);let alpha=0,dark=0;for(let i=0;i<b.length;i+=4){if(b[i+3])++alpha;if(b[i+3]&&b[i]<230&&b[i+1]<230)++dark;}return {alpha,dark,error:g.getError()};};
      const records=[{step:'failure',state:h.observation(),canonical:pixels('axiom-canvas'),preview:pixels('axiom-preview')}];
      try {
      h.module._axiom_web_cancel_all(h.handle);
      let t=100000000000n,generation=0n;
      for(const [phase,x] of [[0,30],[1,110],[1,180]]){const ok=h.module._axiom_web_pointer(h.handle,2,999,generation,t+=16000000n,phase,x,100,.5,1,2,0,0,0,0);if(!phase)generation=h.module._axiom_web_pointer_generation(h.handle,2,999);records.push({step:'pen-input',phase,ok});}
      await h.flush();records.push({step:'pen-preview',state:h.observation(),preview:pixels('axiom-preview')});
      h.module._axiom_web_cancel_all(h.handle);await h.control('shape',0);
      generation=0n;
      for(const [phase,x] of [[0,30],[1,110],[2,180]]){const ok=h.module._axiom_web_pointer(h.handle,3,1000,generation,t+=16000000n,phase,x,160,.5,0,1,0,0,0,0);if(!phase)generation=h.module._axiom_web_pointer_generation(h.handle,3,1000);records.push({step:'shape-input',phase,ok});}
      await h.flush();records.push({step:'shape',state:h.observation(),canonical:pixels('axiom-canvas'),preview:pixels('axiom-preview')});
      h.toggleDebugUi();await h.flush();records.push({step:'debug',pixels:pixels('debugUi')});
      } catch(error) {records.push({error:String(error),state:h.observation()});}
      return records;})()`});
    writeFileSync(resolve(out,label+'-diagnostic-cdp.json'),JSON.stringify(diagnostic,null,2));
    result.diagnostic = diagnostic.result?.result?.value || diagnostic;
  }
  writeFileSync(resolve(out,label+'-result.json'),JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify(result,null,2));
  await send('Browser.close');
  process.exitCode = result.status==='PASS' ? 0 : 1;
} finally {
  ws?.close(); child.kill(); closeSync(log);
}

