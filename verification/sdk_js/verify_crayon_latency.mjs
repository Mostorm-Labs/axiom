// Controlled same-machine comparison; CDP round-trip plus two RAFs, not frame time.
// node verify_crayon_latency.mjs <built-sdk-dir> <Chrome> <observation.json>
// AXIOM_INK_TEST_DPR=2 selects the high-DPI workload. Does not issue a perf Gate.
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-crayon-latency-'));
const files = path.join(root, 'consumer');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
let browser, server, socket, failure;
const brush=process.env.AXIOM_INK_TEST_BRUSH||'crayon';
if(!['crayon','chalk','watercolor'].includes(brush))throw Error('invalid brush workload');
const observation = {brush,checks: {}, errors: [], exceptions: [], input: 'Chrome CDP mouse; public SDK Demo; per point waits two RAFs; timing includes CDP and scheduling'};
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
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html?brush=${brush}`});
  for(let n=0;!await evaluate(`document.querySelector('#tools')?.getAttribute('aria-busy')==='false'`);++n){if(n>300)throw Error('demo startup');await delay(50);}
  await evaluate(`(()=>{const c=document.getElementById('ink-size');c.value='32';c.dispatchEvent(new Event('change'));})()`);await delay(150);
  const mounted=await evaluate(`(()=>{const r=document.getElementById('canvas').getBoundingClientRect();return {left:r.left,top:r.top,width:r.width,height:r.height};})()`);
  observation.measurements=[];
  for(let row=0;row<2;++row){const durations=[];const y=mounted.top+55+row*90;
    let start=performance.now();await mouse('mousePressed',80,y);durations.push(performance.now()-start);
    for(let k=1;k<=32;++k){start=performance.now();await mouse('mouseMoved',80+k*17,y+Math.sin(k*.1)*18,true);await evaluate('new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)))');durations.push(performance.now()-start);}
    start=performance.now();await mouse('mouseReleased',624,y+Math.sin(3.2)*18);await evaluate('new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)))');const release=performance.now()-start;
    observation.measurements.push({row,point_ms:durations,release_ms:release,p50:[...durations].sort((a,b)=>a-b)[16],p95:[...durations].sort((a,b)=>a-b)[31]});
  }
  observation.checks.demoDrawCommits=await evaluate(`document.getElementById('status').textContent.startsWith('2 objects')`);
  const capture=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'.png'),Buffer.from(capture.data,'base64'));
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
