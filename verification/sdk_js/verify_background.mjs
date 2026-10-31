import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-background-web-'));
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
  await fs.copyFile(new URL('./background.html', import.meta.url), path.join(files, 'index.html'));
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
  const legacy = await evaluate('probe.legacyBytes');
  observation.legacySnapshotSha256 = createHash('sha256').update(new Uint8Array(legacy)).digest('hex');
  if (process.argv[5]) {
    const baseline = JSON.parse(await fs.readFile(process.argv[5], 'utf8'));
    if (baseline.legacySnapshotSha256 !== observation.legacySnapshotSha256) throw Error('Legacy snapshot bytes changed');
    observation.checks.legacySnapshotBytes = true;
  }
  const result = await evaluate('probe.run()');
  Object.assign(observation.checks, result.checks);
  observation.pixels = {dots:result.dotsCount,grid:result.gridCount};
  observation.diagnostics = result.diagnostics;
  observation.errors = result.errors;
  await evaluate('probe.close()');
  observation.checks.cleanClose = true;
  observation.checks.noRuntimeErrors = observation.errors.length === 0;
  observation.checks.noUncaughtExceptions = observation.exceptions.length === 0;
  await send('Page.navigate', {url:`http://127.0.0.1:${server.address().port}/example.html`});
  for (let n=0;!await evaluate("Boolean(document.getElementById('background-apply'))&&document.getElementById('tools').getAttribute('aria-busy')!=='true'");++n) {
    if(n>300)throw Error('Demo background controls did not initialize');
    await delay(50);
  }
  const demoPixel = () => evaluate(`(()=>{const input=document.querySelector('#canvas canvas');const c=document.createElement('canvas');c.width=input.width;c.height=input.height;const ctx=c.getContext('2d');ctx.drawImage(input,0,0);return Array.from(ctx.getImageData(12,12,1,1).data);})()`);
  observation.checks.demoDefaultTool=await evaluate("Array.from(document.querySelectorAll('button')).find(button=>button.textContent==='Ink')?.getAttribute('aria-pressed')==='true'");
  let beforeDraft;
  for(let n=0;n<100;++n){beforeDraft=await demoPixel();if(beforeDraft[3]===255)break;await delay(25);}
  await evaluate(`(()=>{for(const [id,value] of [['background-mode','grid'],['background-color','#00ff00'],['background-pattern-color','#ff0000']]){const input=document.getElementById(id);input.value=value;input.dispatchEvent(new Event('input',{bubbles:true}));}})()`);
  observation.checks.demoDraft=JSON.stringify(await demoPixel())===JSON.stringify(beforeDraft);
  await evaluate("document.getElementById('background-cancel').click()");
  observation.checks.demoCancel=await evaluate("document.getElementById('background-mode').value==='plain'");
  await evaluate(`(()=>{for(const [id,value] of [['background-mode','grid'],['background-color','#00ff00'],['background-pattern-color','#ff0000']]){const input=document.getElementById(id);input.value=value;input.dispatchEvent(new Event('input',{bubbles:true}));}document.getElementById('background-apply').click();})()`);
  let applied;
  for(let n=0;n<100;++n){applied=await demoPixel();if(applied[1]===255&&applied[0]===0)break;await delay(25);}
  observation.checks.demoApply=applied[0]===0&&applied[1]===255&&applied[2]===0;
  observation.demoStatus=await evaluate("document.getElementById('status').textContent");
  if(process.argv[4]){
    const screenshot=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});
    const output=path.resolve(process.argv[4]).replace(/\.json$/,'.png');
    await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,Buffer.from(screenshot.data,'base64'));
  }
  observation.checks.noUncaughtExceptions=observation.exceptions.length===0;
  observation.status = Object.values(observation.checks).every(Boolean) ? 'PASS' : 'FAIL';
  if (observation.status !== 'PASS') failure = Error('Background regression checks failed');
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
