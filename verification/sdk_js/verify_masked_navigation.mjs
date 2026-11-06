// Actual root SDK, mixed image/shape/erased ink, wheel and two-touch navigation.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-mask-navigation-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id="board"></div><script type="module">import{createRuntime}from'./axiom.mjs';window.errors=[];window.sdk=await createRuntime({mount:document.getElementById('board'),documentId:'5f900000000000000000000000000001',permission:'localSingleUser',builtinFonts:false,onError:e=>errors.push(String(e))});</script>`;
let browser,server;const result={checks:{},samples:[],errors:[]};
try{
 await fs.cp(sdk,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);await fs.mkdir(path.dirname(output),{recursive:true});
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname.replace(/\/$/,'/index.html'));if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2,hasTouch:true});page.on('pageerror',e=>result.errors.push(String(e)));result.frameRejections=[];page.on('console',m=>{if(m.text().includes('Axiom Web frame rejected:'))result.frameRejections.push({stage:m.text(),afterPhase:result.samples.at(-1)?.phase??'startup'});});
 await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.sdk);
 const settle=async()=>{await page.waitForFunction(()=>{const s=sdk.readState();return !s.activeContacts&&s.lastCommit.presentedGeneration===s.generation;});await page.waitForTimeout(80);};
 const drag=async(x,y,dx,dy)=>{await page.mouse.move(x,y);await page.mouse.down();await page.mouse.move(x+dx,y+dy,{steps:24});await page.mouse.up();await settle();};
 await page.evaluate(async()=>{const c=document.createElement('canvas');c.width=350;c.height=300;const g=c.getContext('2d');g.fillStyle='#00b040';g.fillRect(0,0,350,300);const b=await new Promise(r=>c.toBlob(r,'image/png'));await sdk.importImage(new Uint8Array(await b.arrayBuffer()),'image/png');await sdk.transformSelection([1,0,0,1,100,100]);await sdk.selectShape('rectangle');await sdk.setAppearance({selected:false,fill:[0,.3,.9,1],fillEnabled:true});});
 await drag(470,100,330,300);
 await page.evaluate(async()=>{await sdk.selectBrush('round-handdrawn',4);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:24,thinning:0,color:[0,0,0,1]});});await drag(150,240,600,0);
 await page.evaluate(()=>sdk.setEraser({mode:2,diameter:40}));await drag(250,240,400,0);await page.evaluate(()=>sdk.selectTool('select'));await page.mouse.click(220,150);await settle();
 const before=await page.evaluate(()=>sdk.readState());result.before=before;
 const sample=async phase=>{await page.waitForTimeout(20);const row=await page.evaluate(()=>{const s=sdk.readState(),c=sdk.canvas,g=c.getContext('webgl2'),fb=g.getParameter(g.FRAMEBUFFER_BINDING),points=[];g.bindFramebuffer(g.FRAMEBUFFER,null);for(const [wx,wy]of [[280,240],[580,240]]){const x=Math.round((wx*s.camera.scale+s.camera.x)*2),y=c.height-1-Math.round((wy*s.camera.scale+s.camera.y)*2);if(x<0||y<0||x>=c.width||y>=c.height){points.push(null);continue;}const p=new Uint8Array(4);g.readPixels(x,y,1,1,g.RGBA,g.UNSIGNED_BYTE,p);points.push([...p]);}g.bindFramebuffer(g.FRAMEBUFFER,fb);return {points,camera:s.camera,generation:s.generation};});result.samples.push({phase,...row});};
 await sample('before');const initial=result.samples[0].points;
 result.checks.mixedFixtureVisible=initial[0]?.[1]>120&&initial[0]?.[0]<30&&initial[1]?.[2]>180&&initial[1]?.[0]<30;
 await page.mouse.move(450,450);for(let i=0;i<20;i++){await page.mouse.wheel(i%2?4:-3,2);await sample('wheel-pan');}
 await page.keyboard.down('Control');for(let i=0;i<18;i++){await page.mouse.wheel(0,i<9?-4:4);await sample('wheel-pinch');}await page.keyboard.up('Control');
 const cdp=await page.context().newCDPSession(page);const point=(id,x,y)=>({id,x,y,radiusX:4,radiusY:4,force:1});
 await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[point(1,350,500),point(2,550,500)]});
 for(let i=1;i<=8;i++){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point(1,350-i*4,500+i),point(2,550+i*5,500+i)]});await sample('two-finger-pinch');}
 for(let i=1;i<=8;i++){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point(1,318+i*4,508+i),point(2,590+i*4,508+i)]});await sample('two-finger-pan');}
 await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await settle();await sample('after');
 const colored=result.samples.flatMap(r=>r.points).filter(p=>p&&p[3]>250);result.checks.noWhiteMaskDuringNavigation=colored.length>60&&colored.every(p=>p[0]<100);
 const after=await page.evaluate(()=>sdk.readState());result.after=after;result.checks.documentUnchanged=before.generation===after.generation&&before.objects===after.objects&&before.lastCommit.operationId===after.lastCommit.operationId;
 result.checks.selectionPreserved=JSON.stringify(before.selected)===JSON.stringify(after.selected);result.checks.cameraMoved=JSON.stringify(before.camera)!==JSON.stringify(after.camera);
 result.errors.push(...await page.evaluate(()=>errors));await page.screenshot({path:output.replace(/\.json$/,'-scene.png')});await page.evaluate(()=>sdk.close());
 result.status=result.errors.length===0&&Object.values(result.checks).every(Boolean)?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify({status:result.status,checks:result.checks,error:result.error,errors:result.errors}));}
