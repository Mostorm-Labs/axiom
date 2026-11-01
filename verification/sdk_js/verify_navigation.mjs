import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
import {createHash} from 'node:crypto';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const html=await fs.readFile(new URL('./navigation_feedback.html',import.meta.url));
const server=http.createServer(async(req,res)=>{try{
  const pathname=new URL(req.url,'http://localhost').pathname;
  const file=path.resolve(sdk,'.'+pathname);if(pathname!=='/'&&!file.startsWith(sdk+path.sep))throw Error('path');
  res.setHeader('Content-Type',pathname==='/'?'text/html':file.endsWith('.wasm')?'application/wasm':file.endsWith('.mjs')?'text/javascript':'application/octet-stream');
  res.end(pathname==='/'?html:await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{
  const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
  const exceptions=[];page.on('pageerror',e=>exceptions.push(String(e)));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>document.getElementById('result').textContent!=='PENDING',null,{timeout:60000});
  const result=JSON.parse(await page.locator('#result').textContent());
  const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
  await fs.writeFile(output,JSON.stringify({scope:'Chrome WebGL2 DPR2, synthetic ctrl-wheel and deliberately delayed completion polls; not physical input-to-photon',result,exceptions,sha256},null,2));
  console.log(JSON.stringify({status:result.status,error:result.error,results:result.results,fences:result.fences,polls:result.polls,updated:result.samples.filter(x=>x.draws).length}));
  if(result.status!=='PASS'||exceptions.length)process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
