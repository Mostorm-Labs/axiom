import path from 'node:path';import {fileURLToPath} from 'node:url';
import {runWorker} from './prepared_body_index.mjs';
export const REVERIFY_CASES=['S0-erase-navigation','S2-erase-navigation'];
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const [sdkArg,outArg,variant,oracle,referenceCatalog]=process.argv.slice(2);
  if(!referenceCatalog)throw Error('explicit frozen reference catalog required');
  await runWorker({sdkArg,outArg,variant,oracle,referenceCatalog,cases:REVERIFY_CASES,
    functionalPacing:true,identityRunner:'prepared_body_index_reverify_runner.py'});
}
