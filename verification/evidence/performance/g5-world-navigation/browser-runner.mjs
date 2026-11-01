import {chromium} from '../../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2] ?? 'out/g5-world-reproduce'),sdk=path.resolve(process.argv[3] ?? 'out/g5-render-cache/web/build/sdk/web');
await fs.mkdir(root,{recursive:true});await fs.copyFile(new URL('./browser-final/browser.html',import.meta.url),path.join(root,'browser.html'));
const server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://localhost').pathname;const p=u.startsWith('/sdk/')?path.join(sdk,u.slice(5)):path.join(root,'browser.html');res.setHeader('Content-Type',p.endsWith('.wasm')?'application/wasm':p.endsWith('.mjs')?'text/javascript':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const summary=[];const median=a=>{a=[...a].sort((a,b)=>a-b);return a.length%2?a[(a.length-1)/2]:(a[a.length/2-1]+a[a.length/2])/2;};
try {
 for(const count of [64]){
  const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:1});const cdp=await page.context().newCDPSession(page);
  await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);await page.evaluate(()=>ready);
  for(let n=0;n<count;++n){for(let i=0;i<16;++i){await cdp.send('Input.dispatchMouseEvent',{type:i===0?'mousePressed':i===15?'mouseReleased':'mouseMoved',x:230+i*12,y:180+(n%8)*15+Math.sin(i*.4)*5,button:'left',buttons:i===15?0:1,clickCount:i===0||i===15?1:0});}await page.evaluate(()=>settle());}
  for(const mode of ['zoom','touch']) {
   await page.evaluate(()=>start());
   if(mode==='touch'){
    const points=(i)=>[{id:1,x:300-i,y:230-i/4,radiusX:2,radiusY:2,force:1},{id:2,x:500+i,y:270+i/4,radiusX:2,radiusY:2,force:1}];
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:points(0)});
    for(let i=1;i<=16;++i){await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:points(i)});await page.evaluate(()=>new Promise(requestAnimationFrame));}
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
   }else for(let i=0;i<16;++i){await cdp.send('Input.dispatchMouseEvent',{type:'mouseWheel',x:400,y:250,deltaX:mode==='pan'?1:0,deltaY:mode==='zoom'?-.6:0,modifiers:mode==='zoom'?2:0});await page.evaluate(()=>new Promise(requestAnimationFrame));}
   const raw=await page.evaluate(()=>stop());await fs.writeFile(path.join(root,`web-${count}-${mode}.json`),JSON.stringify(raw,null,2));
   const render=raw.raf.filter(x=>x.name==='render'),draw=render.filter(x=>x.duration>2);
   summary.push({objects:count,mode,renderer:raw.renderer,render_callbacks:render.length,expensive_callbacks:draw.length,render_p50_ms:median(render.map(x=>x.duration)),expensive_p50_ms:draw.length?median(draw.map(x=>x.duration)):null,wheel_p50_ms:median(raw.events.map(x=>x.duration)),present_callbacks:raw.frames.length,document_unchanged:raw.before.generation===raw.after.generation,events:raw.events.length,errors:raw.errors});
  }
  await page.close();
 }
 const bindings={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])bindings[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 await fs.writeFile(path.join(root,'web-summary.json'),JSON.stringify({scope:'Chrome CDP paced wheel, root SDK RAF callback CPU duration; not physical latency or GPU timer',sdk_sha256:bindings,runs:summary},null,2));console.log(JSON.stringify(summary,null,2));
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
