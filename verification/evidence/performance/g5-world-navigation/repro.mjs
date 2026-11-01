import {chromium} from '../../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const root=path.resolve('out/g5-world-navigation'),sdk=path.resolve('out/g5-render-cache/web/build/sdk/web');
const html=await fs.readFile(new URL('./repro.html',import.meta.url),'utf8');
await fs.mkdir(root,{recursive:true});
const server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://local').pathname;if(u==='/'){res.setHeader('Content-Type','text/html');res.end(html);return;}const p=path.join(sdk,u.slice(5));res.setHeader('Content-Type',p.endsWith('.wasm')?'application/wasm':'text/javascript');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{const page=await browser.newPage({viewport:{width:800,height:500},deviceScaleFactor:1});const cdp=await page.context().newCDPSession(page);await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.evaluate(()=>ready);
 for(let n=0;n<16;++n){for(let i=0;i<16;++i){await cdp.send('Input.dispatchMouseEvent',{type:i===0?'mousePressed':i===15?'mouseReleased':'mouseMoved',x:20+i*50,y:20+n*29+Math.sin(i*.7)*8,button:'left',buttons:i===15?0:1,clickCount:i===0||i===15?1:0});}await page.waitForFunction(()=>{const s=runtime.readState();return BigInt(s.lastCommit.presentedGeneration)>=BigInt(s.generation);});}
 let old=await page.evaluate(()=>frameCount);await page.evaluate(()=>runtime.zoomAt(400,250,2));await page.waitForFunction(n=>frameCount>n,old);
 const before=await page.evaluate(()=>({frames:frameCount,state:runtime.readState()}));
 await page.evaluate(()=>{window.capture=true;return runtime.zoomAt(400,250,.5);});await page.waitForFunction(()=>window.navImage,null,{polling:10});
 const during=await page.evaluate(()=>({frames:navFrameCount,state:navState}));
 await fs.writeFile(path.join(root,'navigation.png'),Buffer.from((await page.evaluate(()=>navImage)).split(',')[1],'base64'));
 await page.evaluate(()=>release());await new Promise(r=>setTimeout(r,150));
 await fs.writeFile(path.join(root,'navigation-150ms.png'),Buffer.from((await page.evaluate(()=>runtime.canvas.toDataURL())).split(',')[1],'base64'));
 await page.waitForFunction(n=>frameCount>n,before.frames);
 await fs.writeFile(path.join(root,'exact.png'),Buffer.from((await page.evaluate(()=>runtime.canvas.toDataURL())).split(',')[1],'base64'));
 const after=await page.evaluate(()=>({frames:frameCount,state:runtime.readState(),errors}));
 await fs.writeFile(path.join(root,'repro.json'),JSON.stringify({before,during,after},null,2));console.log(JSON.stringify({before:before.frames,during:during.frames,after:after.frames,errors:after.errors}));
}finally{await browser.close();await new Promise(r=>server.close(r));}
