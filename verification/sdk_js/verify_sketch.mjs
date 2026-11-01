import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-sketch-web-'));
const files = path.join(root, 'consumer');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
let browser, server, socket, failure, send;
const observation = {checks: {}, errors: [], exceptions: [], input: 'Chrome CDP mouse, public JS SDK, independent WASM instances'};
try {
  await fs.mkdir(files);
  const hashes = {};
  for (const name of ['axiom.mjs', 'axiom.d.mts', 'package.json', 'internal', 'axiom.wasm', 'axiom.data', 'pointer_input.mjs', 'text_input.mjs', 'example.html']) {
    await fs.cp(path.resolve(process.argv[2], name), path.join(files, name), {recursive: true});
    if (name !== 'internal') hashes[name] = createHash('sha256').update(await fs.readFile(path.join(files, name))).digest('hex');
  }
  hashes['internal/axiom.mjs'] = createHash('sha256').update(await fs.readFile(path.join(files, 'internal/axiom.mjs'))).digest('hex');
  observation.files = hashes;
  await fs.copyFile(new URL('./sketch.html', import.meta.url), path.join(files, 'index.html'));
  if(process.argv[5])for(const name of ['fixture.bin','native-render.png'])await fs.copyFile(path.resolve(process.argv[5],name),path.join(files,name));
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
  send = (method, params = {}) => new Promise((resolve, reject) => {
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
  const mouse=async(type,x,y,extra={})=>send('Input.dispatchMouseEvent',{type,x,y,button:type==='mouseMoved'?(extra.buttons===1?'left':'none'):'left',buttons:type==='mousePressed'?1:0,force:type==='mousePressed'||extra.buttons===1?.5:0,clickCount:1,...extra});
  const drag=async(x,y,x2,y2)=>{await mouse('mouseMoved',x,y);await mouse('mousePressed',x,y);await delay(30);await mouse('mouseMoved',(x+x2)/2,(y+y2)/2,{buttons:1});await delay(30);await mouse('mouseMoved',x2,y2,{buttons:1});await delay(30);await mouse('mouseReleased',x2,y2);await delay(150);};
  const click=async(x,y)=>{await mouse('mouseMoved',x,y);await mouse('mousePressed',x,y);await delay(30);await mouse('mouseReleased',x,y);await delay(150);};
  if(!process.env.AXIOM_SKETCH_DEMO_ONLY){
  await evaluate('probe.ready');await delay(200);
  observation.buildInfo = await evaluate('probe.info()');
  await drag(40,40,210,160);await evaluate('probe.select()');await delay(200);await click(120,90);observation.initialState=await evaluate('({state:probe.state(),read:probe.read()})');
  Object.assign(observation.checks,(await evaluate('probe.run()')).checks);
  if(process.argv[5]){const pixels=await evaluate('probe.pixels()');await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-render.png'),Buffer.from(pixels.bytes));observation.pixels=pixels.pixels;Object.assign(observation.checks,pixels.checks);observation.checks.sharedNativeFixtureRendered=true;}
  const baselinePreview=await send('Page.captureScreenshot',{format:'png'});
  await evaluate('probe.previewOnly()');await delay(250);const transientPreview=await send('Page.captureScreenshot',{format:'png'});
  observation.checks.previewVisible=baselinePreview.data!==transientPreview.data;
  Object.assign(observation.checks,(await evaluate('probe.commitPreview()')).checks);await delay(250);const committedPreview=await send('Page.captureScreenshot',{format:'png'});
  observation.checks.previewCommitPixelsContinuous=transientPreview.data===committedPreview.data;
  await evaluate('probe.undoPreview()');await delay(150);
  const matrix=[['rectangle',0,'hachure',1,1,[]],['ellipse',1,'crosshatch',2,1,[]],['triangle',2,'solid',4,.5,[]],['rectangle',3,'none',6,1,[8,5]],['sticky',1.5,'crosshatch',2,.7,[]],['line',2,'none',3,1,[]],['arrow',2.5,'none',2,.5,[8,5]],['ellipse',3,'hachure',1,1,[]]];
  for(let i=0;i<matrix.length;i++){await evaluate(`probe.prepare(...${JSON.stringify(matrix[i])})`);const x=30+(i%4)*190,y=210+Math.floor(i/4)*170;await drag(x,y,x+(i===7?20:140),y+(i===7?14:100));const step=await evaluate('probe.diagnostics()');observation.matrixSteps??=[];observation.matrixSteps.push({i,state:step.state,errors:step.errors});if(step.state.activeContacts||step.errors.length)throw Error('matrix input '+i+' '+JSON.stringify(step));}
  Object.assign(observation.checks,(await evaluate('probe.matrixDone()')).checks);
  const screenshot=await send('Page.captureScreenshot',{format:'png'});if(process.argv[4])await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'.png'),Buffer.from(screenshot.data,'base64'));
  await click(70,420);observation.stickyRead=await evaluate('({read:probe.read(),state:probe.state()})');Object.assign(observation.checks,(await evaluate('probe.sticky()')).checks);await evaluate('probe.selectMode()');await delay(200);await drag(10,10,790,540);observation.multiState=await evaluate('({state:probe.state(),read:probe.read()})');Object.assign(observation.checks,(await evaluate('probe.multi()')).checks);
  await evaluate('probe.begin()');await click(785,545);Object.assign(observation.checks,(await evaluate('probe.stale()')).checks);
  const done=await evaluate('probe.close()');observation.errors=done.errors;
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/`});for(let n=0;!await evaluate('Boolean(window.probe)');++n){if(n>300)throw Error('Route fixture initialization');await delay(50);}await evaluate('probe.ready');const routeSetup=await evaluate('probe.routeSetup()');await click(...routeSetup.mid);observation.routeBefore=await evaluate('probe.routePreview()');await delay(600);const routePreview=await send('Page.captureScreenshot',{format:'png'});const routeCommit=await evaluate('probe.routeCommit()');await delay(600);const routeCommitted=await send('Page.captureScreenshot',{format:'png'});Object.assign(observation.checks,routeCommit.checks);observation.routeAfter=routeCommit.route;observation.checks.routeWidthPreviewCommitContinuous=routePreview.data===routeCommitted.data;await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-route-preview.png'),Buffer.from(routePreview.data,'base64'));await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-route-committed.png'),Buffer.from(routeCommitted.data,'base64'));observation.errors.push(...(await evaluate('probe.close()')).errors);

  }
  await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
  for(let n=0;!await evaluate("document.getElementById('tools')?.getAttribute('aria-busy')==='false'");++n){if(n>300)throw Error('Demo failed');await delay(50);}
  await evaluate("[...document.querySelectorAll('#drawing-tools button')].find(b=>b.textContent==='矩形').onclick()");await delay(150);
  observation.checks.demoDefaultControls=await evaluate("!document.getElementById('sketch-parameters').hidden");
  await evaluate("(async()=>{const enabled=document.getElementById('sketch-enabled');enabled.checked=true;await enabled.onchange();const fill=document.getElementById('sketch-fill-kind');fill.value='crosshatch';await fill.onchange();})()");await delay(200);
  await evaluate('new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))');
  const rect=await evaluate("(()=>{const r=document.querySelector('#canvas canvas').getBoundingClientRect();return {x:r.x,y:r.y,w:r.width,h:r.height}})()");observation.demoRect=rect;await evaluate(`(()=>{window.demoInputLog=[];window.demoStatusLog=[];new MutationObserver(records=>{for(const r of records)if(r.type==='childList')for(const n of [...r.addedNodes,...r.removedNodes])window.demoStatusLog.push(n.textContent);}).observe(document.getElementById('status'),{childList:true,subtree:true});for(const type of ['pointerdown','pointermove','pointerup','pointercancel','gotpointercapture','lostpointercapture'])document.addEventListener(type,e=>window.demoInputLog.push({type,target:e.target.id,x:e.clientX,y:e.clientY,buttons:e.buttons,pressure:e.pressure}),true);})()`);observation.demoTarget=await evaluate(`document.elementFromPoint(${rect.x+80},${rect.y+50})?.outerHTML`);await mouse('mouseMoved',rect.x+80,rect.y+50);await mouse('mousePressed',rect.x+80,rect.y+50);await delay(150);await mouse('mouseMoved',rect.x+180,rect.y+120,{buttons:1});await delay(150);await mouse('mouseMoved',rect.x+300,rect.y+190,{buttons:1});await delay(150);await mouse('mouseReleased',rect.x+300,rect.y+190);await delay(200);observation.demoInputLog=await evaluate('window.demoInputLog');observation.demoStatusLog=await evaluate('window.demoStatusLog');observation.demoAfterDraw=await evaluate("({status:document.getElementById('status').textContent,error:document.getElementById('status').dataset.error,context:document.getElementById('context-title').textContent,shape:document.querySelector('[data-tool=\"shape:0\"]').getAttribute('aria-pressed')})");const drawShot=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-draw.png'),Buffer.from(drawShot.data,'base64'));
  await evaluate("document.querySelector('[data-tool=\"tool:1\"]').onclick()");await click(rect.x+180,rect.y+120);await delay(150);
  observation.checks.demoSelectedControls=await evaluate("!document.getElementById('sketch-parameters').hidden&&document.getElementById('status').textContent.includes('1 selected')");
  observation.demoSelection=await evaluate("({status:document.getElementById('status').textContent,error:document.getElementById('status').dataset.error,context:document.getElementById('context-title').textContent,hidden:document.getElementById('sketch-parameters').hidden})");
  if(!observation.checks.demoSelectedControls)throw Error('Demo selected appearance controls missing');
  await evaluate("const slider=document.getElementById('sketch-roughness');slider.dispatchEvent(new PointerEvent('pointerdown'));slider.value='2.4';slider.dispatchEvent(new Event('input'));slider.dispatchEvent(new Event('change'))");await delay(250);
  observation.checks.demoSlider=await evaluate("document.getElementById('sketch-degree').value==='2.40'&&document.getElementById('status').dataset.error==='false'");
  const sliderBounds=await evaluate("(()=>{const r=document.getElementById('sketch-roughness').getBoundingClientRect();return{x:r.x+10,y:r.y+r.height/2,end:r.right-10}})()");
  await drag(sliderBounds.x,sliderBounds.y,sliderBounds.end,sliderBounds.y);await delay(200);
  const degreeAfterDrag=await evaluate("document.getElementById('sketch-degree').value");observation.sliderDegreeAfterDrag=degreeAfterDrag;observation.sliderBounds=sliderBounds;observation.checks.realSliderDrag=Number(degreeAfterDrag)>2.5&&Number(degreeAfterDrag)<=3;
  await evaluate("document.getElementById('undo').onclick()");await delay(200);observation.checks.realSliderSingleUndo=await evaluate("document.getElementById('sketch-degree').value==='2.40'");
  await mouse('mousePressed',sliderBounds.x,sliderBounds.y);await delay(50);await mouse('mouseMoved',sliderBounds.end,sliderBounds.y,{buttons:1});await delay(100);await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await delay(100);await mouse('mouseReleased',sliderBounds.end,sliderBounds.y);await delay(200);
  observation.checks.realSliderEscapeCancel=await evaluate("document.getElementById('sketch-degree').value==='2.40'&&document.getElementById('status').dataset.error==='false'");

  const shot=await send('Page.captureScreenshot',{format:'png'});if(process.argv[4])await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo.png'),Buffer.from(shot.data,'base64'));
  observation.status=Object.values(observation.checks).every(Boolean)&&!observation.errors.length&&!observation.exceptions.length?'PASS':'FAIL';
  if(observation.status!=='PASS')failure=Error('Sketch checks failed');
} catch (error) {
  failure = error;try{const shot=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-failure.png'),Buffer.from(shot.data,'base64'));}catch{} observation.status = 'ERROR'; observation.error = String(error.stack || error);
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
