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
  await cdp.send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/index.html?manual=1`});
  const evaluate=async expression=>{const r=await cdp.send('Runtime.evaluate',{expression,returnByValue:true});
    if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  async function until(expression,label){for(let i=0;i<600;i++){
    const state=await evaluate(`(()=>{let e=document.getElementById('result');return e?{text:e.textContent,preview:e.dataset.preview,lost:e.dataset.lost,restored:e.dataset.restored}:null})()`);
    if(state?.text?.startsWith('FAIL')||state?.text?.startsWith('BLOCKED'))throw new Error(JSON.stringify(state));
    if(await evaluate(expression))return state; await delay(50);
  }throw new Error('timeout '+label);}
  await until(`document.getElementById('result')?.textContent==='READY'`,'ready');
  await until(`window.externalTextInput && !document.getElementById('external-text-tool').hidden`,'text tool');
  await evaluate(`document.getElementById('external-text-tool').click()`);
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',x:90,y:90,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:90,y:90,button:'left',buttons:0,clickCount:1});
  await until(`document.activeElement?.id==='axiom-ime'`,'IME carrier focus');
  await cdp.send('Input.imeSetComposition',{text:'nihao',selectionStart:5,selectionEnd:5});
  await until(`window.externalTextInput?.snapshot().phase===2`,'IME preedit');
  await cdp.send('Input.insertText',{text:'你好'});
  await until(`String.fromCharCode(...window.externalTextInput.snapshot().projection)==='你好'`,'single IME result');
  await cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await cdp.send('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await until(`window.externalTextInput.snapshot().phase===0`,'finish editor');
  const beforeInk = await evaluate('Module._sdk_object_count()');
  await evaluate(`document.getElementById('external-ink-tool').click()`);
  await delay(100);
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',x:20,y:20,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:60,y:60,button:'left',buttons:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:60,y:60,button:'left',buttons:0,clickCount:1});
  await until(`Module._sdk_object_count()===${beforeInk + 1}`,'ink after text');
  if(events.some(e=>e.method==='Runtime.exceptionThrown'))throw new Error('browser exception');
  console.log(JSON.stringify({status:'PASS_SCOPED',consumer:build,chrome,args,coverage:['external interactive SDK consumer','synthetic Chrome IME composition/result']}));
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
