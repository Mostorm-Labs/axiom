import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';

// Synthetic browser integration test, not physical IME/touch evidence.
// node --experimental-websocket <this-file> [url] [result.json]
// The flag is needed by Node 20; AXIOM_CHROME can override the executable.
const url = process.argv[2] || 'http://127.0.0.1:4174/index.html';
const output = path.resolve(process.argv[3] || 'out/fc02-resume/shape-growth-web.json');
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
const results = {url,browser:browser.Browser,mode:'Headless Chrome SwiftShader; CDP mouse/insertText; no physical input',cases:[]};

const capture = async name => {
  const data = (await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false})).data;
  fs.writeFileSync(path.join(path.dirname(output),name+'.png'),Buffer.from(data,'base64'));
  const shapes = await evaluate(`(async()=>{
    const bitmap=await createImageBitmap(await (await fetch('data:image/png;base64,${data}')).blob());
    const canvas=new OffscreenCanvas(bitmap.width,bitmap.height),ctx=canvas.getContext('2d');
    ctx.drawImage(bitmap,0,0);const p=ctx.getImageData(0,0,bitmap.width,bitmap.height).data;
    return [0,1,2].map(i=>{let left=1e9,top=1e9,right=-1,bottom=-1,count=0;
      for(let y=${Math.round(origin.y)};y<bitmap.height-32;y++)for(let x=50+i*330;x<320+i*330;x++){
        const k=(y*bitmap.width+x)*4;
        if(p[k]>40&&p[k]<95&&p[k+1]>105&&p[k+1]<165&&p[k+2]>175&&p[k+2]<235){
          left=Math.min(left,x);right=Math.max(right,x);top=Math.min(top,y);bottom=Math.max(bottom,y);count++;
        }
      }return {left,top,right,bottom,count};});})()`);
  return {image:name+'.png',shapes};
};
try {
  await call('Emulation.setDeviceMetricsOverride',{width:1100,height:950,deviceScaleFactor:1,mobile:false});
  await call('Runtime.enable');await call('Page.enable');await call('Network.enable');
  await call('Network.setCacheDisabled',{cacheDisabled:true});await call('Page.navigate',{url});
  await waitFor('!!globalThis.axiomMinimalHost');await flush();
  origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
  results.initial=await observation();await evaluate("(()=>{globalThis.dragEvents=[];for(const type of ['pointerdown','pointermove','pointerup','gotpointercapture','lostpointercapture'])document.addEventListener(type,e=>dragEvents.push({type,target:e.target.id,x:e.clientX,y:e.clientY,buttons:e.buttons}),true);return true})()");
  for(let preset=0;preset<3;preset++){
    const x=60+preset*330,y=500,right=x+240,bottom=y+80;
    await control('shape',preset);
    await mouse('mousePressed',x,y,{buttons:1,clickCount:1});
    await mouse('mouseMoved',right,bottom,{buttons:1});
    await mouse('mouseReleased',right,bottom,{buttons:0,clickCount:1});await flush();
    const current={preset,result:'RUNNING'};results.cases.push(current);const drawn=await observation(),before=await capture(`growth-${preset}-before`);
    await evaluate("document.getElementById('textTool').click()");await waitFor('axiomMinimalHost.observation().tool===4');await doubleClick(x+120,y+50);
    assert.equal((await snapshot()).phase,1,'Text/Select double-click enters Shape label');
    const text=Array.from({length:8},(_,i)=>`Line ${i+1} 文字`).join('\n');
    await call('Input.insertText',{text});
    await waitFor(`String.fromCharCode(...axiomMinimalHost.textInput.snapshot().projection)===${JSON.stringify(text)}`);
    await flush();
    const typed=await snapshot(),after=await capture(`growth-${preset}-typed`),typedObservation=await observation();
    Object.assign(current,{drawn,typed,typedObservation,before,after});
    assert.ok(typed.geometry.available,'full label caret remains available after growth');
    assert.equal(typedObservation.objects,preset+1,'typing keeps the same Shape');
    assert.equal(typedObservation.operations,drawn.operations+2,'confirmed text and undoable geometry growth');
    assert.ok(after.shapes[preset].top<before.shapes[preset].top-20,'overflow grows the painted shape upward');
    // Fill pixels may inset by several pixels as the transformed outline grows;
    // the native oracle separately checks the exact geometric bottom anchor.
    assert.ok(Math.abs(after.shapes[preset].bottom-before.shapes[preset].bottom)<=5,'growth preserves the bottom edge');
    assert.ok(Math.abs(after.shapes[preset].right-before.shapes[preset].right)<=5,'growth preserves width');
    await evaluate('axiomMinimalHost.textInput.finish()');await control('tool',1);
    await click(x+120,y+40);await flush();
    await mouse('mousePressed',right,bottom,{buttons:1,clickCount:1});
    await mouse('mouseMoved',right-80,bottom-60,{buttons:1});await flush();
    const shrinking=await capture(`growth-${preset}-shrinking`);
    await mouse('mouseReleased',right-80,bottom-60,{buttons:0,clickCount:1});await flush();
    const shrunk=await capture(`growth-${preset}-shrunk`),afterShrink=await observation();Object.assign(current,{drawn,typed,typedObservation,before,after,shrunk,afterShrink,events:await evaluate('dragEvents.splice(0)')});
    assert.equal(afterShrink.operations,typedObservation.operations+1,'resize commits exactly once');
    assert.ok(shrunk.shapes[preset].right<after.shapes[preset].right-50,'the actual shape became narrower');
    await click(right-80-16,bottom-60-16);await flush();
    const expanded=await capture(`growth-${preset}-expanded`),afterExpand=await observation();
    assert.equal(afterExpand.operations,afterShrink.operations+1,'the visible expand button performs an Operation in Select');
    assert.ok(expanded.shapes[preset].top<shrunk.shapes[preset].top-20,'button expands upward');
    assert.ok(Math.abs(expanded.shapes[preset].bottom-shrunk.shapes[preset].bottom)<=5,'button preserves bottom');
    await evaluate("document.getElementById('textTool').click()");await waitFor('axiomMinimalHost.observation().tool===4');await doubleClick(x+80,bottom-90);
    assert.equal(String.fromCharCode(...(await snapshot()).projection),text,'ellipsis preserves all canonical text');
    await evaluate('axiomMinimalHost.textInput.finish()');await flush();
    Object.assign(current,{shrinking,expanded,afterExpand,result:'PASS'});
  }
  assert.deepEqual(errors,[],'no uncaught browser exceptions');
  results.final=await observation();results.result='PASS';
} catch(error){results.result='FAIL';results.error=String(error.stack||error);process.exitCode=1;}
finally{
  results.errors=errors;fs.mkdirSync(path.dirname(output),{recursive:true});
  fs.writeFileSync(output,JSON.stringify(results,null,2));console.log(`${results.result}: ${results.cases.length} Shape variants; ${output}`);
  if(results.error)console.error(results.error);await call('Browser.close');socket.close();
}
