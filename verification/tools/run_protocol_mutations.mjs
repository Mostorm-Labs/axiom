import { mkdtemp, readFile, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

const mutations=["duplicate-completion","stale-epoch","lease-leak","fault-transition","late-fence","transport-drop","boundary-equivalence"];
const entries=[];
for(const mutation of mutations){const output=await mkdtemp(join(tmpdir(),`axiom-${mutation}-`));const result=spawnSync(process.execPath,["tools/run_protocol_vectors.mjs"],{cwd:resolve("."),encoding:"utf8",env:{...process.env,AXIOM_PROTOCOL_MUTATION:mutation,AXIOM_PROTOCOL_EVIDENCE_ROOT:output}});const manifest=JSON.parse(await readFile(join(output,"corpus-integrity.json"),"utf8"));const failed=manifest.results.filter((item)=>item.status==="FAIL").map((item)=>`${item.vectorId}@${item.boundaryMode}`);if(result.status===0||failed.length===0)throw new Error(`${mutation}: mutation survived`);entries.push({mutation,status:"DETECTED",failedVectors:failed});}
const summary={format:"axiom-platform-protocol-mutation-summary-v1",suiteId:"protocol-seed-v0.1",mutationCount:entries.length,status:"PASS",entries};
const target=resolve("evidence/g0/gt-g0-06/mutation-guard-summary.json");await writeFile(target,`${JSON.stringify(summary)}\n`);console.log(`protocol mutation guards: ${entries.length}/${entries.length} detected`);
