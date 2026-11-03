import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import {spawn} from 'node:child_process';

const build=path.resolve(process.argv[2]), chrome=path.resolve(process.argv[3]);
const sceneMode=process.argv[4]==='scene';
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
  await cdp.send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/index.html?selftest=1${sceneMode?'&scene=1':''}`});
  const evaluate=async expression=>{const r=await cdp.send('Runtime.evaluate',{expression,returnByValue:true});
    if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  async function until(expression,label){for(let i=0;i<600;i++){
    const state=await evaluate(`(()=>{let e=document.getElementById('result');return e?{text:e.textContent,preview:e.dataset.preview,lost:e.dataset.lost,restored:e.dataset.restored}:null})()`);
    if(state?.text?.startsWith('FAIL')||state?.text?.startsWith('BLOCKED'))throw new Error(JSON.stringify(state));
    if(await evaluate(expression))return state; await delay(50);
  }throw new Error('timeout '+label);}
  await until(`document.getElementById('result')?.textContent==='READY'`,'ready');
  if(sceneMode){
    const dispatch=async(type,x,y,buttons=0)=>cdp.send('Input.dispatchMouseEvent',{type,x,y,button:'left',buttons,clickCount:1});
    const control=async(action,value)=>evaluate(`(()=>{const ok=Module._sdk_control(${action},${value});Module._sdk_schedule();return ok;})()`);
    const selectionDiagnostics=async()=>JSON.parse(await evaluate('Module.UTF8ToString(Module._sdk_selection_diagnostics())'));
    if(!(await control(1,0)))throw new Error('rectangle tool rejected');
    await dispatch('mouseMoved',40,40);await dispatch('mousePressed',40,40,1);await dispatch('mouseMoved',170,105,1);await dispatch('mouseReleased',170,105,0);
    await until(`Module._sdk_object_count()>=1`,'rectangle commit');
    if(!(await evaluate('Module._sdk_ink_tool()')))throw new Error('ink tool rejected');
    await evaluate('Module._sdk_schedule()');
    await dispatch('mouseMoved',25,135);await dispatch('mousePressed',25,135,1);
    for(let x=45;x<=375;x+=22)await dispatch('mouseMoved',x,135+Math.sin(x*.04)*10,1);
    await dispatch('mouseReleased',375,135,0);await until(`Module._sdk_object_count()>=2`,'long stroke commit');
    const beforeObjects=await evaluate('Module._sdk_object_count()');
    const beforeScale=await evaluate('Module._sdk_camera_scale()');
    const beforePresented=await selectionDiagnostics();
    if(beforeObjects!==2||!(beforeScale>0))throw new Error('scene setup diagnostics '+JSON.stringify({beforeObjects,beforeScale,beforePresented}));
    if(!(await evaluate(`Module._sdk_view(1,100,75,Module._sdk_camera_scale()*1.5)`)))throw new Error('zoom rejected');
    if(!(await evaluate(`Module._sdk_view(0,-24,12,1)`)))throw new Error('pan rejected');
    await evaluate('Module._sdk_schedule()');
    await until(`Module._sdk_object_count()===2&&Module._sdk_camera_scale()>${Number(beforeScale)*1.2}`,'scene navigation');
    const afterObjects=await evaluate('Module._sdk_object_count()');
    const afterScale=await evaluate('Module._sdk_camera_scale()');
    const afterPresented=await selectionDiagnostics();
    if(afterObjects!==2||afterScale<=beforeScale*1.2)throw new Error('scene navigation diagnostics '+JSON.stringify({beforeObjects,beforeScale,beforePresented,afterObjects,afterScale,afterPresented}));
    await evaluate(`document.getElementById('result').textContent='PASS_SCENE'`);
    await evaluate('Module._sdk_shutdown()');
    console.log(JSON.stringify({status:'PASS_SCENE',consumer:build,chrome,before:{objects:beforeObjects,scale:beforeScale,selection:beforePresented},after:{objects:afterObjects,scale:afterScale,selection:afterPresented},coverage:['external Web SDK consumer','rectangle plus long compact stroke','zoom and pan','canonical surface presentation','public control receipts']}));
    try{await cdp.send('Browser.close');}catch{}
    throw new Error('__SCENE_DONE__');
  }
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:60,y:60});
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',x:60,y:60,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:150,y:100,button:'left',buttons:1});
  await until(`document.getElementById('result')?.dataset.preview==='complete'`,'preview fence');
  const pixel=async id=>await evaluate(`(()=>{const c=document.getElementById(${JSON.stringify(id)}),g=c.getContext('webgl2'),p=new Uint8Array(4),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(97,c.height-72,1,1,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,f);return [...p];})()`);
  const previewPixel=await pixel('preview');
  if(!previewPixel || previewPixel[3]===0)throw new Error('preview pixel not rendered '+JSON.stringify(previewPixel));
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',x:150,y:100,button:'left',buttons:0,clickCount:1});
  await until(`document.getElementById('result')?.dataset.commit==='complete'`,'canonical fence');
  await until(`getComputedStyle(document.getElementById('preview')).visibility==='hidden'`,'preview retirement');
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
} catch(e){if(String(e.message??e)!=='__SCENE_DONE__'){failed=String(e.stack??e);console.error(failed);}}
finally {
  socket?.close();browser?.kill();if(server)await new Promise(r=>server.close(r));
  const temp=await fs.realpath(os.tmpdir()), resolved=await fs.realpath(profile);
  if(!resolved.startsWith(temp+path.sep)||!path.basename(resolved).startsWith('axiom-external-chrome-'))
    throw new Error('refusing to remove unexpected browser profile');
  await fs.rm(resolved,{recursive:true,force:true,maxRetries:5,retryDelay:100});
}
process.exitCode=failed?1:0;
