// Private Skia/Ganesh parity probe; this is not a consumer SDK export.
// node verify_material_gpu.mjs <built-sdk-dir> <observation.json>
import {chromium} from '../verification/node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const observation={input:'private C++ production renderer probe on actual Chrome WebGL/Ganesh',deadlineMs:240000};
const html='<script>window.result={done:false,logs:[],errors:[]};</script><script src="g5_coverage_probe.js"></script><script>createCacheProbe({arguments:["--masked-canonical-only"],print:s=>result.logs.push(s),printErr:s=>result.errors.push(s),onExit:code=>{result.code=code;result.done=true;},onAbort:s=>{result.errors.push(String(s));result.code=1;result.done=true;}}).catch(e=>{result.errors.push(String(e));result.code=1;result.done=true;});</script>';
const server=http.createServer(async(req,res)=>{try{
 const u=new URL(req.url,'http://localhost').pathname,file=path.resolve(root,'.'+u);
 if(u!=='/'&&!file.startsWith(root+path.sep))throw Error('path');
 res.setHeader('Content-Type',u==='/'?'text/html':u.endsWith('.wasm')?'application/wasm':'text/javascript');
 res.end(u==='/'?html:await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
let browser;
try{
 for(const name of ['g5_coverage_probe.js','g5_coverage_probe.wasm']){
   observation[name]=hash(await fs.readFile(path.join(root,name)));
 }
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 observation.browser=await browser.version();
 const page=await browser.newPage();await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>result.done,null,{timeout:observation.deadlineMs});
 observation.result=await page.evaluate(()=>window.result);
 observation.status=observation.result.code===0?'PASS':'FAIL';
}catch(error){observation.status='ERROR';observation.error=String(error.stack||error);}
finally{
 if(browser)await browser.close();if(server.listening)await new Promise(r=>server.close(r));
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(observation,null,2)+'\n');
 console.log(JSON.stringify(observation));if(observation.status!=='PASS')process.exitCode=1;
}
