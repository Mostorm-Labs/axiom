import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';import {createHash} from 'node:crypto';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const html=await fs.readFile(new URL('./surface_continuity.html',import.meta.url));
const server=http.createServer(async(req,res)=>{try{
  const pathname=new URL(req.url,'http://localhost').pathname,file=path.resolve(sdk,'.'+pathname);
  if(pathname!=='/'&&!file.startsWith(sdk+path.sep))throw Error('path');
  res.setHeader('Content-Type',pathname==='/'?'text/html':file.endsWith('.wasm')?'application/wasm':file.endsWith('.mjs')?'text/javascript':'application/octet-stream');
  res.end(pathname==='/'?html:await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:process.env.CHROME_EXECUTABLE||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{
  const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2}),exceptions=[];
  page.on('pageerror',e=>exceptions.push(String(e)));await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>document.getElementById('result').textContent!=='PENDING',null,{timeout:60000});
  const result=JSON.parse(await page.locator('#result').textContent()),sha256={};
  for(const name of ['axiom.mjs','internal/axiom.mjs','axiom.wasm','axiom.data'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({result,exceptions,sha256,scope:'Chrome WebGL2 DPR2; real GPU fences delayed; synthetic DOM input'},null,2));
  console.log(JSON.stringify({result,exceptions}));if(result.status!=='PASS'||exceptions.length)process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
