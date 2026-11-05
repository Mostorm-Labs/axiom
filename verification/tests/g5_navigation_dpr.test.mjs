import test from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
for(const dpr of ['0','5','not-a-number'])test(`navigation corroboration rejects unsupported DPR ${dpr}`,()=>{
 const r=spawnSync(process.execPath,['verification/runners/g5/navigation_lifecycle.mjs','unused','unused'],
  {env:{...process.env,AXIOM_NAV_DPR:dpr},encoding:'utf8'});
 assert.notEqual(r.status,0);assert.match(r.stderr,/invalid DPR/);
});
