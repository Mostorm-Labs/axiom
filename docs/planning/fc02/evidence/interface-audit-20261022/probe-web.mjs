import assert from 'node:assert/strict';
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';

// Diagnostic probe: success means expected implementation gaps reproduced, not FC02 acceptance.
// node --experimental-websocket <this-file> [url] [result.json]
// The flag is needed by Node 20; AXIOM_CHROME can override the executable.
const url = process.argv[2] || 'http://127.0.0.1:4174/index.html';
const output = path.resolve(process.argv[3] || 'out/fc02-resume/interface-audit-web.json');
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'axiom-interface-audit-'));
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
  type,x:origin.x+x,y:origin.y+y,button:type==='mouseMoved'?(extra.buttons?'left':'none'):'left',...extra,
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

const results={url,browser:browser.Browser,mode:'Synthetic Chrome/CDP and direct public JS/WASM consumer calls; not physical IME',findings:[]};
try {
  await call('Runtime.enable');await call('Page.enable');await call('Network.enable');
  await call('Network.setCacheDisabled',{cacheDisabled:true});await call('Page.navigate',{url});
  await waitFor('!!globalThis.axiomMinimalHost');await flush();
  results.observation=await observation();
  origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
  await evaluate("document.getElementById('textTool').click()");await waitFor('axiomMinimalHost.observation().tool===4');
  await click(200,180);await flush();
  results.initial=await snapshot();assert.equal(results.initial.phase,1);
  results.formatRaw=await evaluate("(()=>{const h=axiomMinimalHost,m=h.module,t=m.stringToNewUTF8(JSON.stringify(h.textInput.snapshot().token));try{return m._axiom_web_text_intent(h.handle,15,t,0,0,320);}finally{m._free(t)}})()");
  results.afterFormat=await evaluate("axiomMinimalHost.textInput.intent(15,{format:{bold:true}})");
  results.formatError=await evaluate("axiomMinimalHost.textInput.error()");
  results.snapshotFields=Object.keys(results.afterFormat);
  results.consumerFields=await evaluate('Object.keys(axiomMinimalHost.textInput)');
  await call('Input.imeSetComposition',{text:'abcd',selectionStart:1,selectionEnd:1});await flush();
  results.compositionOne=await snapshot();
  results.nativeSelectionOne=await evaluate("[document.getElementById('axiom-ime').selectionStart,document.getElementById('axiom-ime').selectionEnd]");
  await call('Input.imeSetComposition',{text:'abcd',selectionStart:3,selectionEnd:3});await flush();
  results.compositionThree=await snapshot();
  results.nativeSelectionThree=await evaluate("[document.getElementById('axiom-ime').selectionStart,document.getElementById('axiom-ime').selectionEnd]");
  await call('Input.imeSetComposition',{text:'',selectionStart:0,selectionEnd:0});await flush();
  await evaluate('axiomMinimalHost.textInput.intent(5,{width:420})');await flush();
  results.widthBeforeZero=(await snapshot()).width;
  results.widthAfterZero=await evaluate('axiomMinimalHost.textInput.intent(5,{width:0})');
  const keys=['format','formatObject','fonts','mixedFormat','preeditSelection'];
  results.findings=[
    {id:'WEB_FORMAT_BRIDGE',reproduced:results.formatRaw===4&&!!results.formatError,actual:results.formatRaw},
    {id:'WEB_READBACK_FIELDS',reproduced:keys.every(k=>!results.snapshotFields.includes(k)),absent:keys.filter(k=>!results.snapshotFields.includes(k))},
    {id:'WEB_PREEDIT_CURSOR',reproduced:results.nativeSelectionOne[0]===1&&results.nativeSelectionThree[0]===3&&JSON.stringify(results.compositionOne.geometry.caret)===JSON.stringify(results.compositionThree.geometry.caret),actual:{native:[results.nativeSelectionOne,results.nativeSelectionThree],runtime:[results.compositionOne.geometry.caret,results.compositionThree.geometry.caret]}},
    {id:'WEB_ZERO_WIDTH',reproduced:results.widthBeforeZero===420&&results.widthAfterZero.width===320,actual:results.widthAfterZero.width}
  ];
  assert.ok(results.findings.every(f=>f.reproduced),'Audit hypothesis not reproduced');
  results.result='EXPECTED_GAPS_REPRODUCED';
}catch(error){results.result='PROBE_FAILED';results.error=String(error.stack||error);process.exitCode=1;}
finally{results.errors=errors;fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(results,null,2));console.log(JSON.stringify({result:results.result,findings:results.findings,error:results.error},null,2));await Promise.race([call('Browser.close'),pause(1000)]).catch(()=>{});socket.close();}
