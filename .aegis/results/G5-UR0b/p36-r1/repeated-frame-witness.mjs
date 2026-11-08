// Expected RED against the exact P36-r1 decoder. No browser or mutation.
import fs from 'node:fs';import path from 'node:path';import {pathToFileURL} from 'node:url';
const [repo,rawPath]=process.argv.slice(2);if(!repo||!rawPath)throw Error('repo and preserved raw path required');
const {decodeTrace}=await import(pathToFileURL(path.join(repo,'verification/runners/g5/ur0/decode_trace.mjs')));
const rows=fs.readFileSync(rawPath,'utf8').trim().split('\n').map(JSON.parse),decoded=decodeTrace(rows,{critical:0,statistical:0});
const accepted=rows.filter(r=>r.kind===12&&r.tag===29&&r.values[0]==='1');
const result={status:accepted.length&&decoded.qualified_completions.length!==accepted.length?'REPRODUCED_FALSE_NEGATIVE':'NOT_REPRODUCED',accepted_raw_completions:accepted.map(r=>({seq:r.seq,values:r.values})),decoded_qualified_completions:decoded.qualified_completions.length,missing:decoded.missing,errors:decoded.errors};
console.log(JSON.stringify(result,null,2));process.exitCode=result.status==='REPRODUCED_FALSE_NEGATIVE'?1:0;
