import assert from 'node:assert/strict';
import fs from 'node:fs';
import {createHash} from 'node:crypto';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';

// Synthetic browser integration test, not physical IME/touch evidence.
// node --experimental-websocket <this-file> [url] [result.json]
// The flag is needed by Node 20; AXIOM_CHROME can override the executable.
const url = process.argv[2] || 'http://127.0.0.1:4174/index.html';
const output = path.resolve(process.argv[3] || 'out/fc02-resume/web-reentry.json');
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
  await call('Network.setCacheDisabled',{cacheDisabled:true});await call('Page.navigate',{url});
  await waitFor('!!globalThis.axiomMinimalHost');await flush();
  origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
  await evaluate("(()=>{globalThis.inputEvents=[];for(const type of ['pointerdown','pointerup','dblclick','focusin','focusout'])document.addEventListener(type,e=>inputEvents.push({type,target:e.target.id,focus:document.activeElement?.id,phase:axiomMinimalHost.textInput.snapshot().phase,selection:axiomMinimalHost.textInput.snapshot().selection}),true);return true})()");
  for (const dpr of [1,2]) {
    await call('Emulation.setDeviceMetricsOverride',{width:1100,height:850,deviceScaleFactor:dpr,mobile:false});
    await call('Page.navigate',{url}); await waitFor('!!globalThis.axiomMinimalHost'); await flush();
    origin=await evaluate("(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left,y:r.top}})()");
    await evaluate("(()=>{globalThis.inputEvents=[];for(const type of ['pointerdown','pointerup','dblclick','focusin','focusout'])document.addEventListener(type,e=>inputEvents.push({type,target:e.target.id,focus:document.activeElement?.id,phase:axiomMinimalHost.textInput.snapshot().phase,composition:axiomMinimalHost.textInput.snapshot().composition,selection:axiomMinimalHost.textInput.snapshot().selection}),true);return true})()");
    for(let preset=0;preset<3;preset++){
      const x=60+preset*330,y=180;
      await control('shape',preset);
      await mouse('mousePressed',x,y,{buttons:1,clickCount:1});
      await mouse('mouseMoved',x+240,y+180,{buttons:1});
      await mouse('mouseReleased',x+240,y+180,{buttons:0,clickCount:1});await flush();
      await evaluate("document.getElementById('textTool').click()");await waitFor('axiomMinimalHost.observation().tool===4');
      await doubleClick(x+120,y+110);
      const label=preset===2?'1234567890123456789012345678901234567890':'你好 Axiom 这是中文测试，用于检查再次进入文本编辑。';
      if(preset===2)await call('Input.imeSetComposition',{text:label,selectionStart:label.length,selectionEnd:label.length});
      await call('Input.insertText',{text:label});await flush();
      const typed=await snapshot(),g=typed.geometry.caret;
      const end={x:g[0]+2,y:(g[1]+g[5])/2};
      await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
      await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});await flush();
      const ended=await snapshot();
      await control('tool',1); await doubleClick(end.x,end.y);
      const reopened=await snapshot(),samples=[];
      for(let i=0;i<9;i++){
        const image=await call('Page.captureScreenshot',{format:'png',clip:{x:origin.x+end.x-3,y:origin.y+g[1]-2,width:8,height:g[5]-g[1]+4,scale:1}});
        const sample=await evaluate('({frame:axiomMinimalHost.observation().presentedFrames,time:performance.now()})');
        samples.push({...sample,hash:createHash('sha256').update(image.data).digest('hex')});
        await pause(155);
      }
      await click(end.x-35,end.y);await flush();const middle=await snapshot();
      await click(end.x,end.y);await flush();const clicked=await snapshot();
      await call('Input.insertText',{text:'!'});await flush();const inserted=await snapshot();
      await click(1050,650);await flush();const outside=await snapshot();
      await doubleClick(end.x,end.y);const afterOutside=await snapshot();
      await click(end.x-35,end.y);await flush();const afterOutsideClick=await snapshot();
      await evaluate('axiomMinimalHost.textInput.finish()');await flush();const idleStart=(await observation()).presentedFrames;await pause(750);
      const idleEnd=(await observation()).presentedFrames;
      const checks={noIdleFramesAfterFinish:idleStart===idleEnd,compositionEnded:typed.composition==='0',boundedBlinkFrames:samples.at(-1).frame-samples[0].frame<=Math.ceil((samples.at(-1).time-samples[0].time)/500)+1,enterClosed:ended.phase===0,reopened:reopened.phase===1,reopenedAtHit:reopened.selection[0]>=label.length-1,
        idleBlink:new Set(samples.map(s=>s.hash)).size>1,clickMiddle:middle.phase===1&&middle.selection[0]>0&&middle.selection[0]<label.length,
        clickEnd:clicked.phase===1&&clicked.selection[0]===label.length,inserted:String.fromCharCode(...inserted.projection)===label+'!',
        outsideClosed:outside.phase===0,reopenAfterOutside:afterOutside.phase===1,clickAfterOutside:afterOutsideClick.phase===1};
      results.cases.push({dpr,preset,checks,typed,reopened,middle,clicked,outside,afterOutside,afterOutsideClick,samples,events:await evaluate('inputEvents.splice(0)')});
      await evaluate('axiomMinimalHost.textInput.finish()');await flush();
    }
  }
  results.result=results.cases.every(c=>Object.values(c.checks).every(Boolean))&&errors.length===0?'PASS':'FAIL';
  if(results.result==='FAIL')process.exitCode=1;
}catch(error){results.result='FAIL';results.error=String(error.stack||error);process.exitCode=1;}
finally{results.errors=errors;fs.mkdirSync(path.dirname(output),{recursive:true});fs.writeFileSync(output,JSON.stringify(results,null,2));console.log(JSON.stringify({result:results.result,cases:results.cases.map(c=>({preset:c.preset,checks:c.checks})),composition:results.composition?.checks,error:results.error},null,2));await Promise.race([call('Browser.close'),pause(1000)]).catch(()=>{});socket.close();}
