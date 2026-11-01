import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-ink-v2-web-'));
const files = path.join(root, 'consumer');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
let browser, server, socket, failure;
const observation = {checks: {}, errors: [], exceptions: [], input: 'Chrome CDP mouse, public JS SDK, independent WASM instances'};
try {
  await fs.mkdir(files);
  const hashes = {};
  for (const name of ['axiom.mjs', 'internal', 'axiom.wasm', 'axiom.data', 'pointer_input.mjs', 'text_input.mjs', 'example.html']) {
    await fs.cp(path.resolve(process.argv[2], name), path.join(files, name), {recursive: true});
    if (name !== 'internal') hashes[name] = createHash('sha256').update(await fs.readFile(path.join(files, name))).digest('hex');
  }
  hashes['internal/axiom.mjs'] = createHash('sha256').update(await fs.readFile(path.join(files, 'internal/axiom.mjs'))).digest('hex');
  observation.files = hashes;
  await fs.copyFile(new URL('./ink-v2.html', import.meta.url), path.join(files, 'index.html'));
  server = http.createServer(async (request, response) => {
    try {
      const filename = path.resolve(files, '.' + new URL(request.url, 'http://localhost').pathname.replace(/\/$/, '/index.html'));
      if (!filename.startsWith(files + path.sep)) throw Error('invalid path');
      response.setHeader('Content-Type', filename.endsWith('.wasm') ? 'application/wasm'
        : filename.endsWith('.mjs') ? 'text/javascript' : filename.endsWith('.html') ? 'text/html' : 'application/octet-stream');
      response.end(await fs.readFile(filename));
    } catch { response.statusCode = 404; response.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const started = await launchChrome(process.argv[3], path.join(root, 'profile'));
  browser = started.browser;
  const page = await fetch(`http://127.0.0.1:${started.port}/json/new?about:blank`, {method: 'PUT'}).then(response => response.json());
  socket = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  let serial = 0;
  const pending = new Map();
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.id) {
      const request = pending.get(message.id);
      if (!request) return;
      pending.delete(message.id); clearTimeout(request.timer);
      message.error ? request.reject(Error(JSON.stringify(message.error))) : request.resolve(message.result);
    } else if (message.method === 'Runtime.exceptionThrown') observation.exceptions.push(message.params);
  });
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++serial;
    const timer = setTimeout(() => { pending.delete(id); reject(Error(`CDP timeout: ${method}`)); }, 90000);
    pending.set(id, {resolve, reject, timer}); socket.send(JSON.stringify({id, method, params}));
  });
  const evaluate = async expression => {
    const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});
    if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;
  };
  observation.browser=await send('Browser.getVersion');await send('Runtime.enable');await send('Page.enable');
  const deviceScaleFactor = Number(process.env.AXIOM_INK_TEST_DPR || 1);
  observation.deviceScaleFactor = deviceScaleFactor;
  observation.brushRevision = Number(process.env.AXIOM_INK_TEST_REVISION || 1);
  await send('Emulation.setDeviceMetricsOverride',{width:1000,height:650,deviceScaleFactor,mobile:false});
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/`});
  for(let n=0;!await evaluate('Boolean(window.probe)');++n){if(n>300)throw Error('startup');await delay(50);}
  await evaluate('probe.ready');await evaluate(`probe.revision(${observation.brushRevision})`);observation.buildInfo=await evaluate('probe.info()');
  const mouse=(type,x,y,held=false)=>send('Input.dispatchMouseEvent',{type,x,y,button:'left',buttons:type==='mousePressed'||held?1:0,clickCount:1});
  for(let index=0;index<7;++index){
    await evaluate(`probe.select(${index})`);const y=40+index*70;
    await mouse('mousePressed',50,y);await delay(30);
    for(let k=1;k<=12;++k){await mouse('mouseMoved',50+k*30,y+Math.sin(k*.5)*14,true);await delay(10);}
    await delay(100);
    const preview=await send('Page.captureScreenshot',{format:'png'});
    await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,`-preview-${index}.png`),Buffer.from(preview.data,'base64'));
    await evaluate(`probe.down(${index})`);
    await mouse('mouseReleased',410,y+Math.sin(6)*14);await delay(150);
    Object.assign(observation.checks,(await evaluate(`probe.up(${index})`)).checks);
  }
  const persist=await evaluate('probe.persist()');Object.assign(observation.checks,persist.checks);observation.snapshotBytes=persist.snapshotBytes;
  await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-render.png'),Buffer.from(persist.bytes));
  Object.assign(observation.checks,(await evaluate('probe.lifecycle()')).checks);
  const done=await evaluate('probe.close()');Object.assign(observation.checks,done.checks);observation.errors=done.errors;
  if(observation.brushRevision>=3){
    await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
    for(let attempt=0;!await evaluate(`document.querySelector('#tools')?.getAttribute('aria-busy')==='false'`);++attempt){
      if(attempt>300)throw Error('tuning Demo startup');await delay(50);
    }
    const controls=await evaluate(`Object.fromEntries(['size','thinning','streamline','smoothing','startTaper','endTaper','easing','pressure-model'].map(k=>[k,document.getElementById('ink-'+k)?.value]))`);
    observation.demoControls=controls;
    observation.checks.demoVectorDefaults=controls.size==='16'&&controls.thinning==='0.99'&&controls.streamline==='0.13'&&controls.smoothing==='0.61'&&controls.endTaper===(observation.brushRevision>=4?'0':'16')&&controls.easing==='1'&&controls['pressure-model']==='2';
    const demo=await send('Page.captureScreenshot',{format:'png'});
    await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo.png'),Buffer.from(demo.data,'base64'));
    const mounted=await evaluate(`(()=>{const r=document.getElementById('canvas').getBoundingClientRect();return {left:r.left,top:r.top,width:r.width,height:r.height};})()`);
    const y=mounted.top+Math.min(110,mounted.height/2);
    await mouse('mousePressed',100,y);for(let k=1;k<=16;++k){await mouse('mouseMoved',100+k*25,y+Math.sin(k*.3)*45,true);await delay(12);}
    // Slow the final real motion before lifting: distance pressure should recover
    // a small round end without a forced taper or a stationary-release dot.
    for(let k=1;k<=8;++k){await mouse('mouseMoved',500+k*2,y+Math.sin(4.8)*45,true);await delay(12);}
    await mouse('mouseReleased',516,y+Math.sin(4.8)*45);await delay(250);
    observation.checks.demoDrawCommits=await evaluate(`document.getElementById('status').textContent.startsWith('1 objects')`);
    const drawn=await send('Page.captureScreenshot',{format:'png'});
    await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo-stroke.png'),Buffer.from(drawn.data,'base64'));
  }
  observation.status=Object.values(observation.checks).every(Boolean)&&!observation.exceptions.length&&!observation.errors.length?'PASS':'FAIL';
  if(observation.status!=='PASS')failure=Error('Ink v2 browser checks failed');
} catch(error){failure=error;observation.status='ERROR';observation.error=String(error.stack||error);}
finally{
  socket?.close();if(browser){const exited=new Promise(r=>browser.exitCode!==null?r():browser.once('exit',r));browser.kill();await Promise.race([exited,delay(5000)]);}
  if(server)await new Promise(r=>server.close(r));
  const output=path.resolve(process.argv[4]);await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(observation,null,2)+'\n');console.log(JSON.stringify(observation));
  await fs.rm(root,{recursive:true,force:true,maxRetries:6,retryDelay:100});
}
process.exitCode=failure?1:0;
