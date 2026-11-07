// Private Skia/Ganesh parity probe; this is not a consumer SDK export.
// node verify_material_gpu.mjs <built-sdk-dir> <observation.json>
import {gpuProbeStatus} from '../verification/runners/g5/gpu_probe_status.mjs';
import {chromium} from '../verification/node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const observation={pageErrors:[],input:'private C++ production renderer probe on actual Chrome WebGL/Ganesh',deadlineMs:240000};
const html='<script>window.result={done:false,logs:[],errors:[]};</script><script src="g5_coverage_probe.js"></script><script>createCacheProbe({arguments:["--affine-only"],print:s=>result.logs.push(s),printErr:s=>{if(s.startsWith("AXIOM_NAVIGATION_TIMING ")||s.startsWith("AXIOM_FRAME_TIMING "))(result.diagnostics??=[]).push(s);else result.errors.push(s)},onExit:code=>{result.processExit=code;result.done=true;},onAbort:s=>{result.errors.push(String(s));result.code=1;result.done=true;}}).catch(e=>{result.errors.push(String(e));result.code=1;result.done=true;});</script>';
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
 const page=await browser.newPage();await page.addInitScript(()=>{const get=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(...a){const c=get.apply(this,a);if(c&&a[0]==='webgl2'&&!window.glObservation){const e=c.getExtension('WEBGL_debug_renderer_info');window.glObservation={version:c.getParameter(c.VERSION),renderer:e?c.getParameter(e.UNMASKED_RENDERER_WEBGL):c.getParameter(c.RENDERER)};}return c;};});page.on('pageerror',e=>observation.pageErrors.push(String(e)));await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.waitForFunction(()=>result.done,null,{timeout:observation.deadlineMs});
 observation.result=await page.evaluate(()=>window.result);
 observation.gl=await page.evaluate(()=>window.glObservation??null);
 observation.status=gpuProbeStatus(observation.result,observation.pageErrors);
}catch(error){observation.status='ERROR';observation.error=String(error.stack||error);}
finally{
 if(browser)await browser.close();if(server.listening)await new Promise(r=>server.close(r));
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(observation,null,2)+'\n');
 console.log(JSON.stringify({...observation,result:{...observation.result,failedPixels:observation.result?.failedPixels?"retained in observation file":undefined}}));if(observation.status!=='PASS')process.exitCode=1;
}
