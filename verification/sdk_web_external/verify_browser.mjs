import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import {spawn} from 'node:child_process';

const build=path.resolve(process.argv[2]), chrome=path.resolve(process.argv[3]);
const delay=ms=>new Promise(r=>setTimeout(r,ms));
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-external-chrome-'));
let server,browser,socket,failed=''; const events=[];
class CDP {
  constructor(ws){this.ws=ws;this.id=0;this.pending=new Map();ws.addEventListener('message',e=>{
    const m=JSON.parse(e.data); if(!m.id){events.push(m);return;}
    const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);clearTimeout(p.timer);
    m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);
  });}
  send(method,params={}){const id=++this.id;return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('CDP timeout '+method)),30000);
    this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));
  });}
}
try {
  server=http.createServer(async(req,res)=>{try {
    const name=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname);
    const target=path.resolve(build,'.'+(name==='/'?'/index.html':name));
    if(!target.startsWith(build+path.sep))throw new Error('outside consumer');
    const bytes=await fs.readFile(target);
    res.setHeader('Content-Type',target.endsWith('.wasm')?'application/wasm':
      /\.m?js$/.test(target)?'text/javascript':target.endsWith('.data')?'application/octet-stream':'text/html');
    res.end(bytes);
  }catch{res.statusCode=404;res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const args=['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader',
    '--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  browser=spawn(chrome,args,{stdio:'ignore',windowsHide:true});
  const portFile=path.join(profile,'DevToolsActivePort'); let port='';
  for(let i=0;i<200&&!port;i++){try{port=(await fs.readFile(portFile,'utf8')).split('\n')[0];}catch{await delay(50);}}
  if(!port)throw new Error('Chrome did not start');
  const page=await fetch(`http://127.0.0.1:${port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());
  socket=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  const cdp=new CDP(socket);await cdp.send('Runtime.enable');await cdp.send('Page.enable');
  await cdp.send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/index.html`});
  const evaluate=async expression=>{const r=await cdp.send('Runtime.evaluate',{expression,returnByValue:true});
    if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  async function until(expression,label){for(let i=0;i<600;i++){
    const state=await evaluate(`(()=>{let e=document.getElementById('result');return e?{text:e.textContent,preview:e.dataset.preview,lost:e.dataset.lost,restored:e.dataset.restored}:null})()`);
    if(state?.text?.startsWith('FAIL')||state?.text?.startsWith('BLOCKED'))throw new Error(JSON.stringify(state));
    if(await evaluate(expression))return state; await delay(50);
  }throw new Error('timeout '+label);}
  await until(`document.getElementById('result')?.textContent==='READY'`,'ready');
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:60,y:60});
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',x:60,y:60,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:150,y:100,button:'left',buttons:1});
  await until(`document.getElementById('result')?.dataset.preview==='complete'`,'preview fence');
  const pixel=async id=>await evaluate(`(()=>{const c=document.getElementById(${JSON.stringify(id)}),g=c.getContext('webgl2'),p=new Uint8Array(4),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(97,c.height-72,1,1,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,f);return [...p];})()`);
  const previewPixel=await pixel('preview');
  if(!previewPixel || previewPixel[3]===0)throw new Error('preview pixel not rendered '+JSON.stringify(previewPixel));
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:150,y:100,button:'left',buttons:0,clickCount:1});
  await until(`document.getElementById('result')?.dataset.commit==='complete'`,'canonical fence');
  const canonicalPixel=await pixel('surface');
  if(!canonicalPixel || canonicalPixel[3]===0 || canonicalPixel.slice(0,3).every(v=>v>230))
    throw new Error('canonical stroke pixel not rendered '+JSON.stringify(canonicalPixel));
  await evaluate(`document.getElementById('result').dataset.pixelChecked='yes'`);
  const final=await until(`document.getElementById('result')?.textContent==='PASS'`,'commit and recovery');
  if(final.lost!=='2'||final.restored!=='2')throw new Error('both WebGL contexts must lose and restore '+JSON.stringify(final));
  if(events.some(e=>e.method==='Runtime.exceptionThrown'))throw new Error('browser exception '+JSON.stringify(events.filter(e=>e.method==='Runtime.exceptionThrown')));
  console.log(JSON.stringify({status:'PASS_SCOPED',consumer:build,chrome,args,final,previewPixel,canonicalPixel,
    coverage:['real browser WebGL2 contexts','synthetic DOM pointer at DPR 2','preview GPU fence','canonical feedback','resize','in-flight loss','WEBGL_lose_context events/rebind','font catalog','synthetic text input','shutdown']}));
  try{await cdp.send('Browser.close');}catch{}
} catch(e){failed=String(e.stack??e);console.error(failed);}
finally {
  socket?.close();browser?.kill();if(server)await new Promise(r=>server.close(r));
  const temp=await fs.realpath(os.tmpdir()), resolved=await fs.realpath(profile);
  if(!resolved.startsWith(temp+path.sep)||!path.basename(resolved).startsWith('axiom-external-chrome-'))
    throw new Error('refusing to remove unexpected browser profile');
  await fs.rm(resolved,{recursive:true,force:true,maxRetries:5,retryDelay:100});
}
process.exitCode=failed?1:0;
