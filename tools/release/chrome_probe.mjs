// Qualification environment only; this never substitutes a browser or a SUT.
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {spawn} from 'node:child_process';
import {pathToFileURL} from 'node:url';
const delay=ms=>new Promise(r=>setTimeout(r,ms));
export async function launchChrome(executable,profile) {
  executable=path.resolve(executable);profile=path.resolve(profile);
  await fs.access(executable);await fs.mkdir(profile,{recursive:true});
  const args=['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader','--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  const start=Date.now(),browser=spawn(executable,args,{windowsHide:true,stdio:['ignore','ignore','pipe']});
  let stderr='',spawnError;
  browser.stderr.on('data',data=>{stderr=(stderr+data.toString()).slice(-32768);});
  browser.on('error',error=>{spawnError=String(error);});
  const diagnostics=()=>({executable,args,profile,elapsedMs:Date.now()-start,exitCode:browser.exitCode,signal:browser.signalCode,spawnError,stderr});
  try {
    // This is an environment startup deadline, separate from all SUT deadlines.
    while(Date.now()-start<45000) {
      if(spawnError||browser.exitCode!==null||browser.signalCode!==null)throw Error('Chrome exited before its debugging endpoint');
      try{const port=(await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0].trim();if(/^\d+$/.test(port))return {browser,port,diagnostics};}catch{}
      await delay(50);
    }
    throw Error('Chrome debugging endpoint startup timeout');
  }catch(error){
    const facts=diagnostics();browser.kill();throw Error(`${error.message}: ${JSON.stringify(facts)}`);
  }
}
async function probe(executable,output) {
  const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-chrome-probe-'));let browser,result;
  try {
    const started=await launchChrome(executable,path.join(root,'profile'));browser=started.browser;
    const response=await fetch(`http://127.0.0.1:${started.port}/json/version`,{signal:AbortSignal.timeout(10000)});
    if(!response.ok)throw Error(`Chrome version endpoint: ${response.status}`);
    result={status:'ENVIRONMENT_READY',browser:await response.json(),startup:started.diagnostics()};
  }catch(error){result={status:'ENVIRONMENT_BLOCKED',error:String(error)};process.exitCode=1;}
  finally {
    if(browser){const exited=new Promise(resolve=>browser.exitCode!==null?resolve():browser.once('exit',resolve));browser.kill();await Promise.race([exited,delay(5000)]);}
    console.log(JSON.stringify(result));if(output){await fs.mkdir(path.dirname(path.resolve(output)),{recursive:true});await fs.writeFile(output,JSON.stringify(result,null,2)+'\n');}
    const temp=await fs.realpath(os.tmpdir()),actual=await fs.realpath(root);if(!actual.startsWith(temp+path.sep)||!path.basename(actual).startsWith('axiom-chrome-probe-'))throw Error('unsafe temporary path');
    await fs.rm(root,{recursive:true,force:true,maxRetries:6,retryDelay:100});
  }
}
if(process.argv[1]&&import.meta.url===pathToFileURL(path.resolve(process.argv[1])).href)await probe(process.argv[2],process.argv[3]);
