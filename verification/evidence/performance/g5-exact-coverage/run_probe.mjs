// Explicit private verification target, never installed in the consumer SDK.
import {chromium} from '../../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const root=path.resolve(process.argv[2]);
const args=process.argv.slice(3);
const html=`<script>window.result={done:false,logs:[],errors:[]};</script><script src="g5_coverage_probe.js"></script><script>createCacheProbe({arguments:${JSON.stringify(args)},print:s=>result.logs.push(s),printErr:s=>result.errors.push(s),onExit:code=>{result.code=code;result.done=true;},onAbort:s=>{result.errors.push(String(s));result.code=1;result.done=true;}}).catch(e=>{result.errors.push(String(e));result.code=1;result.done=true;});</script>`;
const server=http.createServer(async(req,res)=>{try{
 const u=new URL(req.url,'http://localhost').pathname,file=path.resolve(root,'.'+u);
 if(u!=='/'&&!file.startsWith(root+path.sep))throw Error('path');
 res.setHeader('Content-Type',u==='/'?'text/html':u.endsWith('.wasm')?'application/wasm':'text/javascript');
 res.end(u==='/'?html:await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{
 const page=await browser.newPage();await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>result.done,null,{timeout:120000});
 const result=await page.evaluate(()=>window.result);console.log(JSON.stringify(result));
 if(result.code!==0)process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
