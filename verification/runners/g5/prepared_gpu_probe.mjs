// Standalone private C++/Skia oracle; never copied into the consumer SDK.
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {createHash} from 'node:crypto';
import {gpuProbeStatus} from './gpu_probe_status.mjs';
const root=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const probeArgument=process.argv[4]??'--gpu-candidate';
if(!['--gpu-candidate','--frontier-only','--replay-only','--contour-only','--stream-only','--continuity-only','--affine-only','--background-only','--exact-replay-only','--quality-only','--oversized-only','--detail-only','--detail-share-only','--detail-replay-only'].includes(probeArgument))throw Error('invalid GPU probe mode');
const result={scope:'Real Chrome WebGL2 private renderer Direct parity, not a latency measurement',pageErrors:[],deadlineMs:240000,artifacts:{}};
for(const name of ['g5_render_cache_gpu.js','g5_render_cache_gpu.wasm'])
  result.artifacts[name]=createHash('sha256').update(await fs.readFile(path.join(root,name))).digest('hex');
const html='<script>window.result={nativeDone:false,logs:[],errors:[]}</script><script src="g5_render_cache_gpu.js"></script><script>createCacheProbe({arguments:["--gpu-candidate"],print:s=>result.logs.push(s),printErr:s=>result.errors.push(s),onExit:code=>{result.processExit=code},onAbort:e=>{result.errors.push(String(e));result.code=1;result.nativeDone=true}}).catch(e=>{result.errors.push(String(e));result.code=1;result.nativeDone=true})</script>';
const probeHtml=html.replace('["--gpu-candidate"]',JSON.stringify([probeArgument]));result.probeArgument=probeArgument;
if(probeArgument==='--stream-only')result.scope='WASM Release production drawing-stream oracle; no GPU raster claim';
const server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://localhost').pathname,f=path.resolve(root,'.'+u);if(u!=='/'&&!f.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',u==='/'?'text/html':f.endsWith('.wasm')?'application/wasm':'text/javascript');res.end(u==='/'?probeHtml:await fs.readFile(f));}catch{res.statusCode=404;res.end();}});
let browser;
try{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
  const page=await browser.newPage();page.on('pageerror',e=>result.pageErrors.push(String(e)));
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>window.result.nativeDone,null,{timeout:result.deadlineMs});
  result.observation=await page.evaluate(()=>window.result);
  result.status=gpuProbeStatus(result.observation,result.pageErrors);
}catch(error){result.status='BLOCKED_EVIDENCE';result.error=String(error.stack||error);}
finally{
  if(browser)await browser.close();if(server.listening)await new Promise(r=>server.close(r));
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(result,null,2));
  console.log(JSON.stringify({status:result.status,code:result.observation?.code,error:result.error}));
  if(result.status!=='PASS')process.exitCode=1;
}
