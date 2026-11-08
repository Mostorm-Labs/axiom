import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {runWithDiagnostics} from '../runners/g5/transient_reference.mjs';
import {CASES,validateSnapshotInput,validateRestoredCase,validateRestoreEnvelope,consumerModuleSource,attributionConsumer} from '../runners/g5/transient_attribution.mjs';
import {observeExistingGpuCalls} from '../runners/g5/transient_attribution_observer.mjs';

test('a nextDown product failure preserves actual predicate, prior phase and SDK error',async()=>{
  const phases=[{id:'plain-drag',status:'COMPLETED'},{id:'erase-0',afterUp:{activeContacts:0},afterNextDown:{activeContacts:0}}];
  const error=Object.assign(Error('PRODUCT_CHECK: immediate next Down rejected'),{predicate:{actual:0,expected:1}});
  const observed=await runWithDiagnostics(async()=>{throw error;},()=>({phases,errors:[{code:'INPUT_REJECTED',message:'INPUT_REJECTED'}],state:{activeContacts:0}}));
  assert.equal(observed.status,'FAILED');
  assert.equal(observed.failure.predicate.actual,0);
  assert.equal(observed.diagnostics.phases[0].status,'COMPLETED');
  assert.equal(observed.diagnostics.phases[1].afterNextDown.activeContacts,0);
  assert.equal(observed.diagnostics.errors[0].code,'INPUT_REJECTED');
});

test('successful action retains output and read-only diagnostic export',async()=>{
  const observed=await runWithDiagnostics(async()=>({final:'cold'}),()=>({phases:[],errors:[]}));
  assert.equal(observed.status,'COMPLETED');assert.equal(observed.value.final,'cold');
  assert.deepEqual(observed.diagnostics.errors,[]);
});

test('restored live semantic clock is independent of source and external checkpoint clocks',()=>{
  const source={metadata:{documentRevision:'16',resourceBinding:'1'},expected_config:{family:1},state:{documentId:'9'.repeat(32),generation:'16',camera:{x:0,y:0,scale:1}}};
  const init={binding:'1',config:{family:1},state:{documentId:'9'.repeat(32),generation:'0',objects:16,activeContacts:0,camera:{x:0,y:0,scale:1}}};
  assert.deepEqual(validateRestoredCase(init,source),{live_initial_semantic_generation:'0',source_capture_semantic_generation:'16',external_checkpoint_revision:'16'});
  assert.equal(validateRestoredCase({...init,state:{...init.state,generation:'7'}},source).live_initial_semantic_generation,'7');
  for(const patch of [{documentId:'8'.repeat(32)},{objects:15},{activeContacts:1},{camera:{x:1,y:0,scale:1}}])
    assert.throws(()=>validateRestoredCase({...init,state:{...init.state,...patch}},source));
  assert.throws(()=>validateRestoredCase({...init,config:{family:2}},source));
  assert.throws(()=>validateRestoredCase({...init,binding:'0'},source));
});

test('caller-issued envelope preserves capture0 and checkpoint and rejects foreign/zero input',()=>{
  const source={name:'S0',sha256:'a'.repeat(64),state:{documentId:'9'.repeat(32)},metadata:{documentRevision:'16',resourceBinding:'0',checkpointDigest:[1,2],recoveryFrontier:[8,1]}};
  const s={...source,source_capture_metadata:structuredClone(source.metadata),metadata:{...source.metadata,resourceBinding:'1'},
    restore_custody:{task:'G5-UR1a-attribution-r2',issuer:'verification-fixture-data-owner',binding:'1',documentId:source.state.documentId,snapshot_sha256:{S0:source.sha256}}};
  validateRestoreEnvelope(s);assert.equal(source.metadata.resourceBinding,'0');
  for(const b of ['0','','-1','18446744073709551616'])assert.throws(()=>validateRestoreEnvelope({...s,metadata:{...s.metadata,resourceBinding:b},restore_custody:{...s.restore_custody,binding:b}}));
  const max='18446744073709551615';validateRestoreEnvelope({...s,metadata:{...s.metadata,resourceBinding:max},restore_custody:{...s.restore_custody,binding:max}});
  for(const p of [{restore_custody:null},{restore_custody:{...s.restore_custody,issuer:'foreign'}},{sha256:'b'.repeat(64)},
    {metadata:{...s.metadata,checkpointDigest:[9]}},{state:{documentId:'8'.repeat(32)}}])assert.throws(()=>validateRestoreEnvelope({...s,...p}));
});

test('consumer module promise waits for deliberately delayed installation',async()=>{
  const source=consumerModuleSource();assert.match(source,/;await \(async function attributionConsumer/);
  let release;const delayed=()=>new Promise(r=>{release=r;});const installed={};
  const modeled=source.replace(attributionConsumer.toString(),'async function(){await delayed();window.ur1attr={ready:true};}');
  const ModuleAsync=Object.getPrototypeOf(async function(){}).constructor;
  let resolved=false;const pending=new ModuleAsync('window','delayed',modeled)(installed,delayed).then(()=>{resolved=true;});
  await Promise.resolve();assert.equal(resolved,false);assert.equal(installed.ur1attr,undefined);release();await pending;assert.equal(installed.ur1attr.ready,true);
});

test('only exact same snapshot and complete metadata may enter measured cases',()=>{
  const input={name:'S0',sha256:'a'.repeat(64),metadata:{documentRevision:'16',checkpointDigest:[1],resourceBinding:'1',recoveryFrontier:[8,1]}};
  validateSnapshotInput(structuredClone(input),input);
  for(const patch of [{name:'S2'},{sha256:'b'.repeat(64)},{metadata:{...input.metadata,resourceBinding:'2'}},{metadata:{...input.metadata,documentRevision:'15'}}])
    assert.throws(()=>validateSnapshotInput({...input,...patch},input));
  assert.deepEqual(CASES,['plain-drag','immediate-reerase','masked-drag']);
});

test('optional GPU wrappers only forward preexisting calls once and label polling interval',()=>{
  let now=0,issued=0;
  class GL {ALREADY_SIGNALED=1;CONDITION_SATISFIED=2;
    fenceSync(){issued++;return {};}
    clientWaitSync(){issued++;return 2;}
    deleteSync(){issued++;}finish(){issued++;}readPixels(){issued++;}}
  const env={WebGL2RenderingContext:GL,performance:{now:()=>now++}},observer=observeExistingGpuCalls(env),gl=new GL(),p={};
  observer.begin(p);assert.equal(issued,0);const f=gl.fenceSync();gl.clientWaitSync(f);gl.deleteSync(f);observer.end(p);
  assert.equal(issued,3);assert.equal(p.gpuCalls.length,3);
  assert.match(p.gpuCalls[1].interval_scope,/not pure GPU/);observer.restore();assert.equal(issued,3);
});

test('no per-Move process logging, no timed readback and cold work is separate',async()=>{
  const src=attributionConsumer.toString();
  const run=src.slice(src.indexOf('async run(id,head)'),src.indexOf('async cold(undoRedo)'));
  assert.doesNotMatch(run,/readPixels|captureSnapshot|cold\(/);
  assert.match(src,/resourceBinding:sdk\.resources\.list\(\)\.binding/);
  assert.match(src,/checkpointDigest:snapshot\.checkpointDigest/);
  assert.match(src,/next Down rejected/);assert.match(src,/gpu_coverage_path:'unavailable'/);
  const moves=src.slice(src.indexOf('const eraseMoves='),src.indexOf('const seed='));assert.doesNotMatch(moves,/console\.|emit\(/);
  const old=await fs.readFile(new URL('../runners/g5/transient_reference.mjs',import.meta.url),'utf8');
  assert.match(old,/immediate next Down rejected.*actual:p.afterNextDown.activeContacts/);
  assert.match(old,/partial.json/);
});
