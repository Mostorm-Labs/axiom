import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-export-web-'));
const files = path.join(root, 'consumer');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
let browser, server, socket, failure;
const downloadNames = new Map(), completedDownloads = new Set();
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
  await fs.copyFile(new URL('./image_export.html', import.meta.url), path.join(files, 'index.html'));
  await fs.copyFile(new URL('./image_export_erased.bin',import.meta.url),path.join(files,'image_export_erased.bin'));
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
    else if (message.method === 'Browser.downloadWillBegin') downloadNames.set(message.params.guid, message.params.suggestedFilename);
    else if (message.method === 'Browser.downloadProgress' && message.params.state === 'completed')
      completedDownloads.add(downloadNames.get(message.params.guid));
  });
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++serial;
    const timer = setTimeout(() => { pending.delete(id); reject(Error(`CDP timeout: ${method}`)); }, 90000);
    pending.set(id, {resolve, reject, timer}); socket.send(JSON.stringify({id, method, params}));
  });
  const evaluate = async expression => {
    const result = await send('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
    if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  observation.browserVersion = await send('Browser.getVersion');
  await send('Runtime.enable'); await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride', {width: 900, height: 750, deviceScaleFactor: 1, mobile: false});
  await send('Page.navigate', {url: `http://127.0.0.1:${server.address().port}/`});
  for (let attempt = 0; !await evaluate('Boolean(window.probe)'); ++attempt) {
    if (attempt > 300) throw Error('SDK page did not initialize');
    await delay(50);
  }
  await evaluate('probe.ready');
  observation.buildInfo = await evaluate('probe.info()');
  const draw=async(x,y,dx=100,dy=80,release=true)=>{
    const r=await evaluate('probe.rect()');
    await send('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x+x,y:r.y+y,button:'left',buttons:1,clickCount:1});await delay(30);
    if(dx||dy)await send('Input.dispatchMouseEvent',{type:'mouseMoved',x:r.x+x+dx,y:r.y+y+dy,button:'left',buttons:1});await delay(30);
    if(release)await send('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x+x+dx,y:r.y+y+dy,button:'left',buttons:0,clickCount:1});
    await delay(100);
  };
  Object.assign(observation.checks,await evaluate('probe.initial()'));
  await evaluate("probe.sdk.selectShape('rectangle')");await draw(40,40);
  await evaluate("probe.sdk.selectTool('select')");await draw(80,80,0,0);
  await evaluate("probe.sdk.setAppearance({fill:[1,0,0,1],strokeEnabled:false,fillEnabled:true})");
  await evaluate("probe.sdk.transformSelection([1,0,0,1,-100,20])");
  await evaluate("probe.sdk.selectShape('rectangle')");await draw(40,40);
  await evaluate("probe.sdk.selectTool('select')");await draw(80,80,0,0);
  await evaluate("probe.sdk.setAppearance({fill:[0,0,1,1],strokeEnabled:false,fillEnabled:true})");
  await evaluate("probe.sdk.transformSelection([1,0,0,1,400,20])");
  Object.assign(observation.checks,await evaluate('probe.content()'));
  await evaluate("probe.sdk.selectShape('rectangle')");await draw(10,160,40,40,false);
  Object.assign(observation.checks,await evaluate('probe.creation()'));
  const r=await evaluate('probe.rect()');await send('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x+50,y:r.y+200,button:'left',buttons:0,clickCount:1});
  await evaluate('probe.sdk.undo()');
  await evaluate("probe.sdk.selectTool('ink')");await draw(20,160,80,10,false);
  Object.assign(observation.checks,await evaluate("probe.creation('activeInkExcluded')"));
  await send('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x+100,y:r.y+170,button:'left',buttons:0,clickCount:1});await delay(100);
  await evaluate('probe.sdk.undo()');
  Object.assign(observation.checks,await evaluate('probe.imageAndText()'));
  Object.assign(observation.checks,await evaluate('probe.erasedInk()'));
  observation.errors=await evaluate('probe.errors');
  await evaluate('probe.close()');
  observation.checks.closed=await evaluate('probe.closed()');
  observation.checks.noRuntimeErrors=observation.errors.length===0;
  // Exercise the actual Demo Blob/download flow into an isolated temporary directory.
  const downloads=path.join(root,'downloads');await fs.mkdir(downloads);
  await send('Browser.setDownloadBehavior',{behavior:'allow',downloadPath:downloads,eventsEnabled:true});
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
  for(let n=0;!await evaluate("Boolean(document.getElementById('export-image'))&&document.getElementById('tools').getAttribute('aria-busy')!=='true'");++n){if(n>300)throw Error('Demo export controls unavailable');await delay(50);}
  const downloaded={};
  for(const format of ['png','jpg','jpeg']){
    await evaluate(`(()=>{const select=document.getElementById('export-format');select.value='${format}';select.dispatchEvent(new Event('change'));document.getElementById('export-image').click();})()`);
    const filename=path.join(downloads,`axiom-board.${format}`);
    // Chrome can create the destination before its bytes are complete.
    // Wait for the actual download completion fact before decoding the file.
    for(let n=0;!completedDownloads.has(`axiom-board.${format}`);++n){if(n>150)throw Error('Demo download incomplete: '+format);await delay(50);}
    downloaded[format]=await fs.readFile(filename);
  }
  observation.checks.demoDownloads=downloaded.png.subarray(0,8).equals(Buffer.from([137,80,78,71,13,10,26,10]))&&['jpg','jpeg'].every(format=>downloaded[format][0]===255&&downloaded[format][1]===216);
  const expectedSize=await evaluate("(()=>{const r=document.getElementById('canvas').getBoundingClientRect();return [Math.ceil(r.width+32),Math.ceil(r.height+32)];})()");
  observation.checks.demoDecodedDownloads=true;
  for(const [format,bytes] of Object.entries(downloaded)){
    const result=await evaluate(`(async()=>{const raw=atob('${bytes.toString('base64')}'),bytes=Uint8Array.from(raw,c=>c.charCodeAt(0));const bitmap=await createImageBitmap(new Blob([bytes],{type:'image/${format==='png'?'png':'jpeg'}'}));const c=document.createElement('canvas');c.width=bitmap.width;c.height=bitmap.height;const ctx=c.getContext('2d');ctx.drawImage(bitmap,0,0);const result={size:[bitmap.width,bitmap.height],pixel:Array.from(ctx.getImageData(10,10,1,1).data)};bitmap.close();return result;})()`);
    observation.checks.demoDecodedDownloads&&=JSON.stringify(result.size)===JSON.stringify(expectedSize)&&JSON.stringify(result.pixel)==='[255,255,255,255]';
  }
  observation.downloadSizes=Object.fromEntries(Object.entries(downloaded).map(([key,value])=>[key,value.length]));
  const screenshot=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});
  if(process.argv[4])await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'.png'),Buffer.from(screenshot.data,'base64'));
  observation.checks.noUncaughtExceptions=observation.exceptions.length===0;
  observation.status=Object.values(observation.checks).every(Boolean)?'PASS':'FAIL';
  if(observation.status!=='PASS')failure=Error('Image export checks failed');
} catch (error) {
  failure = error; observation.status = 'ERROR'; observation.error = String(error.stack || error);
} finally {
  socket?.close();
  if (browser) {
    const exited = new Promise(resolve => browser.exitCode !== null ? resolve() : browser.once('exit', resolve));
    browser.kill(); await Promise.race([exited, delay(5000)]);
  }
  if (server) await new Promise(resolve => server.close(resolve));
  if (process.argv[4]) {
    const output = path.resolve(process.argv[4]); await fs.mkdir(path.dirname(output), {recursive: true});
    await fs.writeFile(output, JSON.stringify(observation, null, 2) + '\n');
  }
  console.log(JSON.stringify(observation));
  await fs.rm(root, {recursive: true, maxRetries: 6, retryDelay: 100});
}
process.exitCode = failure ? 1 : 0;
