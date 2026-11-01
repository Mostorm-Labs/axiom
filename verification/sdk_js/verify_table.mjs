import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-table-web-'));
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
  await fs.copyFile(new URL('./table.html', import.meta.url), path.join(files, 'index.html'));
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
  Object.assign(observation.checks,(await evaluate('probe.run()')).checks);
  await evaluate('probe.edit()');
  await send('Input.insertText',{text:' input'});
  await evaluate('probe.finish()');
  observation.checks.nativeTextInput=(await evaluate('probe.read().cells.some(c=>c.text.includes(" input"))'));
  await evaluate('probe.interact()');
  const mouse=async(type,p,extra={})=>send('Input.dispatchMouseEvent',{type,x:p.x,y:p.y,button:type==='mouseMoved'?'none':'left',buttons:type==='mousePressed'?1:0,clickCount:1,...extra});
  const click=async(p,count=1)=>{await mouse('mousePressed',p,{clickCount:count});await mouse('mouseReleased',p,{clickCount:count});await delay(150);};
  await click({x:790,y:490});
  observation.checks.deselectOutside=(await evaluate('probe.state().selected.length===0'));
  const cell=await evaluate('probe.point(60,20)');await click(cell);
  observation.checks.pointerReselectsTable=(await evaluate('probe.state().selected[0]===probe.read().objectId'));
  const edge=await evaluate('probe.point(160,20)'),moved=await evaluate('probe.point(184,20)');
  const gen=await evaluate('probe.state().generation');await mouse('mousePressed',edge);await mouse('mouseMoved',moved,{buttons:1});
  observation.checks.resizePreviewReadOnly=(await evaluate('probe.state().generation'))===gen;
  await mouse('mouseReleased',moved);await delay(200);
  observation.checks.pointerResize=Math.abs((await evaluate('probe.read().columns[0].size'))-184)<1;
  const edge2=await evaluate('probe.point(184,20)'),moved2=await evaluate('probe.point(200,20)');const gen2=await evaluate('probe.state().generation');
  await mouse('mousePressed',edge2);await mouse('mouseMoved',moved2,{buttons:1});await evaluate('probe.cancelPointer()');await mouse('mouseReleased',moved2);await delay(150);
  observation.checks.cancelResize=(await evaluate('probe.state().generation'))===gen2&&Math.abs((await evaluate('probe.read().columns[0].size'))-184)<1;
  await click(cell);await click(cell,2);
  observation.checks.doubleClickEditor=await evaluate('probe.text().phase===1&&probe.text().token.row===probe.read().rows[0].id');
  await send('Input.insertText',{text:'新单元格'});await delay(150);await evaluate('probe.finish()');
  observation.checks.doubleClickText=await evaluate('probe.read().cells[0].text==="新单元格"');
  await evaluate('probe.focus()');const original=await evaluate('probe.target().columnId');
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Tab',code:'Tab',windowsVirtualKeyCode:9});await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Tab',code:'Tab',windowsVirtualKeyCode:9});await delay(150);
  observation.checks.tabNavigates=(await evaluate('probe.target().columnId'))!==original;
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await delay(150);
  observation.checks.enterEdits=await evaluate('probe.text().phase===1');await evaluate('probe.finish()');
  const screenshot=await send('Page.captureScreenshot' ,{format:'png'});
  if(process.argv[4])await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'.png'),Buffer.from(screenshot.data,'base64'));
  observation.result=await evaluate('probe.read()');
  await evaluate('probe.close()');
  observation.checks.cleanClose=true;
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
  for(let n=0;!await evaluate("document.getElementById('tools')?.getAttribute('aria-busy')==='false'");++n){if(n>300)throw Error('Demo failed to initialize');await delay(50);}
  await evaluate("document.getElementById('table-open').click();document.getElementById('table-rows').value='2';document.getElementById('table-columns').value='3';document.getElementById('table-place').click()");await delay(200);
  const placement=await evaluate("(()=>{const r=document.querySelector('#canvas canvas').getBoundingClientRect();return {x:r.left+120,y:r.top+100};})()");await click(placement);await delay(200);
  observation.checks.demoTableControls=await evaluate("!document.getElementById('table-parameters').hidden&&document.getElementById('table-row-height').value==='48'");
  await evaluate("document.getElementById('table-edit').click()");await delay(150);await send('Input.insertText',{text:'表格 Demo'});await delay(150);
  await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await delay(150);
  await evaluate("const h=document.getElementById('table-row-height');h.value='64';h.dispatchEvent(new Event('change',{bubbles:true}))");await delay(150);
  observation.checks.demoResize=await evaluate("document.getElementById('table-row-height').value==='64'&&document.getElementById('status').dataset.error==='false'");
  await evaluate("document.getElementById('undo').click()");await delay(150);observation.checks.demoUndo=await evaluate("document.getElementById('table-row-height').value==='48'");
  const demoScreenshot=await send('Page.captureScreenshot',{format:'png'});if(process.argv[4])await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo.png'),Buffer.from(demoScreenshot.data,'base64'));

  observation.checks.noUncaughtExceptions=observation.exceptions.length===0;
  observation.status=Object.values(observation.checks).every(Boolean)?'PASS':'FAIL';
  if(observation.status!=='PASS')failure=Error('Table regression checks failed');
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
