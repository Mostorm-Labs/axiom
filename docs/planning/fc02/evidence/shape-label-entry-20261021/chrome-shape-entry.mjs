import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';

// Synthetic browser integration test, not physical IME/touch evidence.
// node --experimental-websocket <this-file> [url] [result.json]
// The flag is needed by Node 20; AXIOM_CHROME can override the executable.
const url = process.argv[2] || 'http://127.0.0.1:4173/index.html';
const output = path.resolve(process.argv[3] || 'out/fc02-resume/shape-web-result.json');
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'axiom-shape-entry-'));
const chrome = spawn(process.env.AXIOM_CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe', [
  '--headless=new', '--no-first-run', '--no-default-browser-check',
  `--user-data-dir=${profile}`, '--remote-debugging-address=127.0.0.1',
  '--remote-debugging-port=0', '--enable-unsafe-swiftshader', '--use-angle=swiftshader', 'about:blank',
], {windowsHide:true, stdio:'ignore'});
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
const portFile = path.join(profile, 'DevToolsActivePort');
for (let i = 0; i < 100 && !fs.existsSync(portFile); ++i) await pause(100);
assert.ok(fs.existsSync(portFile), 'Chrome did not start');
const port = fs.readFileSync(portFile, 'utf8').split('\n')[0];
const browser = await fetch(`http://127.0.0.1:${port}/json/version`).then(r => r.json());
const page = await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, {method:'PUT'}).then(r => r.json());
const socket = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(resolve => socket.addEventListener('open', resolve, {once:true}));
let serial = 0;
const pending = new Map(), errors = [];
socket.addEventListener('message', event => {
  const message = JSON.parse(event.data);
  if (message.id) {
    const promise = pending.get(message.id);
    pending.delete(message.id);
    message.error ? promise.reject(message.error) : promise.resolve(message.result);
  } else if (message.method === 'Runtime.exceptionThrown') errors.push(message.params);
});
const call = (method, params = {}) => new Promise((resolve, reject) => {
  const id = ++serial;
  pending.set(id, {resolve,reject});
  socket.send(JSON.stringify({id,method,params}));
});
const evaluate = async expression => {
  const result = await call('Runtime.evaluate', {expression,returnByValue:true,awaitPromise:true});
  assert.ok(!result.exceptionDetails, JSON.stringify(result.exceptionDetails));
  return result.result.value;
};
const waitFor = async expression => {
  for (let i = 0; i < 150; ++i) {
    if (await evaluate(expression)) return;
    await pause(100);
  }
  assert.fail(`Timed out: ${expression}`);
};
const observation = () => evaluate('axiomMinimalHost.observation()');
const snapshot = () => evaluate('axiomMinimalHost.textInput.snapshot()');
const flush = () => evaluate('axiomMinimalHost.flush().then(()=>true)');
const control = (kind, ...values) => evaluate(`axiomMinimalHost.control(${JSON.stringify(kind)},${values.join(',') || '0'})`);
let origin;
const mouse = (type,x,y,extra={}) => call('Input.dispatchMouseEvent', {
  type,x:origin.x+x,y:origin.y+y,button:type==='mouseMoved'?'none':'left',...extra,
});
const click = async (x,y,count=1) => {
  await mouse('mousePressed',x,y,{buttons:1,clickCount:count});
  await mouse('mouseReleased',x,y,{buttons:0,clickCount:count});
};
const doubleClick = async (x,y) => {
  await click(x,y);
  await pause(60);
  await click(x,y,2);
  await pause(200);
  await flush();
};
const results = {url,browser:browser.Browser,mode:'Headless Chrome SwiftShader; CDP mouse/insertText; no physical input',cases:[]};
try {
  await call('Emulation.setDeviceMetricsOverride',{width:1100,height:900,deviceScaleFactor:1,mobile:false});
  await call('Runtime.enable');
  await call('Page.enable');
  await call('Network.enable');
  await call('Network.setCacheDisabled',{cacheDisabled:true});
  await call('Page.navigate',{url});
  await waitFor('!!globalThis.axiomMinimalHost');
  await flush();
  origin = await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
  results.initial = await observation();
  await evaluate(`(()=>{globalThis.shapeEntryEvents=[];for(const type of ['pointerdown','pointerup','click','dblclick'])document.addEventListener(type,event=>shapeEntryEvents.push({type,target:event.target.id,detail:event.detail,x:event.clientX,y:event.clientY}),true);return true})()`);
  for (let preset = 0; preset < 3; ++preset) {
    const x = preset === 1 ? 420 : 100, y = preset === 2 ? 300 : 100;
    const center = [x+100,y+45];
    await control('shape',preset);
    await mouse('mousePressed',x,y,{buttons:1,clickCount:1});
    await mouse('mouseMoved',x+200,y+90,{buttons:1});
    await mouse('mouseReleased',x+200,y+90,{buttons:0,clickCount:1});
    await flush();
    const drawn = await observation();
    const current = {preset,drawn,result:'RUNNING'};
    results.cases.push(current);
    assert.equal(drawn.objects,preset+1,'one new Shape');
    await evaluate("document.getElementById('textTool').click()");
    await waitFor('axiomMinimalHost.observation().tool===4');
    await doubleClick(...center);
    const edit = await snapshot();
    current.edit = edit;
    current.events = await evaluate('shapeEntryEvents.splice(0)');
    current.target = await evaluate(`(()=>{const e=document.elementFromPoint(${origin.x+center[0]},${origin.y+center[1]});return {id:e?.id,active:document.activeElement?.id}})()`);
    assert.ok(current.events.some(event=>event.type==='dblclick' && event.target==='axiom-canvas'),
      'the real double-click reaches the canvas, not the hidden native input');
    assert.equal(edit.phase,1,'Text double-click enters Editing');
    assert.equal(current.target.active,'axiom-ime','double-click transfers native keyboard focus to the editor');
    assert.ok(edit.geometry.available,'real caret layout');
    assert.ok(Math.abs(edit.width-200)<0.01,`label uses Shape width: ${edit.width}`);
    assert.equal((await observation()).operations,drawn.operations,'empty label adds no Operation');
    const object = edit.token.object;
    const text = `你好 Shape ${preset}`;
    await call('Input.insertText',{text});
    await waitFor(`String.fromCharCode(...axiomMinimalHost.textInput.snapshot().projection)===${JSON.stringify(text)}`);
    const typed = await snapshot();
    assert.equal(typed.token.object,object,'label stays on original Shape');
    await evaluate('axiomMinimalHost.textInput.finish()');
    await flush();
    assert.equal((await snapshot()).phase,0);
    assert.equal((await observation()).objects,preset+1,'no separate RichText object');
    await control('undo');
    await doubleClick(...center);
    const undone = await snapshot();
    assert.equal(undone.token.object,object);
    assert.deepEqual(undone.projection,[],'Undo restores unlabeled Shape');
    await evaluate('axiomMinimalHost.textInput.finish()');
    await control('redo');
    await control('tool',1); // Select
    await doubleClick(...center);
    const redone = await snapshot();
    assert.equal(redone.phase,1,'Select double-click reenters label');
    assert.equal(redone.token.object,object);
    assert.equal(String.fromCharCode(...redone.projection),text,'Redo restores text');
    assert.equal((await observation()).objects,preset+1);
    await evaluate('axiomMinimalHost.textInput.finish()');
    Object.assign(current,{typed,undone,redone,result:'PASS'});
  }
  await control('tool',0); // Ink
  await doubleClick(200,145);
  assert.equal((await snapshot()).phase,0,'Ink double-click never edits text');
  results.inkDoubleClick = 'PASS';
  results.final = await observation();
  assert.deepEqual(errors,[],'no uncaught browser exceptions');
  results.result = 'PASS';
} catch (error) {
  results.result = 'FAIL';
  results.error = String(error.stack || error);
  process.exitCode = 1;
} finally {
  results.errors = errors;
  fs.mkdirSync(path.dirname(output),{recursive:true});
  fs.writeFileSync(output,JSON.stringify(results,null,2));
  console.log(`${results.result}: ${results.cases.length} Shape variants; Ink ${results.inkDoubleClick || 'NOT_RUN'}; ${output}`);
  if (results.error) console.error(results.error);
  await call('Browser.close');
  socket.close();
}
