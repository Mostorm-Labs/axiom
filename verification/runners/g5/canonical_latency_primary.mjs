// Same original primary workload, new bounded task namespace.
import fs from'node:fs/promises';import{run}from'./cpu_postworker_web.mjs';
export{consumer,installPostworkerObserver}from'./cpu_postworker_web.mjs';
if(process.argv[1]===new URL(import.meta.url).pathname){const r=await run(JSON.parse(await fs.readFile(process.argv[2],'utf8')),process.argv[3]);console.log(JSON.stringify(r));if(r.status!=='OBSERVED')process.exitCode=1;}
