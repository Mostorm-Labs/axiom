import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-js-sdk-'));
const profile=path.join(root,'profile'), files=path.join(root,'consumer');
let browser,server,socket;let failure;let observation;
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
try {
  await fs.mkdir(files);
  for(const name of ['axiom.mjs','axiom.d.mts','internal','axiom.wasm','axiom.data','pointer_input.mjs','text_input.mjs','package.json','README.md','example.html','serve.py','licenses'])
    await fs.cp(path.resolve(process.argv[2],name),path.join(files,name),{recursive:true});
  const framesOnly=process.argv[5]==='frames';
  await fs.copyFile(new URL(framesOnly?'./frame_handoff.html':'./browser.html',import.meta.url),path.join(files,'index.html'));
  await fs.copyFile(new URL('./frame_handoff.html',import.meta.url),path.join(files,'frame_handoff.html'));
  server=http.createServer(async(req,res)=>{try{const p=path.resolve(files,'.'+new URL(req.url,'http://localhost').pathname.replace(/\/$/,'/index.html'));
    if(!p.startsWith(files+path.sep))throw Error('path');
    res.setHeader('Content-Type',p.endsWith('.wasm')?'application/wasm':p.endsWith('.mjs')?'text/javascript':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));
  }catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=spawn(process.argv[3],['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader','--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'],{windowsHide:true,stdio:'ignore'});
  let port;for(let i=0;i<200&&!port;i++){try{port=(await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];}catch{await delay(50);}}
  if(!port)throw Error('Chrome unavailable');const page=await fetch(`http://127.0.0.1:${port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());
  socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
  let serial=0;const pending=new Map(),errors=[],consoleErrors=[];
  socket.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);if(!p)return;pending.delete(m.id);clearTimeout(p.timer);m.error?p.reject(Error(JSON.stringify(m.error))):p.resolve(m.result);}else if(m.method==='Runtime.exceptionThrown')errors.push(m.params);else if(m.method==='Runtime.consoleAPICalled'&&m.params.type==='error')consoleErrors.push(m.params.args.map(a=>a.value??a.description).join(' '));});
  const send=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial;const timer=setTimeout(()=>{pending.delete(id);reject(Error(`CDP timeout: ${method}`));},30000);pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));});
  // Browser.close may tear down this target socket before its response arrives.
  // Prove real process termination instead of waiting for a vanished transport.
  const closeBrowser=()=>new Promise((resolve,reject)=>{
    const done=(code,signal)=>{clearTimeout(timer);observation.browserExit={code,signal};code===0&&!signal?resolve():reject(Error(`Chrome exit: ${code}/${signal}`));};
    const timer=setTimeout(()=>{browser.removeListener('exit',done);reject(Error('Chrome exit timeout'));},30000);
    if(browser.exitCode!==null||browser.signalCode!==null){done(browser.exitCode,browser.signalCode);return;}
    browser.once('exit',done);socket.send(JSON.stringify({id:++serial,method:'Browser.close',params:{}}));
  });
  const browserVersion=await send('Browser.getVersion');
  await send('Runtime.enable');await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/`});
  let result;
  for(let i=0;i<900;i++) {const r=await send('Runtime.evaluate',{expression:`document.getElementById('result')?.textContent`,returnByValue:true});
    if(r.result.value && r.result.value!=='PENDING'){result=JSON.parse(r.result.value);break;}await delay(100);}
  const hashes={};for(const name of ['axiom.mjs','axiom.d.mts','internal/axiom.mjs','axiom.wasm','axiom.data','pointer_input.mjs','text_input.mjs'])hashes[name]=createHash('sha256').update(await fs.readFile(path.join(files,name))).digest('hex');
  observation={consumer:files,result,exceptions:errors,consoleErrors,browserVersion,mode:'headless Chrome/WebGL2; synthetic DOM input',files:hashes};
  console.log(JSON.stringify(observation));
  const validFrames=value=>value?.status==='PASS'&&['pending-input','no-duplicate-completion','pending-resize','native-loss-before-dom','context-recovery'].every(id=>value.scenarios?.includes(id))&&value.delayed>0;
  if(result?.status!=='PASS'||!Array.isArray(result.results)||(framesOnly?!validFrames(result):result.results.length<40)||errors.length)throw Error('JS SDK browser acceptance failed');
  if(!framesOnly&&process.env.AXIOM_EXPECTED_REVISION&&(result.buildInfo?.sourceRevision!==process.env.AXIOM_EXPECTED_REVISION||result.buildInfo?.dirty))throw Error('JS engine is not the frozen clean release revision');
  if(framesOnly){await closeBrowser();}
  else {
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/frame_handoff.html`});
  let handoff;
  for(let i=0;i<900;i++){const r=await send('Runtime.evaluate',{expression:`document.getElementById('result')?.textContent`,returnByValue:true});if(r.result.value&&r.result.value!=='PENDING'){handoff=JSON.parse(r.result.value);break;}await delay(100);}
  observation.frameRegression=handoff;
  console.log(JSON.stringify({frameRegression:handoff}));
  if(!validFrames(handoff))throw Error('JS SDK frame handoff regression failed');
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
  const evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  const until=async(expression,label)=>{for(let i=0;i<300;i++){if(await evaluate(expression))return;await delay(50);}throw Error('Example timeout: '+label);};
  await until(`document.getElementById('status')?.textContent.includes('0 objects')`,'startup');
  await evaluate(`[...document.querySelectorAll('button')].find(b=>b.textContent==='rectangle').click()`);
  await until(`[...document.querySelectorAll('button')].find(b=>b.textContent==='rectangle')?.getAttribute('aria-pressed')==='true'`,'tool receipt');
  const point=await evaluate(`(()=>{const r=document.getElementById('canvas').getBoundingClientRect();return {x:r.left+60,y:r.top+60}})()`);
  await send('Input.dispatchMouseEvent',{type:'mousePressed',...point,button:'left',buttons:1,clickCount:1});
  await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:point.x+100,y:point.y+70,button:'left',buttons:1});
  await send('Input.dispatchMouseEvent',{type:'mouseReleased',x:point.x+100,y:point.y+70,button:'left',buttons:0,clickCount:1});
  await until(`document.getElementById('status').textContent.includes('1 objects')`,'rectangle');
  await until(`(()=>{const c=document.querySelector('#canvas canvas'),g=c.getContext('webgl2'),p=new Uint8Array(4),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(90,c.height-90,1,1,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,f);return p[2]>p[0]+20&&p[3]>0})()`,'visible rectangle');
  observation.example={status:await evaluate(`document.getElementById('status').textContent`),toolIndicated:true,visibleRectangle:true,input:'Chrome CDP mouse'};
  if(errors.length)throw Error('Example exception');
  console.log(JSON.stringify({example:observation.example}));
  await closeBrowser();
  }
}catch(error){failure=error;console.error(error.stack||error);}
finally{socket?.close();browser?.kill();if(server)await new Promise(r=>server.close(r));
  if(process.argv[4]){const output=path.resolve(process.argv[4]);await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({command:process.argv,...observation,exitCode:failure?1:0,error:failure?String(failure):undefined},null,2)+'\n');}
  const safe=await fs.realpath(os.tmpdir()),actual=await fs.realpath(root);if(!actual.startsWith(safe+path.sep)||!path.basename(actual).startsWith('axiom-js-sdk-'))throw Error('unsafe temporary path');
  await fs.rm(root,{recursive:true,force:true,maxRetries:6,retryDelay:100});}
process.exitCode=failure?1:0;
