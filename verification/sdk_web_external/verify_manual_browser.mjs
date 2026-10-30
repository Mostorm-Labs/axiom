import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import {spawn} from 'node:child_process';

const build=path.resolve(process.argv[2]), chrome=path.resolve(process.argv[3]);
const delay=ms=>new Promise(r=>setTimeout(r,ms));
const profile=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-external-chrome-'));
let server,badServer,browser,socket,failed=''; const events=[];
class CDP {
  constructor(ws){this.ws=ws;this.id=0;this.pending=new Map();ws.addEventListener('message',e=>{
    const m=JSON.parse(e.data); if(!m.id){events.push(m);return;}
    const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);clearTimeout(p.timer);
    m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);
  });}
  send(method,params={}){const id=++this.id;return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('CDP timeout '+method)),30000);
    this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));
  });}
}
try {
  server=spawn(process.env.AXIOM_SDK_PYTHON||'python',[path.join(build,'serve.py'),'--port','0','--directory',build],{stdio:['ignore','pipe','pipe'],windowsHide:true});
  let base='',serverError=''; server.stdout.on('data',chunk=>{base+=String(chunk);});server.stderr.on('data',chunk=>{serverError+=String(chunk);});
  for(let i=0;i<200 && !base.includes('\n') && server.exitCode===null;i++)await delay(50);
  base=base.trim();if(!/^http:\/\/127\.0\.0\.1:\d+\/$/.test(base))throw new Error('SDK server did not start: '+serverError);
  const moduleResponse=await fetch(base+'pointer_input.mjs');
  if(!moduleResponse.ok || !/javascript/.test(moduleResponse.headers.get('content-type')||''))throw new Error('SDK server does not serve ES modules as JavaScript');
  const args=['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader',
    '--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  browser=spawn(chrome,args,{stdio:'ignore',windowsHide:true});
  const portFile=path.join(profile,'DevToolsActivePort'); let port='';
  for(let i=0;i<200&&!port;i++){try{port=(await fs.readFile(portFile,'utf8')).split('\n')[0];}catch{await delay(50);}}
  if(!port)throw new Error('Chrome did not start');
  const page=await fetch(`http://127.0.0.1:${port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());
  socket=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  const cdp=new CDP(socket);await cdp.send('Runtime.enable');await cdp.send('Page.enable');
  await cdp.send('Emulation.setDeviceMetricsOverride',{width:1024,height:768,deviceScaleFactor:1,mobile:false});
  await cdp.send('Page.navigate',{url:base+'index.html'});
  const evaluate=async expression=>{const r=await cdp.send('Runtime.evaluate',{expression,returnByValue:true});
    if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  async function until(expression,label){for(let i=0;i<600;i++){
    const state=await evaluate(`(()=>{let e=document.getElementById('result');return e?{text:e.textContent,preview:e.dataset.preview,lost:e.dataset.lost,restored:e.dataset.restored}:null})()`);
    if(state?.text?.startsWith('FAIL')||state?.text?.startsWith('BLOCKED'))throw new Error(JSON.stringify(state));
    if(await evaluate(expression))return state; await delay(50);
  }throw new Error('timeout '+label);}
  await until(`document.getElementById('result')?.textContent==='READY'`,'ready');
  if (!await evaluate(`document.body.classList.contains('manual')`)) throw new Error('default page is not interactive');
  const initialStage=await evaluate(`(()=>{const r=document.getElementById('stage').getBoundingClientRect();return {width:r.width,height:r.height}})()`);
  if(initialStage.width < 600 || initialStage.height < 400) throw new Error('interactive canvas is not responsive '+JSON.stringify(initialStage));
  await cdp.send('Emulation.setDeviceMetricsOverride',{width:900,height:700,deviceScaleFactor:2,mobile:false});
  await until(`(()=>{const r=document.getElementById('stage').getBoundingClientRect(),c=document.getElementById('surface');return c.width===Math.round(r.width*2) && c.height===Math.round(r.height*2) && Module._sdk_logical_width()===r.width})()`,'resize and DPR');
  await until(`window.externalTextInput && !document.getElementById('external-text-tool').hidden`,'text tool');
  const point=async(x,y)=>await evaluate(`(()=>{const r=document.getElementById('stage').getBoundingClientRect();return {x:r.left+${x},y:r.top+${y}}})()`);
  const clickAndDrag=async(start,end)=>{
    const a=await point(...start), b=await point(...end);
    await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',...a,button:'left',buttons:1,clickCount:1});
    await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',...b,button:'left',buttons:1});
    await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',...b,button:'left',buttons:0,clickCount:1});
  };
  const clickButton=async(id)=>{
    const p=await evaluate(`(()=>{const r=document.getElementById('${id}').getBoundingClientRect();return {x:r.left+r.width/2,y:r.top+r.height/2}})()`);
    await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',...p,button:'left',buttons:1,clickCount:1});
    await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',...p,button:'left',buttons:0,clickCount:1});
  };
  if (!await evaluate(`document.getElementById('external-ink-tool').getAttribute('aria-pressed')==='true'`))
    throw new Error('initial Ink tool is not indicated');
  await clickButton('external-shape');
  await delay(100);
  await clickAndDrag([600,300],[700,380]);
  await until(`Module._sdk_object_count()===1`,'physical button rectangle creation');
  if (!await evaluate(`document.getElementById('external-shape').getAttribute('aria-pressed')==='true'`))
    throw new Error('selected Rectangle tool is not indicated');
  if (await evaluate(`document.getElementById('external-ink-tool').getAttribute('aria-pressed')`) !== 'false')
    throw new Error('previous tool remains indicated');
  await until(`(()=>{const c=document.getElementById('surface'),g=c.getContext('webgl2'),p=new Uint8Array(4),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(1300,c.height-680,1,1,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,f);return p[2]>p[0]+20 && p[3]>0;})()`,'rectangle visible on canvas');
  await clickButton('external-undo');
  await until(`Module._sdk_object_count()===0`,'physical undo button');
  if (!await evaluate(`document.getElementById('external-shape').getAttribute('aria-pressed')==='true'`))
    throw new Error('one-shot action incorrectly changed selected tool');
  await clickButton('external-text-tool');
  await until(`document.getElementById('external-text-tool').getAttribute('aria-pressed')==='true'`,'text selected state');
  const textPoint=await point(90,90);
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',...textPoint,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',...textPoint,button:'left',buttons:0,clickCount:1});
  await until(`document.activeElement?.id==='axiom-ime'`,'IME carrier focus');
  await cdp.send('Input.imeSetComposition',{text:'nihao',selectionStart:5,selectionEnd:5});
  await until(`window.externalTextInput?.snapshot().phase===2`,'IME preedit');
  await cdp.send('Input.insertText',{text:'你好'});
  await until(`String.fromCharCode(...window.externalTextInput.snapshot().projection)==='你好'`,'single IME result');
  if (!await evaluate(`!!document.getElementById('external-font-size')`)) throw new Error('text formatting is missing');
  await cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'a',code:'KeyA',windowsVirtualKeyCode:65,modifiers:2});
  await cdp.send('Input.dispatchKeyEvent',{type:'keyUp',key:'a',code:'KeyA',windowsVirtualKeyCode:65,modifiers:2});
  await until(`window.externalTextInput.snapshot().selection[1]===2`,'select text');
  await evaluate(`(()=>{const e=document.getElementById('external-font-size');e.value='24';e.dispatchEvent(new Event('change',{bubbles:true}));})()`);
  await until(`window.externalTextInput.snapshot().format.fontSize===24`,'text size formatting');
  await cdp.send('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await cdp.send('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
  await until(`window.externalTextInput.snapshot().phase===0`,'finish editor');
  const beforeInk = await evaluate('Module._sdk_object_count()');
  await evaluate(`document.getElementById('external-ink-tool').click()`);
  await delay(100);
  await clickAndDrag([20,20],[60,60]);
  await until(`Module._sdk_object_count()===${beforeInk + 1}`,'ink after text');
  const beforeShape=await evaluate('Module._sdk_object_count()');
  await evaluate(`document.getElementById('external-shape').click()`); await delay(100);
  await clickAndDrag([210,130],[310,210]);
  await until(`Module._sdk_object_count()===${beforeShape + 1}`,'rectangle creation');
  if (!await evaluate(`!!Module._sdk_hover`)) throw new Error('connector hover is missing');
  await evaluate(`document.getElementById('external-select').click()`); await delay(100);
  if (!await evaluate(`document.getElementById('external-select').getAttribute('aria-pressed')==='true' && document.getElementById('external-shape').getAttribute('aria-pressed')==='false'`))
    throw new Error('Select tool indication is incorrect');
  const shapeCenter=await point(260,170);
  await cdp.send('Input.dispatchMouseEvent',{type:'mousePressed',...shapeCenter,button:'left',buttons:1,clickCount:1});
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseReleased',...shapeCenter,button:'left',buttons:0,clickCount:1});
  await until(`Module._sdk_selection_count()===1`,'select rectangle');
  await delay(300);
  const attachment=await evaluate(`(()=>{for(let y=50;y<300;y+=8)for(let x=150;x<520;x+=8){
    Module._sdk_hover(0,0);if(Module._sdk_hover(x,y)){Module._sdk_hover(0,0);return {x,y};}}return null})()`);
  if (!attachment) throw new Error('selected rectangle attachment point was not observable: '+await evaluate('Module.UTF8ToString(Module._sdk_selection_diagnostics())'));
  const hover=await point(attachment.x,attachment.y);
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseMoved',...hover,buttons:0}); await delay(100);
  if (await evaluate(`Module._sdk_hover(${attachment.x},${attachment.y})`) !== 0 ||
      await evaluate('Module._sdk_hover(0,0)') !== 1) throw new Error('DOM hover was not forwarded');
  await evaluate(`document.getElementById('external-undo').click()`);
  await until(`Module._sdk_object_count()===${beforeShape}`,'undo');
  await evaluate(`document.getElementById('external-redo').click()`);
  await until(`Module._sdk_object_count()===${beforeShape + 1}`,'redo');
  await evaluate(`document.getElementById('external-sticky').click()`); await delay(100);
  await clickAndDrag([400,130],[500,230]);
  await until(`Module._sdk_object_count()===${beforeShape + 2}`,'sticky creation');
  if (!await evaluate(`!!Module._sdk_selection_count && !!document.getElementById('external-rectangle-select')`))
    throw new Error('external multi-selection entry is missing');
  await evaluate(`document.getElementById('external-rectangle-select').click()`); await delay(100);
  await clickAndDrag([190,110],[520,250]);
  await until(`Module._sdk_selection_count()>=2`,'rectangle multi-selection');
  await evaluate(`document.getElementById('external-group').click()`);
  await until(`Module._sdk_object_count()===${beforeShape + 3}`,'group creation');
  await evaluate(`document.getElementById('external-ungroup').click()`);
  await until(`Module._sdk_object_count()===${beforeShape + 2}`,'ungroup');
  if (!await evaluate(`!!document.getElementById('external-image-file')`)) throw new Error('image import is missing');
  await cdp.send('Runtime.evaluate',{expression:`(async()=>{const c=document.createElement('canvas'); c.width=40;c.height=30;
    const g=c.getContext('2d');g.fillStyle='#e55';g.fillRect(0,0,40,30);
    const blob=await new Promise(resolve=>c.toBlob(resolve,'image/png'));const data=new DataTransfer();
    data.items.add(new File([blob],'fixture.png',{type:'image/png'}));const input=document.getElementById('external-image-file');
    input.files=data.files;input.dispatchEvent(new Event('change',{bubbles:true}));})()`,awaitPromise:true,returnByValue:true});
  await until(`Module._sdk_object_count()===${beforeShape + 3}`,'image import');
  if (!await evaluate(`!!Module._sdk_image_selected && !!document.getElementById('external-crop')`)) throw new Error('image crop entry is missing');
  await until(`Module._sdk_image_selected()===1`,'import selects image');
  await evaluate(`document.getElementById('external-crop').click()`);
  await until(`Module._sdk_crop_active()===1`,'crop begin');
  if(!await evaluate(`!!document.getElementById('external-crop-zoom') && !!Module._sdk_crop_scale`)) throw new Error('crop zoom is missing');
  await evaluate(`(()=>{const e=document.getElementById('external-crop-zoom');e.value='2';e.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  await until(`Math.abs(Module._sdk_crop_scale()-2)<0.01`,'crop zoom');
  await evaluate(`document.getElementById('external-crop-cancel').click()`);
  await until(`Module._sdk_crop_active()===0`,'crop cancel');
  if (!await evaluate(`!!document.getElementById('external-section')`)) throw new Error('Section control is missing');
  await evaluate(`document.getElementById('external-section').click()`);
  await until(`Module._sdk_object_count()===${beforeShape + 4}`,'Section creation');
  if (!await evaluate(`!!Module._sdk_camera_scale`)) throw new Error('viewport navigation is missing');
  const oldScale=await evaluate('Module._sdk_camera_scale()');
  const wheelPoint=await point(300,200);
  await cdp.send('Input.dispatchMouseEvent',{type:'mouseWheel',...wheelPoint,deltaX:0,deltaY:-60,modifiers:2});
  await until(`Module._sdk_camera_scale()!==${oldScale}`,'wheel zoom');
  if(events.some(e=>e.method==='Runtime.exceptionThrown'))throw new Error('browser exception');
  // A misconfigured generic server must not claim the input handlers are ready.
  badServer=http.createServer(async(req,res)=>{try{
    const name=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname);
    const target=path.resolve(build,'.'+(name==='/'?'/index.html':name));
    if(!target.startsWith(build+path.sep))throw new Error('outside consumer');
    const bytes=await fs.readFile(target);
    res.setHeader('Content-Type',target.endsWith('.mjs')?'text/plain':target.endsWith('.wasm')?'application/wasm':
      target.endsWith('.js')?'text/javascript':target.endsWith('.data')?'application/octet-stream':'text/html');res.end(bytes);
  }catch{res.statusCode=404;res.end();}});
  await new Promise(resolve=>badServer.listen(0,'127.0.0.1',resolve));
  await cdp.send('Page.navigate',{url:`http://127.0.0.1:${badServer.address().port}/index.html?badmime=1`});
  let badStatus='';
  for(let i=0;i<200;i++){
    if(!await evaluate(`location.search.includes('badmime=1')`)){await delay(50);continue;}
    badStatus=await evaluate(`document.getElementById('result')?.textContent||''`);
    if(badStatus.startsWith('FAIL: Web input setup failed'))break;
    if(badStatus.startsWith('READY'))throw new Error('bad MIME incorrectly reported READY');
    await delay(50);
  }
  if(!badStatus.startsWith('FAIL: Web input setup failed'))throw new Error('bad MIME failure was not reported: '+badStatus);
  console.log(JSON.stringify({status:'PASS_SCOPED',consumer:build,chrome,args,coverage:[
    'correct MIME server and visible failure on incorrect MIME','real pointer button selection, blue active state and rendered pixels',
    'responsive canvas and DPR resize','synthetic Chrome IME composition/result and text size',
    'ink, rectangle and sticky','rectangle selection and group/ungroup','undo/redo',
    'synthetic image file import, crop zoom and cancel','Section creation','wheel zoom']}));
  try{await cdp.send('Browser.close');}catch{}
} catch(e){failed=String(e.stack??e);console.error(failed);}
finally {
  socket?.close();browser?.kill();server?.kill();if(badServer)await new Promise(resolve=>badServer.close(resolve));
  const temp=await fs.realpath(os.tmpdir()), resolved=await fs.realpath(profile);
  if(!resolved.startsWith(temp+path.sep)||!path.basename(resolved).startsWith('axiom-external-chrome-'))
    throw new Error('refusing to remove unexpected browser profile');
  await fs.rm(resolved,{recursive:true,force:true,maxRetries:5,retryDelay:100});
}
process.exitCode=failed?1:0;
