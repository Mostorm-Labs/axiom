import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {recoveryWorker} from './resource_recovery.mjs';

export function recoveryArguments({sdkArg,outArg,variant,oracle,referenceCatalog}){
  if(!['qualification','normal'].includes(variant))throw Error('explicit Recovery qualification/normal role required');
  return {sdkArg,outArg,variant,oracle,referenceCatalog,identityRunner:'resource_recovery_loss_reverify_runner.py'};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [sdkArg,outArg,variant,oracle,referenceCatalog]=process.argv.slice(2);
  await recoveryWorker(recoveryArguments({sdkArg,outArg,variant,oracle,referenceCatalog}));
}
