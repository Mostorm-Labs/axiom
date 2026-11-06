// Read-only validation of the frozen repair bundle, never a Gate acceptance tool.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {execFileSync} from 'node:child_process';
import {summarizeInputFeedback, summarizeRuntimeSubmits} from './method/feedback_metrics.mjs';

const dir=path.dirname(fileURLToPath(import.meta.url));
const root=path.resolve(dir,'../../../..');
const sha=b=>createHash('sha256').update(b).digest('hex');
const read=n=>fs.readFileSync(path.join(dir,n));
const json=n=>JSON.parse(n.endsWith('.gz')?gunzipSync(read(n)):read(n));
const text=n=>(n.endsWith('.gz')?gunzipSync(read(n)):read(n)).toString();
const git=(...args)=>execFileSync('git',['-C',root,...args],{maxBuffer:32*1024*1024});
function files(at=dir){return fs.readdirSync(at,{withFileTypes:true}).flatMap(e=>e.isDirectory()?files(path.join(at,e.name)):[path.relative(dir,path.join(at,e.name))]).sort();}
function canonical(value){if(Array.isArray(value))return value.map(canonical);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).sort(([a],[b])=>a<b?-1:a>b?1:0).map(([k,v])=>[k,canonical(v)]));return value;}
const encoded=v=>JSON.stringify(canonical(v));
if(process.argv.includes('--write-manifest')){
  const hashes=Object.fromEntries(files().filter(n=>n!=='manifest.json').map(n=>[n,sha(read(n))]));
  fs.writeFileSync(path.join(dir,'manifest.json'),JSON.stringify({algorithm:'sha256',files:hashes},null,2)+'\n');
}
const manifest=json('manifest.json');assert.equal(manifest.algorithm,'sha256');
assert.deepEqual(files().filter(n=>n!=='manifest.json'),Object.keys(manifest.files).sort());
for(const [n,h] of Object.entries(manifest.files))assert.equal(sha(read(n)),h,n);
const result=json('result.json'),binding=json('source-bindings.json');
assert.equal(result.status,'READY_FOR_CONTROL_REVIEW');assert.equal(result.officialGatePassClaimed,false);
assert.equal(result.physicalInputVerified,false);assert.deepEqual(result.budgetMiB,[32,96]);
assert.equal(result.codeRevision,binding.codeRevision);const code=binding.codeRevision;
for(const [n,h]of Object.entries(binding.productionFiles))assert.equal(sha(git('show',`${code}:${n}`)),h,n);
const scope=json('repair-scope.json');assert.equal(scope.classification,'IMPLEMENTATION_DEFECT');
for(const [n,h]of Object.entries(scope.trusted_refs))assert.equal(sha(git('show',`${code}:${n}`)),h,n);
for(const n of files(path.join(dir,'method')).map(n=>path.basename(n)))
  assert.equal(sha(read('method/'+n)),sha(git('show',`${code}:verification/runners/g5/${n}`)),n);
const oldResult='verification/evidence/performance/g5-retirement-arbitration/result.json';
assert.equal(sha(fs.readFileSync(path.join(root,oldResult))),binding.oldBlockedResultSha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(root,oldResult))).status,'BLOCKED');
const changes=git('diff','--name-only',scope.anchor,code).toString().trim().split('\n');
assert(changes.every(n=>n.startsWith('runtime/render/')||n.startsWith('verification/runners/g5/')||n==='.aegis/repairs/G5-PAUSE-RESUME/repair.json'));

const matrix=json('runs/matrix.json');assert.equal(matrix.length,result.finalRuns);
assert.equal(new Set(matrix.map(x=>x.id)).size,14);
const revs={old:'90ef6b691e7d980ea8ed59195b5311eb7e3da4d9',bad:'3fcf642bbfdda126246e29dd1b32ec7710154f62',fixed:code};
const pixelGroups=new Map(),artifactGroups=new Map();let sourceFingerprint;
for(const row of matrix){
  const r=json(`runs/${row.id}/result.json.gz`),w=json(`runs/${row.id}-worker.worker.json`);
  const expectedRed=row.label==='bad'&&!row.userCorpus;
  assert.equal(r.status,expectedRed?'FAILED_RESPONSE_GUARD':'MEASURED');assert.equal(row.status,r.status);
  assert.equal(row.expectedStatus,r.status);assert.equal(row.exit,expectedRed?2:0);
  assert.equal(r.setup.build.sourceRevision,revs[row.label]);assert.equal(r.setup.build.dirty,false);
  assert.equal(r.setup.build.sdkVersion,'0.3.9');assert.deepEqual(row.build,r.setup.build);
  assert.equal(r.sourceBefore.commit,code);assert.equal(r.sourceBefore.dirty,false);assert.equal(r.sourceAfter.dirty,false);
  assert.deepEqual(r.sourceAfter,r.sourceBefore);assert.equal(r.sourceStable,true);assert.equal(r.artifactStable,true);
  assert.equal(sha(encoded(r.sourceBefore.files)),r.sourceBefore.tree_sha256);
  sourceFingerprint??=r.sourceBefore.tree_sha256;assert.equal(r.sourceBefore.tree_sha256,sourceFingerprint);
  for(const [n,h]of Object.entries(binding.productionFiles))assert.equal(r.sourceBefore.files[n],h,n);
  for(const value of Object.values(r.artifactBefore))assert.match(value,/^[0-9a-f]{64}$/);
  assert.equal(r.wasmSha256,r.artifactBefore['axiom.wasm']);
  const packageKey=row.label+'/'+row.diagnostics;
  artifactGroups.set(packageKey,artifactGroups.get(packageKey)??r.artifactBefore);assert.deepEqual(r.artifactBefore,artifactGroups.get(packageKey));
  assert.equal(w.reason,expectedRed?'nonzero_exit':'completed');assert.equal(w.returncode,expectedRed?1:0);
  assert.equal(w.timeout_s,180);assert.equal(w.rss_limit_kib,3072*1024);assert(w.elapsed_s<180);
  assert(w.peak_process_family_rss_kib<=w.rss_limit_kib);assert.equal(w.cleanup_complete,true);assert.deepEqual(w.remaining_owned_pids,[]);
  assert(Object.values(r.checks).every(x=>x===true));assert.deepEqual(r.errors,[]);assert.equal(r.close.closed,true);
  assert.equal(r.raw.inputEvents.length,144);const resume=r.raw.marks.find(x=>x.kind==='resume').at;
  const pause=r.raw.marks.find(x=>x.kind==='pause-start').at;assert(resume-pause>=1000);
  const phase={...r.raw,started:resume,inputEvents:r.raw.inputEvents.filter(e=>e.started>=resume),nativeRecords:r.raw.nativeRecords.filter(e=>e.at>=resume),colorOps:r.raw.colorOps.filter(e=>e.started>=resume)};
  assert.equal(phase.inputEvents.length,72);assert.deepEqual(summarizeInputFeedback(phase),r.feedback);
  assert.deepEqual(summarizeRuntimeSubmits(phase),r.runtime);
  assert.equal(r.raw.before.generation,r.raw.after.generation);
  assert.equal(r.raw.before.lastCommit.operationId,r.raw.after.lastCommit.operationId);
  assert.deepEqual(r.raw.before.selected,r.raw.after.selected);assert.equal(r.raw.quiescence.pending,0);
  assert.deepEqual(r.raw.pixels,row.pixels);assert.equal(row.firstContentMs,r.feedback.firstContentResponseMs);
  assert.equal(row.firstAcceptedMs,r.runtime.firstAcceptedSubmitMs);assert.equal(row.maxContentGapMs,r.feedback.contentGapMs.max);
  assert.equal(row.lastReceiptMs,r.feedback.lastInputToLastCoarseReceiptMs);
  assert.equal(r.fixtureImageBytes,row.userCorpus?2334857:2076263);assert.equal(row.imageBytes,r.fixtureImageBytes);
  assert.equal(r.setup.state.objects,row.userCorpus?12:11);assert.equal(r.setup.options.lines,5);
  assert.equal(r.setup.options.points,2048);assert.equal(r.pinchDelta,row.delta);assert.equal(r.kind,row.kind);
  if(row.userCorpus)assert.deepEqual(r.setup.options.shapeKinds,['rectangle','triangle','ellipse']);
  if(row.diagnostics==='off'){assert.equal(r.runtime.status,'UNAVAILABLE');assert.equal(r.runtime.firstAcceptedSubmitMs,null);}
  else if(row.label==='fixed'||expectedRed){assert.equal(r.responseGuard.boundMs,150);assert.equal(r.responseGuard.pass,!expectedRed);assert.equal(r.responseGuard.actualMs,r.runtime.firstAcceptedSubmitMs);}
  if(row.label==='fixed')assert(r.feedback.firstContentResponseMs<=150);
  const pixelKey=[row.userCorpus,row.kind,row.delta].join('/');
  pixelGroups.set(pixelKey,pixelGroups.get(pixelKey)??r.raw.pixels);assert.deepEqual(r.raw.pixels,pixelGroups.get(pixelKey));
}
assert.equal(pixelGroups.size,3);
const tests=json('checks/g5-reentry-full-native-comparison.json');
assert.equal(tests.tests,1074);assert.equal(tests.passed,1008);assert.equal(tests.failed_names.length,66);
assert.deepEqual(tests.failed_names,tests.baseline_failed_names);assert.deepEqual(tests.new_failure_names,[]);
const baseline=JSON.parse(fs.readFileSync(path.join(root,tests.baseline)));
assert.deepEqual([...baseline.failed_names].sort(),tests.baseline_failed_names);
assert.match(text('checks/g5-reentry-full-native.log.gz'),/G5RetainedZoomReentry[^\n]*Passed/);
assert.match(text('checks/g5-reentry-native-red.log.gz'),/zoom-in canonical discarded wide coverage before resumed zoom-out/);
assert.match(text('checks/g5-reentry-node-release.log.gz'),/# pass 39\n# fail 0/);
assert.match(text('checks/g5-reentry-python.log.gz'),/Ran 16 tests[\s\S]*\nOK/);
assert.match(text('checks/g5-reentry-boundaries-release.log.gz'),/passed \(118 contract files\)/);
const gpu=json('checks/g5-reentry-gpu-results.json');assert.equal(gpu.length,11);
assert.deepEqual(gpu.map(x=>x.mode).sort(),['affine','background','contour','detail','exact-replay','frontier','oversized','quality','reentry','replay','retirement']);
for(const g of gpu){assert.equal(g.status,'PASS');assert.equal(g.exit,0);assert.equal(g.observation.status,'PASS');
  assert.deepEqual(g.observation.pageErrors,[]);assert.equal(g.observation.observation.code,0);
  const w=json(`checks/g5-reentry-gpu-${g.mode}-worker.worker.json`);assert.equal(w.cleanup_complete,true);assert.equal(w.returncode,0);
}
for(const p of json('checks/g5-reentry-package-checks.json')){assert.equal(p.fileCount,36);assert.deepEqual(p.forbiddenFiles,[]);assert.deepEqual(Object.keys(p.exports),['.']);}
assert.match(text('checks/web-sampling-CMakeCache.txt.gz'),/AXIOM_ENABLE_FRAME_TIMING:BOOL=ON/);
assert.match(text('checks/web-sampling-off-CMakeCache.txt.gz'),/AXIOM_ENABLE_FRAME_TIMING:BOOL=OFF/);
const smoke=json('checks/demo-smoke.json');assert.equal(smoke.length,2);
for(const r of smoke){assert.equal(r.canvases,2);assert.deepEqual(r.pageErrors,[]);}
assert(smoke[1].result.includes(code.slice(0,8)));
console.log(JSON.stringify({status:'VERIFIED_REPAIR_EVIDENCE',gatePass:false,codeRevision:code,finalRuns:matrix.length,gpuModes:gpu.length,nativeNewFailures:[],files:Object.keys(manifest.files).length}));
