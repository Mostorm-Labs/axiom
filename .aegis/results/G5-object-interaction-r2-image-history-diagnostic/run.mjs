import {chromium} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const root='/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/.aegis/results/G5-object-interaction-r2-image-history-diagnostic';
const result={id:'OI-R2-IMAGE-HISTORY',classification:'DIAGNOSTIC_ONLY',start:new Date().toISOString(),events:[],errors:[]};let browser,server,page;
try {
server=http.createServer(async(req,res)=>{try{const rel=decodeURIComponent(new URL(req.url,'http://local').pathname.slice(1));const f=path.resolve(root,rel);if(!f.startsWith(root+'/'))throw Error('path');res.setHeader('Content-Type',f.endsWith('.mjs')?'text/javascript':f.endsWith('.wasm')?'application/wasm':f.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(f));}catch(e){res.statusCode=404;res.end(String(e));}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});page.on('console',m=>{const msg=m.text();result.events.push({at:Date.now(),level:m.type(),msg});if(msg.startsWith('CASE_'))console.log(msg);});page.on('pageerror',e=>result.errors.push(String(e)));
await page.goto('http://127.0.0.1:'+server.address().port+'/diag.html');await page.waitForFunction(()=>typeof window.run==='function');result.rows=await page.evaluate(()=>window.run());result.status='RECORDED';
}catch(e){result.status='HARNESS_OR_SUBJECT_FAILURE';result.error=String(e.stack??e);if(page&&!page.isClosed())try{result.partial=await page.evaluate(()=>window.diagOut);}catch{}}
finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));result.end=new Date().toISOString();await fs.writeFile(root+'/result.json',JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}
