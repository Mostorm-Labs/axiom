import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-rounded-font-')),files=path.join(root,'consumer');
const result={checks:{},errors:[],exceptions:[],files:{},input:'Chrome CDP mouse + public root SDK in an external consumer directory'};
const delay=ms=>new Promise(r=>setTimeout(r,ms));let server,browser,socket,send,failure;
try {
 await fs.mkdir(files);
 for(const name of ['axiom.mjs','axiom.d.mts','package.json','internal','axiom.wasm','axiom.data','pointer_input.mjs','text_input.mjs','example.html']){await fs.cp(path.resolve(process.argv[2],name),path.join(files,name),{recursive:true});if(name!=='internal')result.files[name]=createHash('sha256').update(await fs.readFile(path.join(files,name))).digest('hex');}
 result.files['internal/axiom.mjs']=createHash('sha256').update(await fs.readFile(path.join(files,'internal/axiom.mjs'))).digest('hex');
 await fs.copyFile(new URL('./rounded-font.html',import.meta.url),path.join(files,'index.html'));await fs.copyFile(new URL('./rounded-font-matrix.html',import.meta.url),path.join(files,'matrix.html'));
 if(process.argv[5]){for(const name of ['fixture.bin','native-render.png'])await fs.copyFile(path.resolve(process.argv[5],name),path.join(files,name));await fs.copyFile(path.join(files,'fixture.bin'),path.join(files,'rounded-fixture.bin'));}
 server=http.createServer(async(req,res)=>{try{const name=path.resolve(files,'.'+new URL(req.url,'http://localhost').pathname.replace(/\/$/,'/index.html'));if(!name.startsWith(files+path.sep))throw Error('path');res.setHeader('Content-Type',name.endsWith('.mjs')?'text/javascript':name.endsWith('.wasm')?'application/wasm':name.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(name));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const started=await launchChrome(process.argv[3],path.join(root,'profile'));browser=started.browser;const page=await fetch(`http://127.0.0.1:${started.port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});let serial=0;const pending=new Map();
 socket.addEventListener('message',event=>{const m=JSON.parse(event.data);if(m.id){const p=pending.get(m.id);if(p){pending.delete(m.id);clearTimeout(p.timer);m.error?p.reject(Error(JSON.stringify(m.error))):p.resolve(m.result);}}else if(m.method==='Runtime.exceptionThrown')result.exceptions.push(m.params);});
 send=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout '+method));},90000);pending.set(id,{resolve,reject,timer});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(r.exceptionDetails)throw Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
 result.browser=await send('Browser.getVersion');await send('Runtime.enable');await send('Page.enable');await send('Emulation.setDeviceMetricsOverride',{width:900,height:850,deviceScaleFactor:1,mobile:false});await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/`});for(let i=0;!await evaluate('Boolean(window.probe)');++i){if(i>300)throw Error('startup');await delay(50);}await evaluate('probe.ready');result.buildInfo=await evaluate('probe.info()');
 const mouse=(type,x,y,held=false)=>send('Input.dispatchMouseEvent',{type,x,y,button:type==='mouseMoved'&&!held?'none':'left',buttons:type==='mousePressed'||held?1:0,force:type==='mousePressed'||held?.5:0,clickCount:1});
 const click=async(x,y)=>{await mouse('mouseMoved',x,y);await mouse('mousePressed',x,y);await delay(30);await mouse('mouseReleased',x,y);await delay(150);};
 const drag=async(x,y,endX,endY)=>{await mouse('mouseMoved',x,y);await mouse('mousePressed',x,y);await delay(30);await mouse('mouseMoved',(x+endX)/2,(y+endY)/2,true);await delay(30);await mouse('mouseMoved',endX,endY,true);await delay(30);await mouse('mouseReleased',endX,endY);await delay(180);};
 const checks=async expression=>Object.assign(result.checks,(await evaluate(expression)).checks);
 await checks('probe.start()');await drag(40,40,240,170);await evaluate('probe.select()');await click(130,100);await checks('probe.rectangle()');await delay(200);const preview=await send('Page.captureScreenshot',{format:'png'});await checks('probe.commitPreview()');await delay(200);const committed=await send('Page.captureScreenshot',{format:'png'});result.checks.radiusPreviewCommitPixelsContinuous=preview.data===committed.data;await checks('probe.commit()');await click(790,640);await click(41,41);await checks('probe.cornerMiss()');await evaluate('probe.triangleMode()');await drag(40,280,240,450);await evaluate('probe.select()');await click(140,390);await checks('probe.triangle()');await drag(280,350,400,450);await evaluate('probe.select()');await click(340,400);const connector=await evaluate('probe.ellipse()');Object.assign(result.checks,connector.checks);await click(...connector.mid);const bend=await evaluate('probe.connector()');Object.assign(result.checks,bend.checks);await click(790,640);await click(...bend.elbow);await checks('probe.bendMiss()');await evaluate('probe.multiMode()');await drag(5,5,795,645);await checks('probe.multi()');await checks('probe.fonts()');await checks('probe.restoreRemote()');const output=await evaluate('probe.pixels()');await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-render.png'),Buffer.from(output.bytes));const done=await evaluate('probe.close()');result.errors=done.errors;if(process.argv[5]){const fixture=await evaluate('probe.fixture()');Object.assign(result.checks,fixture.checks);result.pixels=fixture.pixels;await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-native-web.png'),Buffer.from(fixture.bytes));}
 await send('Emulation.setDeviceMetricsOverride',{width:1100,height:900,deviceScaleFactor:1,mobile:false});await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/matrix.html`});for(let i=0;!await evaluate("document.getElementById('status')?.textContent.includes('样例已就绪')");++i){if(i>300)throw Error('gallery startup');await delay(50);}await delay(300);result.checks.galleryActuallyReady=await evaluate("!document.getElementById('export').disabled");const gallery=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-gallery.png'),Buffer.from(gallery.data,'base64'));


 // Exercise the actual Demo controls through real browser mouse/keyboard input.
 await send('Emulation.setDeviceMetricsOverride',{width:1280,height:900,deviceScaleFactor:1,mobile:false});
 await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/example.html`});
 for(let i=0;!await evaluate("document.getElementById('tools')?.getAttribute('aria-busy')==='false'");++i){if(i>300)throw Error('Demo startup');await delay(50);}
 result.checks.demoFourRows=await evaluate("document.querySelectorAll('#tools>nav').length===4&&document.querySelectorAll('#tools>nav')[3].contains(document.getElementById('undo'))&&document.querySelectorAll('#tools>nav')[1].contains(document.getElementById('text-font'))");
 const domClick=async selector=>{const b=await evaluate(`(()=>{const r=document.querySelector(${JSON.stringify(selector)}).getBoundingClientRect();return [r.x+r.width/2,r.y+r.height/2]})()`);await click(...b);};
 await domClick('[data-tool="tool:4"]');
 result.checks.demoFontBeforeTextHint=await evaluate("document.getElementById('text-font').disabled&&document.getElementById('text-parameters').textContent.includes('点击画板')");
 let box=await evaluate("(()=>{const r=document.querySelector('#canvas canvas').getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}})()");
 await click(box.x+90,box.y+80);
 result.checks.demoOriginalDefaultFont=await evaluate("document.getElementById('text-font').selectedOptions[0].textContent.includes('Noto')");
 // Select the native control using its change event (same contract as Playwright selectOption).
 // Mouse input, Unicode input and slider gestures below still use Chrome CDP.
 await evaluate("(()=>{const s=document.getElementById('text-font');s.value=[...s.options].find(o=>o.textContent.includes('Excalifont')).value;s.dispatchEvent(new Event('change',{bubbles:true}));})()");await delay(200);
 result.demoFontSelection='native select change event; confirmed independently with CUA selectOption';
 result.checks.demoFontSelector=await evaluate("document.getElementById('text-font').selectedOptions[0].textContent.includes('Excalifont')&&!document.getElementById('text-font').disabled");
 await click(box.x+90,box.y+80);await send('Input.insertText',{text:'Canvas 2323 我爱我家 繁體中文'});await delay(300);
 result.checks.demoRealMixedTextInput=await evaluate("document.getElementById('status').dataset.error==='false'&&document.getElementById('status').textContent.startsWith('1 objects')");
 const textShot=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo-font.png'),Buffer.from(textShot.data,'base64'));
 await domClick('[data-tool="shape:0"]');
 box=await evaluate("(()=>{const r=document.querySelector('#canvas canvas').getBoundingClientRect();return {x:r.x,y:r.y,width:r.width,height:r.height}})()");
 await drag(box.x+440,box.y+70,box.x+720,box.y+260);await domClick('[data-tool="tool:1"]');await click(box.x+560,box.y+150);
 const slider=await evaluate("(()=>{const s=document.getElementById('corner-radius');const r=s.getBoundingClientRect();return {x:r.x+12,y:r.y+r.height/2,end:r.right-12,before:document.getElementById('corner-radius-value').value}})()");
 result.demoSlider=slider;await drag(slider.x,slider.y,slider.end,slider.y);await delay(250);
 result.checks.demoRealRadiusDrag=await evaluate("Number(document.getElementById('corner-radius-value').value)>50&&document.getElementById('status').dataset.error==='false'");
 await domClick('#undo');await delay(150);
 result.checks.demoRadiusSingleUndo=await evaluate(`document.getElementById('corner-radius-value').value===${JSON.stringify(slider.before)}`);
 await mouse('mousePressed',slider.x,slider.y);await mouse('mouseMoved',slider.end,slider.y,true);await delay(150);await send('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await send('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await mouse('mouseReleased',slider.end,slider.y);await delay(200);
 result.checks.demoRadiusEscapeCancel=await evaluate(`document.getElementById('corner-radius-value').value===${JSON.stringify(slider.before)}&&document.getElementById('status').dataset.error==='false'`);
 const demo=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-demo.png'),Buffer.from(demo.data,'base64'));

 result.status=Object.values(result.checks).every(Boolean)&&!result.errors.length&&!result.exceptions.length?'PASS':'FAIL';if(result.status!=='PASS')failure=Error('extended checks');
} catch(error){failure=error;result.status='ERROR';result.error=String(error.stack||error);try{const r=await send('Runtime.evaluate',{expression:'probe.diagnostics()',returnByValue:true});result.diagnostics=r.result?.value;const shot=await send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.resolve(process.argv[4]).replace(/\.json$/,'-failure.png'),Buffer.from(shot.data,'base64'));}catch{}}
finally{socket?.close();if(browser){const done=new Promise(r=>browser.exitCode!==null?r():browser.once('exit',r));browser.kill();await Promise.race([done,delay(5000)]);}if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.resolve(process.argv[4]),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));await fs.rm(root,{recursive:true,maxRetries:6,retryDelay:100});}
process.exitCode=failure?1:0;
