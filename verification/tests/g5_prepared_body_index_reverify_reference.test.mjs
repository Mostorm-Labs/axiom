import test from 'node:test';import assert from 'node:assert/strict';
import fs from 'node:fs/promises';import os from 'node:os';import path from 'node:path';import {createHash} from 'node:crypto';
import {createReferenceQualifier} from '../runners/g5/prepared_body_index_reverify_reference.mjs';
const hash=b=>createHash('sha256').update(b).digest('hex');
async function fixture(fn){const dir=await fs.mkdtemp(path.join(os.tmpdir(),'g5-reference-'));
  const put=async(name,b)=>{const p=path.join(dir,name);await fs.writeFile(p,b);return{path:p,sha256:hash(b)};};
  try{const oracle=await put('oracle',Buffer.from('immutable oracle')),resource=await put('resource',Buffer.from('resource'));
    const snapshot=await put('old.axsnap',Buffer.from([1,2,3])),fresh=await put('fresh.axsnap',Buffer.from([1,2,3]));
    const rgba=await put('reference.rgba',Buffer.from([10,20,30,255])),edge=await put('reference.edge',Buffer.from([0]));
    const request={width:1,height:1,logical_width:1,logical_height:1,camera:{scale:1,x:0,y:0},hud_bounds:[]};
    const catalog={oracle,resources:[resource],entries:[{snapshot,request,rgba,edge,reference:{status:'REFERENCE_RENDERED'}}]};
    await fn({dir,put,oracle,resource,snapshot,fresh,rgba,edge,request,catalog});
  }finally{await fs.rm(dir,{recursive:true,force:true});}}
test('exact-input saved reference always compares the newly captured candidate',()=>fixture(async f=>{
  const qualify=await createReferenceQualifier(f.catalog,f.oracle.path,()=>{throw Error('unexpected oracle');});
  const args={oracle:f.oracle.path,snapshot:f.fresh.path,request:f.request,actual:Buffer.from([10,20,30,255]),prefix:path.join(f.dir,'new')};
  const good=await qualify(args);assert.equal(good.status,'QUALIFIED');assert.equal(good.reference_origin,'SAVED_EXACT_INPUT');
  assert.deepEqual(await fs.readFile(args.prefix+'.rgba'),await fs.readFile(f.rgba.path));
  const bad=await qualify({...args,actual:Buffer.from([80,20,30,255])});assert.equal(bad.status,'FAILED');assert.equal(bad.interior_mismatched_pixels,1);
}));
test('different snapshot bytes or complete request requires one original oracle invocation',()=>fixture(async f=>{
  let calls=0;const qualify=await createReferenceQualifier(f.catalog,f.oracle.path,async()=>{++calls;throw Error('ETIMEDOUT');});
  await assert.rejects(qualify({snapshot:f.fresh.path,request:{...f.request,extra:1},actual:Buffer.alloc(4),prefix:path.join(f.dir,'new')}),/ETIMEDOUT/);assert.equal(calls,1);
  await fs.writeFile(f.fresh.path,Buffer.from([1,2,4]));
  await assert.rejects(qualify({snapshot:f.fresh.path,request:f.request,actual:Buffer.alloc(4),prefix:path.join(f.dir,'other')}),/ETIMEDOUT/);assert.equal(calls,2);
}));
test('changed oracle or resource identity cannot reuse a saved reference',()=>fixture(async f=>{
  await fs.writeFile(f.resource.path,'changed');await assert.rejects(createReferenceQualifier(f.catalog,f.oracle.path),/resource identity/);
  await fs.writeFile(f.resource.path,'resource');await fs.writeFile(f.oracle.path,'other');await assert.rejects(createReferenceQualifier(f.catalog,f.oracle.path),/oracle identity/);
}));
test('corrupted saved reference is an explicit failure and never hidden by fallback',()=>fixture(async f=>{
  let calls=0;const qualify=await createReferenceQualifier(f.catalog,f.oracle.path,async()=>++calls);
  await fs.writeFile(f.rgba.path,Buffer.alloc(4));
  await assert.rejects(qualify({snapshot:f.fresh.path,request:f.request,actual:Buffer.alloc(4),prefix:path.join(f.dir,'new')}),/saved reference identity/);assert.equal(calls,0);
}));
