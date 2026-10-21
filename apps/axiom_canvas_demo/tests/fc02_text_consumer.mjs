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

const results={url,browser:browser.Browser,mode:'Synthetic CDP; ordinary JS/WASM consumer; not physical IME',cases:[]};
const reset=async()=>{
  if(await evaluate('!!globalThis.axiomMinimalHost')){
    await call('Input.imeSetComposition',{text:'',selectionStart:0,selectionEnd:0});
    await evaluate('axiomMinimalHost.textInput.finish().catch(()=>false)');
  }
  await call('Page.navigate',{url});await waitFor('!!globalThis.axiomMinimalHost');await flush();
  origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
};
const begin=async()=>{
  await evaluate("document.getElementById('textTool').click()");await waitFor('axiomMinimalHost.observation().tool===4');
  await click(150,150);await flush();assert.equal((await snapshot()).phase,1);
};
const run=async(name,action)=>{await reset();try{const evidence=await action();results.cases.push({name,result:'PASS',evidence});}catch(error){results.cases.push({name,result:'FAIL',error:String(error.stack||error)});}};
try{
  await call('Emulation.setDeviceMetricsOverride',{width:1100,height:850,deviceScaleFactor:1,mobile:false});
  await call('Runtime.enable');await call('Page.enable');await call('Network.enable');await call('Network.setCacheDisabled',{cacheDisabled:true});
  await run('font catalog and per-resource readback available to ordinary JS consumer',async()=>{
    const value=await evaluate("(()=>{const t=axiomMinimalHost.textInput,s=t.snapshot(),catalog=t.fontCatalog();return {s,catalog,ready:t.fontStatus(catalog.entries[0].id),missing:t.fontStatus('ffffffffffffffffffffffffffffffff')}})()");
    assert.equal(value.catalog.entries.length,12);assert.equal(value.s.fonts.length,12);
    assert.ok(value.catalog.entries.every(f=>f.family&&f.contentHash.startsWith('sha256:')&&f.byteLength>0));
    assert.equal(value.missing,null);assert.equal(value.ready.id,value.catalog.entries[0].id);
    assert.ok('format' in value.s && 'formatObject' in value.s && 'mixedFormat' in value.s && 'preeditSelection' in value.s);
    return value;
  });
  await run('format patch and explicit receipt identities support replay and reject collisions',async()=>{
    await begin();await call('Input.insertText',{text:'ABcd'});await flush();
    await evaluate('axiomMinimalHost.textInput.intent(12)');
    const value=await evaluate("(async()=>{const t=axiomMinimalHost.textInput;const id=t.fontCatalog().entries.find(f=>f.family.includes('Noto')).id;const patch={font:id,color:[0.125,0.25,0.75,1],fontSize:24,bold:true,italic:true,underline:true,strikethrough:true,bulleted:true,alignment:1};const options={clientId:'901',requestId:'1',token:t.snapshot().token,format:patch};const formatted=await t.intent(15,options),receipt=t.receipt('901','1'),before=axiomMinimalHost.observation();await t.intent(15,options);const replay=t.receipt('901','1'),afterReplay=axiomMinimalHost.observation();await t.intent(15,{...options,format:{bold:false}});return {patch,formatted,receipt,replay,before,afterReplay,final:t.snapshot(),error:t.error()};})()");
    assert.deepEqual(value.formatted.format,value.patch);assert.equal(value.receipt.status,3);
    assert.equal(value.replay.status,3);
    assert.equal(value.before.operations,value.afterReplay.operations);assert.equal(value.final.format.bold,true);
    assert.match(value.error,/\(7\)/);return value;
  });
  await run('zero width and invalid format fail without changing accepted text or width',async()=>{
    await begin();await call('Input.insertText',{text:'Unchanged'});await flush();
    const value=await evaluate("(async()=>{const t=axiomMinimalHost.textInput;await t.intent(5,{width:420});const before=axiomMinimalHost.observation();await t.intent(5,{clientId:'902',requestId:'1',width:0});const zero=t.receipt('902','1');await t.intent(15,{clientId:'902',requestId:'2',format:{fontSize:-1,bold:true}});return {before,after:axiomMinimalHost.observation(),zero,invalid:t.receipt('902','2'),s:t.snapshot()};})()");
    assert.equal(value.zero.status,4);assert.equal(value.invalid.status,4);assert.equal(value.s.width,420);
    assert.equal(value.after.operations,value.before.operations);assert.equal(String.fromCharCode(...value.s.projection),'Unchanged');assert.equal(value.s.format.bold,false);
    return value;
  });
  await run('native preedit caret offsets reach Runtime geometry without canonical writes',async()=>{
    await begin();
    await call('Input.imeSetComposition',{text:'abcd',selectionStart:1,selectionEnd:1});await flush();await pause(60);
    const one=await snapshot();
    await call('Input.imeSetComposition',{text:'abcd',selectionStart:3,selectionEnd:3});await flush();await pause(60);
    const three=await snapshot();assert.deepEqual(one.preeditSelection,[1,0]);assert.deepEqual(three.preeditSelection,[3,0]);
    assert.ok(one.geometry.caret[0]<three.geometry.caret[0]);assert.equal((await observation()).objects,0);assert.equal((await observation()).operations,0);
    await call('Input.imeSetComposition',{text:'',selectionStart:0,selectionEnd:0});await flush();
    return {one,three};
  });
  await run('selected closed Shape label accepts ordinary-consumer whole-label formatting',async()=>{
    await control('shape',2);
    await mouse('mousePressed',100,250,{buttons:1,clickCount:1});await mouse('mouseMoved',340,430,{buttons:1});await mouse('mouseReleased',340,430,{buttons:0,clickCount:1});await flush();
    await control('tool',1);await doubleClick(220,370);assert.equal((await snapshot()).phase,1);
    await call('Input.insertText',{text:'Shape label'});await flush();await evaluate('axiomMinimalHost.textInput.finish()');await flush();
    const value=await evaluate("(async()=>{const t=axiomMinimalHost.textInput,before=t.snapshot(),font=t.fontCatalog().entries.find(f=>f.family==='Inter');const after=await t.intent(15,{clientId:'903',requestId:'1',object:before.formatObject,format:{font:font.id,bold:true,italic:true,color:[0,0,1,1]}});return {before,after,font,receipt:t.receipt('903','1'),objects:axiomMinimalHost.observation().objects};})()");
    assert.equal(value.before.phase,0);assert.notEqual(value.before.formatObject,'00000000000000000000000000000000');assert.equal(value.objects,1);
    assert.equal(value.receipt.status,3);assert.equal(value.after.format.font,value.font.id);assert.equal(value.after.format.bold,true);assert.equal(value.after.format.italic,true);
    return value;
  });
  results.result=results.cases.every(c=>c.result==='PASS')&&errors.length===0?'PASS':'FAIL';if(results.result==='FAIL')process.exitCode=1;
}catch(error){results.result='FAIL';results.error=String(error.stack||error);process.exitCode=1;}
finally{results.errors=errors;fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(results,null,2));console.log(JSON.stringify({result:results.result,cases:results.cases.map(c=>({name:c.name,result:c.result,error:c.error})),error:results.error},null,2));await Promise.race([call('Browser.close'),pause(1000)]).catch(()=>{});socket.close();}
