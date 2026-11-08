import fs from 'node:fs/promises';import path from 'node:path';
import {decodeTrace} from './decode_trace.mjs';import {externalMetrics} from './external_metrics.mjs';
import {compileMetrics} from './metrics_compiler.mjs';import {execFileSync} from 'node:child_process';
import {canonicalChainProof} from './canonical_chain.mjs';import {createHash} from 'node:crypto';
const root=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]??root),read=async n=>JSON.parse(await fs.readFile(path.join(root,n),'utf8'));
await fs.mkdir(output,{recursive:true});
const hash=bytes=>createHash('sha256').update(bytes).digest('hex'),fileHash=async p=>fs.readFile(p).then(hash).catch(()=>null);
const runBytes=await fs.readFile(path.join(root,'worker-result.json')),run=JSON.parse(runBytes);
const rawBytes=await fs.readFile(path.join(root,'raw-trace.jsonl')).catch(()=>null);
const inputBytes=await fs.readFile(path.join(root,'run-inputs.json')).catch(()=>null);
const recording={run,raw_trace_jsonl:rawBytes?.toString()??null,worker_result_json:runBytes.toString(),run_inputs_json:inputBytes?.toString()??null,
 input_refs:{raw_trace_sha256:rawBytes?hash(rawBytes):null,worker_result_sha256:hash(runBytes),run_inputs_sha256:inputBytes?hash(inputBytes):null}};
const raw=(rawBytes?.toString('utf8')??'').trim().split('\n').filter(Boolean).map(JSON.parse);
const decoded=decodeTrace(raw,run.trace?.loss??{critical:0,statistical:0},recording);
await fs.writeFile(path.join(output,'decoded.json'),JSON.stringify(decoded));
const external=run.measurement?externalMetrics(run.measurement):null;
if(external)await fs.writeFile(path.join(output,'external-metrics.json'),JSON.stringify(external,null,2)+'\n');
let schema_status='MISSING',schema_reason='no complete measurement/source/fixture identity',compiled=null;
if(run.measurement){
 try{
  const inputs=await read('run-inputs.json'),runId=path.basename(root);
  compiled=compileMetrics(run,raw,{run_id:runId,row_id:runId.replace(/-r\d+$/,''),repetition:Number(/-r(\d+)$/.exec(runId)?.[1]??1),fixture_sha256:inputs.fixture_sha256,recording});
  const schema=execFileSync('python3.12',[new URL('./validate_metrics.py',import.meta.url).pathname],{input:JSON.stringify(compiled.document),maxBuffer:64*1024*1024,encoding:'utf8'});
  schema_status=JSON.parse(schema).status;schema_reason=null;
  await fs.writeFile(path.join(output,'metrics.json'),JSON.stringify(compiled.document));
  await fs.writeFile(path.join(output,'compiler-provenance.json'),JSON.stringify(compiled.provenance));
 }catch(error){schema_status='MISSING';schema_reason=String(error);}
}
const value={status:run.status,source_stable:run.source_stable,artifacts_stable:run.artifacts_stable,common:external?.common??null,critical_loss:run.trace?.loss?.critical??null,statistical_loss:run.trace?.loss?.statistical??null,identity_errors:decoded.errors,missing:decoded.missing,clock:run.calibration?.status??'MISSING',terminal:run.measurement?.after??null,final_snapshot_sha256:run.final?.snapshot.sha256??null,product_errors:run.measurement?.errors??[],artifact_identity:run.artifacts_before};
value.schema_status=schema_status;value.schema_reason=schema_reason;
value.actual_observations={raw_records:raw.length,owners:decoded.owner_bindings?.length??0,leaves:decoded.leaves.length,commits:decoded.commits.length,qualified_completions:decoded.qualified_completions.length,initialization_completions:decoded.initialization_completions.length};
const canonical=canonicalChainProof(raw,decoded,schema_status==='VALID'?compiled.document:null,recording),canonicalBytes=JSON.stringify(canonical,null,2)+'\n';
await fs.writeFile(path.join(output,'canonical-chain.json'),canonicalBytes);
value.canonical_chain=canonical;value.canonical_chain_ref={path:'canonical-chain.json',sha256:hash(canonicalBytes)};
value.initialization_lifecycle=canonical.initialization_chains;
value.analysis={mode:output===root?'WORKER_COMPILE':'OFFLINE_REVERIFICATION',input_source_tree_sha256:run.source_before?.source_tree_sha256??null,
 raw_trace_sha256:rawBytes?hash(rawBytes):null,worker_result_sha256:await fileHash(path.join(root,'worker-result.json')),run_inputs_sha256:await fileHash(path.join(root,'run-inputs.json')),
 compiler_files:Object.fromEntries(await Promise.all(['compile_run.mjs','decode_trace.mjs','metrics_compiler.mjs','canonical_chain.mjs','external_metrics.mjs','schedule.mjs','validate_metrics.py','initialization_basis.mjs'].map(async n=>[n,await fileHash(new URL(n,import.meta.url))]))),
 verification_spec_ref:{path:'.aegis/verification/G5-UR0b-initialization-v1/verification-spec.json',sha256:await fileHash(new URL('../../../../.aegis/verification/G5-UR0b-initialization-v1/verification-spec.json',import.meta.url))},
 completion_contract_ref:{path:'.aegis/verification/G5-UR0b-initialization-v1/completion-contract.json',sha256:await fileHash(new URL('../../../../.aegis/verification/G5-UR0b-initialization-v1/completion-contract.json',import.meta.url))}};
await fs.writeFile(path.join(output,'compiled-summary.json'),JSON.stringify(value,null,2)+'\n');console.log(JSON.stringify({status:value.status,common:value.common,critical_loss:value.critical_loss,identity_errors:value.identity_errors.length,schema_status,schema_reason}));
