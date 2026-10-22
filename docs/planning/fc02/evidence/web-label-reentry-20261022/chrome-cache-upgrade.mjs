import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawn,execFileSync} from 'node:child_process';
import http from 'node:http';
import {fileURLToPath} from 'node:url';

// Synthetic browser integration test, not physical IME/touch evidence.
// node --experimental-websocket <this-file> [url] [result.json]
// The flag is needed by Node 20; AXIOM_CHROME can override the executable.
const repo=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../../../../..');
const served=path.resolve(process.argv[2] || path.join(repo,'out/fc02-resume/web-build/apps/axiom_canvas_demo'));
const oldFiles=new Map(['index.html','host.js','text_input.js'].map(name=>[name,execFileSync('git',['show',`2fbba9a8:apps/axiom_canvas_demo/platform/web/${name}`],{cwd:repo})]));
let oldMode=true;
const requests=[];
const server=http.createServer((request,response)=>{
 const u=new URL(request.url,'http://localhost'); const name=decodeURIComponent(u.pathname).replace(/^\//,'')||'index.html';
 const file=path.resolve(served,name);
 if(!file.startsWith(served+path.sep)){response.writeHead(403).end();return;}
 requests.push({url:request.url,oldMode});
 const type=name.endsWith('.wasm')?'application/wasm':name.endsWith('.js')?'application/javascript':name.endsWith('.html')?'text/html':'application/octet-stream';
 response.writeHead(200,{'Content-Type':type,'Cache-Control':name.endsWith('.html')?'no-store':'public, max-age=31536000'});
 if(oldMode&&oldFiles.has(name))response.end(oldFiles.get(name));else fs.createReadStream(file).on('error',()=>response.destroy()).pipe(response);
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const url=`http://127.0.0.1:${server.address().port}/index.html`;
const output = path.resolve(process.argv[3] || 'out/fc02-resume/web-cache-upgrade.json');
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

const results={url,browser:browser.Browser,mode:'Synthetic CDP mouse and text; no physical IME',cases:[]};
try {
  await call('Emulation.setDeviceMetricsOverride',{width:1100,height:850,deviceScaleFactor:1,mobile:false});
  await call('Runtime.enable');await call('Page.enable');await call('Network.enable');
  await call('Network.setCacheDisabled',{cacheDisabled:false});await call('Page.navigate',{url});
  await waitFor('!!globalThis.axiomMinimalHost');await flush();
  // Prime normal browser cache with the actual previous release, then deploy
  // the current files to the same origin. Do not clear/disable the cache.
  oldMode=false;
  await call('Page.navigate',{url:url+'?rev=fresh'}); await pause(500);
  await waitFor('!!globalThis.axiomMinimalHost');await flush();
  origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
  await control('shape',2);
  await mouse('mousePressed',100,180,{buttons:1,clickCount:1});
  await mouse('mouseMoved',400,460,{buttons:1});
  await mouse('mouseReleased',400,460,{buttons:0,clickCount:1}); await flush();
  await control('tool',1);await doubleClick(250,350);
  await call('Input.insertText',{text:'1234567890'});await flush();
  const typed=await snapshot(),q=typed.geometry.caret,end={x:q[0]+2,y:(q[1]+q[5])/2};
  await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await flush();
  await doubleClick(end.x,end.y);const reopened=await snapshot();
  await click(end.x-25,end.y);await flush();const clicked=await snapshot();
  const fresh= requests.filter(r=>!r.oldMode).map(r=>r.url);
  const checks={reopened:reopened.phase===1,clickKeepsEditor:clicked.phase===1,clickMovesCaret:clicked.selection[0]>0&&clicked.selection[0]<10,
    freshEntryModule:fresh.some(u=>u.startsWith('/host.js?rev=fresh')),freshEditorModule:fresh.some(u=>u.startsWith('/text_input.js?rev=fresh')),freshWasm:fresh.some(u=>u.startsWith('/axiom_canvas_demo_web.wasm?rev=fresh')),freshFontData:fresh.some(u=>u.startsWith('/axiom_canvas_demo_web.data?rev=fresh'))};
  const requestStart=requests.length;
  await evaluate('axiomMinimalHost.textInput.finish()');await flush();
  await call('Page.navigate',{url:url+'?plain=1'});await pause(300);await waitFor('!!globalThis.axiomMinimalHost');await flush();
  const fallback=requests.slice(requestStart).map(r=>r.url);
  const fallbackVersions=['host.js','text_input.js','axiom_canvas_demo_web.js','axiom_canvas_demo_web.wasm','axiom_canvas_demo_web.data'].map(name=>{
    const match=fallback.find(u=>u.startsWith('/'+name+'?rev='));return match?new URL(match,url).searchParams.get('rev'):null;
  });
  checks.buildFingerprintFallback=fallbackVersions.every(Boolean)&&new Set(fallbackVersions).size===1&&/^[a-f0-9]{64}$/.test(fallbackVersions[0]);
  results.cases.push({checks,typed,reopened,clicked,requests,fallbackVersions});
  results.result=results.cases.every(c=>Object.values(c.checks).every(Boolean))&&errors.length===0?'PASS':'FAIL';
  if(results.result==='FAIL')process.exitCode=1;
}catch(error){results.result='FAIL';results.error=String(error.stack||error);process.exitCode=1;}
finally{results.errors=errors;fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(results,null,2));console.log(JSON.stringify({result:results.result,cases:results.cases.map(c=>({preset:c.preset,checks:c.checks})),composition:results.composition?.checks,error:results.error},null,2));await Promise.race([call('Browser.close'),pause(1000)]).catch(()=>{});socket.close();server.closeAllConnections();server.close();}
