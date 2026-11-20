import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {chromium} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';

// One P36 correctness process. It collects no performance measurements and
// never attaches to the user's browser or any pre-existing document.
const [sdkRoot, output] = process.argv.slice(2);
const sha = b => createHash('sha256').update(b).digest('hex');
const report = {kind:'P36 correctness only', status:'STARTED', cases:[], errors:[], served:{}};
let server, browser;
await fs.mkdir(output,{recursive:true});
const save = () => fs.writeFile(path.join(output,'smoke.json'),JSON.stringify(report,null,2)+'\n');
try {
  server=http.createServer(async(req,res)=>{
    try {
      const name=new URL(req.url,'http://localhost').pathname;
      let bytes;
      if(name==='/') {
        bytes=Buffer.from('<style>html,body{margin:0}#mount{position:relative;width:1000px;height:650px}</style><div id="mount"></div>');
        res.setHeader('Content-Type','text/html');
      } else if(name==='/favicon.ico') { res.statusCode=204;res.end();return; }
      else {
        if(!name.startsWith('/sdk/')||name.includes('..'))throw Error('invalid path');
        bytes=await fs.readFile(path.join(sdkRoot,name.slice(5)));
        res.setHeader('Content-Type',name.endsWith('.mjs')?'text/javascript':name.endsWith('.wasm')?'application/wasm':'application/octet-stream');
      }
      report.served[name]={bytes:bytes.length,sha256:sha(bytes)};res.end(bytes);
    } catch(error) {report.errors.push(String(error));res.statusCode=500;res.end(String(error));}
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  report.browserVersion=browser.version();
  const cdp=await browser.newBrowserCDPSession();report.system=await cdp.send('SystemInfo.getInfo');
  for(const dpr of [2,4])for(const profile of ['crayon-wax-v1','chalk-compact-v1']) {
    const context=await browser.newContext({viewport:{width:1100,height:750},deviceScaleFactor:dpr});
    const page=await context.newPage();page.on('pageerror',e=>report.errors.push(String(e)));
    await page.goto('http://127.0.0.1:'+server.address().port+'/');
    const row=await page.evaluate(async({profile,dpr})=>{
      const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
      const check=(v,m)=>{if(!v)throw Error(m);};
      const {createRuntime}=await import('/sdk/axiom.mjs');
      const errors=[],operations=[];
      const sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'36000000000000000000000000000001',
        builtinFonts:false,permission:'localSingleUser',onError:e=>errors.push({code:e.code,message:e.message}),
        onOperation:b=>operations.push(b.length)});
      sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
      const event=(type,n)=>{
        const box=sdk.canvas.getBoundingClientRect();
        sdk.canvas.dispatchEvent(new PointerEvent('pointer'+type,{pointerId:1,pointerType:'mouse',
          clientX:box.left+80+n*4,clientY:box.top+260+70*Math.sin(n*.12),
          button:type==='move'?-1:0,buttons:['up','cancel'].includes(type)?0:1,
          pressure:.5,bubbles:true,cancelable:true}));
      };
      const stable=async()=>{
        for(let n=0;n<1000;++n){const s=sdk.readState();
          if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration)return s;
          await pause(16);
        }throw Error('settle did not complete');
      };
      await sdk.selectBrush(profile,3);await sdk.selectTool('ink');await stable();
      check(sdk.canvas.width===1000*dpr&&sdk.canvas.height===650*dpr,'initial DPR dimensions');
      event('down',0);for(let n=1;n<=90;++n){event('move',n);await pause(8);}event('up',90);
      const committed=await stable();check(committed.objects===1,'Up lost stroke');
      const opCount=operations.length;
      event('down',0);for(let n=1;n<=12;++n){event('move',n);await pause(10);}event('cancel',12);
      const cancelled=await stable();check(cancelled.objects===1&&operations.length===opCount,'Cancel mutated canonical');
      event('down',0);for(let n=1;n<=30;++n){event('move',n);await pause(10);}
      document.getElementById('mount').style.width='900px';
      // The SDK deliberately defers rebinding an active contact until its
      // terminal event. Preserve that policy and inspect the final binding.
      await pause(350);check(sdk.canvas.width===1000*dpr,'active contact changed binding');
      event('move',31);event('up',31);const resized=await stable();
      check(resized.objects===2&&sdk.canvas.width===900*dpr,'active resize dropped commit or final binding');
      await pause(150);const afterLate=sdk.readState();
      check(afterLate.objects===2&&!afterLate.activeContacts,'late completion changed settled state');
      // Existing root SDK context-loss handlers exercise lose->new context
      // pair without inspecting a private bridge or geometry.
      const beforeLossGeneration=afterLate.generation;
      const gl=sdk.canvas.getContext('webgl2'),lose=gl?.getExtension('WEBGL_lose_context');
      let contextRecovery='UNAVAILABLE';
      if(lose){
        lose.loseContext();await pause(150);lose.restoreContext();await pause(600);
        const recovered=await stable();check(recovered.objects===2&&recovered.generation===beforeLossGeneration,'context restoration changed canonical');
        contextRecovery='RESTORED';
      }
      event('down',0);for(let n=1;n<=8;++n){event('move',n);await pause(10);}
      // A cancellation can leave a late preview fence queued at close.
      event('cancel',8);await stable();
      const build=sdk.readBuildInfo();await sdk.close();
      check(!errors.length,'SDK error: '+JSON.stringify(errors));
      return {profile,dpr,build,committedObjects:committed.objects,cancelledObjects:cancelled.objects,
        resizedObjects:resized.objects,operations:operations.length,contextRecovery,errors,closed:true,normalRootSDK:true};
    },{profile,dpr});
    report.cases.push(row);await save();await page.close();await context.close();
  }
  if(report.errors.length)throw Error('browser errors');report.status='PASS_CORRECTNESS_ONLY';
} catch(error) {report.status='FAILED';report.error=String(error.stack??error);process.exitCode=1;}
finally {await browser?.close();if(server)await new Promise(resolve=>server.close(resolve));report.browserClosed=true;await save();}
