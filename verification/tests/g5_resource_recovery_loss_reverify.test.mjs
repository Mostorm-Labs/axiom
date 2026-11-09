import test from 'node:test';
import assert from 'node:assert/strict';
import {recoveryArguments} from '../runners/g5/resource_recovery_loss_reverify.mjs';
import {recoveryWorker} from '../runners/g5/resource_recovery.mjs';

const input={sdkArg:'/new/sdk',outArg:'/new/output',variant:'qualification',oracle:'/locked/oracle',referenceCatalog:'/locked/catalog'};
test('loss reverification forwards complete existing Recovery inputs with new private source identity',()=>{
  assert.deepEqual(recoveryArguments(input),{...input,identityRunner:'resource_recovery_loss_reverify_runner.py'});
});
test('normal Recovery keeps the same path and cannot select core or another role',()=>{
  assert.equal(recoveryArguments({...input,variant:'normal'}).variant,'normal');
  assert.throws(()=>recoveryArguments({...input,variant:'core'}));
  assert.throws(()=>recoveryArguments({...input,variant:'seed'}));
});
test('caller cannot replace the exact identity runner and no browser starts on import',()=>{
  assert.equal(recoveryArguments({...input,identityRunner:'resource_recovery_runner.py'}).identityRunner,'resource_recovery_loss_reverify_runner.py');
  assert.equal(typeof recoveryWorker,'function');
});
