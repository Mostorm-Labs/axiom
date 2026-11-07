// Private Skia/Ganesh parity probe; this is not a consumer SDK export.
// node verify_material_gpu.mjs <built-sdk-dir> <observation.json>
import {gpuProbeStatus} from './gpu_probe_status.mjs';
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const argument=process.argv[4]??'--v4-gpu-only';
if(!['--v4-gpu-only','--masked-progress-only','--masked-mixed-only','--affine-erased-only','--regional-mask-only','--live-erase-gpu-miss-only','--body-mask-only','--erase-damage-only','--erase-damage-v3-only'].includes(argument))throw Error('invalid GPU probe mode');
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const observation={pageErrors:[],input:'private C++ production renderer probe on actual Chrome WebGL/Ganesh',deadlineMs:240000};
const htmlTemplate='<script>window.result={done:false,logs:[],errors:[]};</script><script src="g5_coverage_probe.js"></script><script>createCacheProbe({arguments:["--v4-gpu-only"],print:s=>result.logs.push(s),printErr:s=>result.errors.push(s),onExit:code=>{result.processExit=code;result.done=true;},onAbort:s=>{result.errors.push(String(s));result.code=1;result.done=true;}}).catch(e=>{result.errors.push(String(e));result.code=1;result.done=true;});</script>';
const html=htmlTemplate.replace('["--v4-gpu-only"]',JSON.stringify([argument]));observation.argument=argument;
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
 // Raw stderr stays in the artifact. Valid versioned timings are diagnostics;
 // malformed records, unknown versions and actual native errors fail closed.
 observation.timings=[];
 const errors=observation.result.errors.filter(line=>{
   if(!line.startsWith('AXIOM_NAVIGATION_TIMING '))return true;
   try{const row=JSON.parse(line.slice(24));if(row.version!==5||!Number.isFinite(row.elapsed_ns))return true;observation.timings.push(row);return false;}catch{return true;}
 });
 observation.status=gpuProbeStatus({...observation.result,errors},observation.pageErrors);
}catch(error){observation.status='ERROR';observation.error=String(error.stack||error);}
finally{
 if(browser)await browser.close();if(server.listening)await new Promise(r=>server.close(r));
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify(observation,null,2)+'\n');
 console.log(JSON.stringify({...observation,result:{...observation.result,failedPixels:observation.result?.failedPixels?"retained in observation file":undefined}}));if(observation.status!=='PASS')process.exitCode=1;
}
