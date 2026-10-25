// Run against the built Minimal Host, e.g. node web_image_picker_test.mjs http://127.0.0.1:4181/.
// CDP supplies trusted mouse input and intercepts the real browser file chooser.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {spawn} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const base = process.argv[2] || 'http://127.0.0.1:4181/';
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'axiom-image-picker-'));
const chrome = spawn(process.env.CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe', [
  '--headless=new', '--no-first-run', '--no-default-browser-check', `--user-data-dir=${profile}`,
  '--remote-debugging-address=127.0.0.1', '--remote-debugging-port=0',
  '--enable-unsafe-swiftshader', '--use-angle=swiftshader', 'about:blank',
], {windowsHide:true, stdio:'ignore'});
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
const portFile = path.join(profile, 'DevToolsActivePort');
let socket;
try {
  for (let i=0; !fs.existsSync(portFile) && i<100; ++i) await sleep(100);
  const port = fs.readFileSync(portFile, 'utf8').split('\n')[0];
  const page = await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, {method:'PUT'}).then(r=>r.json());
  socket = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise(resolve => socket.addEventListener('open', resolve, {once:true}));
  let serial=0;
  const pending = new Map(), choosers = [];
  socket.addEventListener('message', event => {
    const message=JSON.parse(event.data);
    if (message.id) {
      const task=pending.get(message.id); pending.delete(message.id);
      if (message.error) task.reject(message.error); else task.resolve(message.result);
    } else if (message.method==='Page.fileChooserOpened') choosers.push(message.params);
  });
  const call = (method, params={}) => new Promise((resolve,reject) => {
    const id=++serial; pending.set(id,{resolve,reject}); socket.send(JSON.stringify({id,method,params}));
  });
  const evaluate = async expression => {
    const result=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});
    assert(!result.exceptionDetails, JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  const click = async (x,y) => {
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x,y});
    await call('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});
    await sleep(50);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:1});
    await sleep(100);
  };
  await call('Page.enable');
  await call('Page.setInterceptFileChooserDialog',{enabled:true});
  await call('Emulation.setDeviceMetricsOverride',{width:1400,height:1000,deviceScaleFactor:1,mobile:false});
  await call('Network.enable'); await call('Network.setCacheDisabled',{cacheDisabled:true});
  await call('Page.navigate',{url:new URL('index.html?rev=image-picker-test',base).href});
  for(let i=0; !(await evaluate('!!window.axiomMinimalHost')) && i<150; ++i) await sleep(200);
  assert(await evaluate('!!window.axiomMinimalHost'), 'Minimal Host ready');
  const toggle=await evaluate('(()=>{const r=document.getElementById("debugUiToggle").getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()');
  await click(toggle.x,toggle.y);
  const panel=await evaluate('(()=>{const r=document.getElementById("debugUi").getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}})()');
  const clickPanel = (x,y) => click(panel.x+x*panel.width/410,panel.y+y*panel.height/650);
  const waitFor = async (expression,message) => {
    for(let i=0; i<60; ++i) { if(await evaluate(expression)) return; await sleep(100); }
    const shot=await call('Page.captureScreenshot',{format:'png'});
    fs.writeFileSync(path.join(profile,'failure.png'),Buffer.from(shot.data,'base64'));
    console.error('Failure screenshot:',path.join(profile,'failure.png'));
    console.error('State:',await evaluate('axiomMinimalHost.observation()'));
    assert.fail(message);
  };
  const selectPath = async (chooser,relativePath) => {
    const file=fileURLToPath(new URL(`../../../${relativePath}`,import.meta.url));
    await call('DOM.setFileInputFiles',{backendNodeId:chooser.backendNodeId,files:[file]});
  };
  const selectFile = (chooser,name) => selectPath(chooser,`verification/fc03/fixtures/${name}`);
  // Fixed logical panel size; this is the rendered ImGui Import button, not a test backdoor.
  await clickPanel(75,386);
  for(let i=0; choosers.length===0 && i<30; ++i) await sleep(100);
  assert.equal(choosers.length,1,'Import image must open one browser file chooser');
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),0,'Opening picker does not create an object');
  await selectFile(choosers[0],'fc03-sample.jpg');
  await waitFor('axiomMinimalHost.observation().objects===1','JPEG selected through browser chooser was not imported');
  await evaluate('axiomMinimalHost.control("undo")');
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),0,'Import Undo');
  await evaluate('axiomMinimalHost.control("redo")');
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),1,'Import Redo');
  // Restore selection by clicking the imported image, then the real Replace button.
  await evaluate('axiomMinimalHost.control("tool",1)');
  const canvas=await evaluate('(()=>{const r=document.getElementById("axiom-canvas").getBoundingClientRect();return {x:r.x,y:r.y}})()');
  await click(canvas.x+500,canvas.y+150);
  const beforeReplace=await evaluate('axiomMinimalHost.observation().operations');
  // Selected-object actions add one separator/button row before Image.
  await clickPanel(235,445);
  await waitFor('document.querySelector("input[type=file]")?.value===""','Replacement picker reset');
  assert.equal(choosers.length,2,'Replace must open a second browser chooser');
  await selectFile(choosers[1],'fc03-red.jpg');
  await waitFor(`axiomMinimalHost.observation().operations>${beforeReplace}`,'Replace not committed');
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),1,'Replace preserves object count');
  const replaced=await evaluate('axiomMinimalHost.observation()');
  await clickPanel(75,445);
  assert.equal(choosers.length,3,'Picker can reopen after replacement');
  // Chrome CDP has no native Cancel command; dispatch the browser cancel event.
  await evaluate('document.querySelector("input[type=file]").dispatchEvent(new Event("cancel"))');
  await sleep(150);
  assert.equal(await evaluate('axiomMinimalHost.observation().operations'),replaced.operations,'Cancel must not write history');
  await clickPanel(75,445);
  assert.equal(choosers.length,4,'Picker can reopen after cancellation');
  await selectFile(choosers[3],'fc03-sample.webp');
  await waitFor('axiomMinimalHost.observation().objects===2','WebP selected through browser chooser was not imported');
  const beforeInvalid=await evaluate('axiomMinimalHost.observation().operations');
  await clickPanel(75,445);
  assert.equal(choosers.length,5,'Picker reopens for error recovery');
  await selectPath(choosers[4],'apps/axiom_canvas_demo/CMakeLists.txt');
  await sleep(300);
  assert.equal(await evaluate('axiomMinimalHost.observation().operations'),beforeInvalid,'Invalid file must not write history');
  await clickPanel(75,445);
  assert.equal(choosers.length,6,'Picker reopens after invalid file');
  await selectPath(choosers[5],'verification/fixtures/g47/image-stripes.png');
  await waitFor('axiomMinimalHost.observation().objects===3','PNG selected through browser chooser was not imported');
  assert.equal(await evaluate('axiomMinimalHost.observation().failure'),'','No Web host failure');
  await evaluate('axiomMinimalHost.control("fit")');
  const fitted=await evaluate('axiomMinimalHost.observation()'),camera=fitted.camera;
  // Earlier JPEG dimensions remain 138x138 after replacement. A later small
  // PNG must not become the sole document extent used by Fit Content.
  assert(450*camera.scale+camera.x>=30 && 100*camera.scale+camera.y>=30 &&
    588*camera.scale+camera.x<=fitted.logicalWidth-30 &&
    238*camera.scale+camera.y<=fitted.logicalHeight-30,'Fit Content clipped an earlier imported image');
  // Mixed nine-object scene matching the reported interaction: real image
  // imports, Ink, a Shape, then two attachment clicks that each create a pair.
  await evaluate('(async()=>{await axiomMinimalHost.control("setZoom",0,0,1);const c=axiomMinimalHost.observation().camera;await axiomMinimalHost.control("pan",c.x,c.y)})()');
  const dragCanvas=async(x,y,dx,dy)=>{
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:canvas.x+x,y:canvas.y+y});
    await call('Input.dispatchMouseEvent',{type:'mousePressed',x:canvas.x+x,y:canvas.y+y,button:'left',clickCount:1});
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:canvas.x+dx,y:canvas.y+dy,button:'left',buttons:1});
    await sleep(50);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:canvas.x+dx,y:canvas.y+dy,button:'left',clickCount:1});
    await evaluate('axiomMinimalHost.flush()');
  };
  await evaluate('axiomMinimalHost.control("tool",0)');await dragCanvas(1100,100,1200,180);
  await evaluate('axiomMinimalHost.control("shape",0)');await dragCanvas(1150,500,1250,600);
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),5,'Mixed fixture Ink/Shape creation');
  await evaluate('axiomMinimalHost.control("tool",1)');
  await click(canvas.x+1200,canvas.y+550);await click(canvas.x+1278,canvas.y+550);
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),7,'Right attachment creates a pair');
  await click(canvas.x+1200,canvas.y+550);await click(canvas.x+1200,canvas.y+628);
  assert.equal(await evaluate('axiomMinimalHost.observation().objects'),9,'Bottom attachment creates a pair');
  await evaluate('axiomMinimalHost.control("fit")');
  const mixed=await evaluate('axiomMinimalHost.observation()');
  for(const [x,y] of [[450,100],[588,238],[1100,100],[1200,180],[1150,500],[1250,600]]) {
    const sx=x*mixed.camera.scale+mixed.camera.x,sy=y*mixed.camera.scale+mixed.camera.y;
    assert(sx>=30 && sx<=mixed.logicalWidth-30 && sy>=30 && sy<=mixed.logicalHeight-30,
      'Fit after attachment creation lost earlier mixed-scene content');
  }
  console.log('PASS: trusted Import/Replace chooser, PNG/JPEG/WebP, Undo/Redo, cancel/error/reopen; mixed 9-object attachment creation / Fit');
  await call('Browser.close');
} finally {
  socket?.close();
  chrome.kill();
}
