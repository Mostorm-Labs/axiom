// Positive-chain reconciliation for sampling admission. No product policies.
const identities={runtime_instance:'runtime',lifetime_generation:'lifetime',attachment_epoch:'attachment',document_id:'document',document_epoch:'documentEpoch',resource_generation:'resource',view_id:'view',committed_generation:'semanticGeneration',scene_generation:'sceneGeneration',scene_revision:'sceneRevision',camera_generation:'camera',surface_generation:'surface',metrics_generation:'metrics',frame_id:'frame',operation_id:'operation',commit_revision:'commitRevision'};
const frameFields=['runtime','lifetime','attachment','document','documentEpoch','resource','view','semanticGeneration','sceneGeneration','sceneRevision','camera','surface','metrics','frame','operation','commitRevision'];
const same=(a,b)=>frameFields.every(k=>a[k]!==null&&a[k]!==undefined&&a[k]===b[k]);
export function canonicalChainProof(raw,decoded,document=null){
 const accepted=raw.filter(r=>r.kind===12&&r.tag===29&&r.values[0]==='1'),unresolved=[],chains=[];
 const receipts=document?.interactions.flatMap(i=>i.canonical_receipts)??null;
 const record=seq=>{const found=raw.filter(r=>r.seq===seq);return found.length===1?found[0]:null;};
 const completed=decoded.qualified_completions??[];
 for(const r of accepted){
  const candidates=completed.filter(c=>c.work===r.values[1]&&c.provenance.includes(r.seq));
  if(candidates.length!==1){unresolved.push({completion_seq:r.seq,reason:'accepted raw completion lacks a unique qualified decoded chain'});continue;}
  const c=candidates[0],frames=(decoded.frames??[]).filter(f=>f.work===c.work&&same(f,c));
  if(frames.length!==1){unresolved.push({completion_seq:r.seq,reason:'decoded completion lacks a unique full-identity frame'});continue;}
  const f=frames[0],draw=record(f.draw_seq),submit=record(f.provider_seq),capture=record(f.capture_seq);
  if(!draw||!submit||!capture||draw.kind!==5||draw.tag!==26||draw.values.length!==10||submit.kind!==11||submit.tag!==19||submit.values[6]!=='0'||capture.kind!==11||capture.tag!==29||draw.job!==submit.job||draw.basis!==submit.basis||draw.job!==c.job||draw.basis!==c.basis){
   unresolved.push({completion_seq:r.seq,reason:'successful draw/provider/capture record provenance missing or foreign'});continue;
  }
  if(receipts){
   const metrics=receipts.filter(receipt=>receipt.qualification==='QUALIFIED'&&Object.entries(identities).every(([key,field])=>receipt.basis[key]===c[field]));
   if(metrics.length!==1){unresolved.push({completion_seq:r.seq,reason:'canonical metric receipt missing or foreign to the actual native chain'});continue;}
  }
  chains.push({work_id:c.work,basis:Object.fromEntries(Object.entries(identities).map(([key,field])=>[key,c[field]])),draw_job:c.job,draw_basis:c.basis,
   attempt_jobs:f.attempt_jobs,draw_seq:f.draw_seq,provider_seq:f.provider_seq,capture_seq:f.capture_seq,completion_seq:r.seq,provenance:c.provenance});
 }
 if(decoded.errors?.length||decoded.loss?.critical)unresolved.push({reason:'critical loss or decoded identity error'});
 if(completed.length!==accepted.length)unresolved.push({reason:'raw accepted and qualified decoded completion populations differ'});
 if(receipts&&receipts.length!==accepted.length)unresolved.push({reason:'raw accepted and compiled metric receipt populations differ'});
 return{schema:'g5-ur0b-canonical-chain-proof-v1',scope:document?'NATIVE_AND_METRICS':'NATIVE_ONLY',status:unresolved.length?'MISSING':accepted.length?'QUALIFIED':'NOT_OBSERVED',
  accepted_raw_count:accepted.length,qualified_decoded_count:completed.length,metric_receipt_count:receipts?.length??null,chains,unresolved,
  timing_scope:'native accepted/software submission correlation; no physical display, pure GPU duration or performance acceptance'};
}
