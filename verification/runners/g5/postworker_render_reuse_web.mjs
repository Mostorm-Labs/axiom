// New frozen package wrapper. Primary consumer/input remains byte-for-byte the
// accepted CPUW7 driver; this module introduces no additional browser window.
import fs from 'node:fs/promises';
import {run as primaryRun} from './cpu_postworker_web.mjs';
export {consumer,installPostworkerObserver} from './cpu_postworker_web.mjs';
export async function run(request,out){return primaryRun(request,out);}
if(process.argv[1]===new URL(import.meta.url).pathname){const result=await run(JSON.parse(await fs.readFile(process.argv[2],'utf8')),process.argv[3]);console.log(JSON.stringify(result));if(result.status!=='OBSERVED')process.exitCode=1;}
