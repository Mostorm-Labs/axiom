// Causal offline model, never a native recording or a browser qualification.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {createHash} from 'node:crypto';
const file='verification/evidence/performance/g5-unified-render-ur0b/a3d022abba7d5909aa6dc1e97f5b0f03af0d34154aae585a0a0db21d2f3162c4/resume-r6/source/verification/runners/g5/ur0/consumer.mjs';
const bytes=fs.readFileSync(file);assert.equal(createHash('sha256').update(bytes).digest('hex'),'4512faa07223db9a9a761eeccdc2210e8fef65becf65a9b3e46135553117aee7');
const source=bytes.toString();assert.ok(source.indexOf('sdk=await createRuntime')<source.indexOf('diag().control(2,1)'));
let recording=false,initialPending=true;const observed=[];
async function createRuntime(){/* initial draw/capture while observation OFF */return {generation:'0',pending_gpu:0};}
await createRuntime();observed.length=0;recording=true;
for(let turn=0;turn<17;++turn){if(turn===16){initialPending=false;if(recording)observed.push({model_kind:'accepted',work:'initial',has_observed_draw:false});}}
assert.equal(initialPending,false);assert.equal(observed.length,1);
console.log(JSON.stringify({schema:'OFFLINE_STARTUP_CAUSAL_MODEL','old_source_sha256':'4512faa07223db9a9a761eeccdc2210e8fef65becf65a9b3e46135553117aee7',accepted_after_enable:1,orphaned_completions:1,public_generation:'0',disabled_pending_gpu:0,new_browser_starts:0}));
assert.equal(observed.filter(e=>!e.has_observed_draw).length,0,'startup boundary must be closed; old createRuntime-return/reset/enable leaks a delayed completion');
