// Positive-chain reconciliation for sampling admission. No product policies.
import {initializationBasis} from './initialization_basis.mjs';
const identities={runtime_instance:'runtime',lifetime_generation:'lifetime',attachment_epoch:'attachment',document_id:'document',document_epoch:'documentEpoch',resource_generation:'resource',view_id:'view',committed_generation:'semanticGeneration',scene_generation:'sceneGeneration',scene_revision:'sceneRevision',camera_generation:'camera',surface_generation:'surface',metrics_generation:'metrics',frame_id:'frame',operation_id:'operation',commit_revision:'commitRevision'};
const frameFields=['runtime','lifetime','attachment','document','documentEpoch','resource','view','semanticGeneration','sceneGeneration','sceneRevision','camera','surface','metrics','frame','operation','commitRevision'];
const same=(a,b,initial=false)=>frameFields.every(k=>(initial&&k==='operation'?a[k]===null:a[k]!==null&&a[k]!==undefined)&&a[k]===b[k]);
const uint=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<2n**64n;
export function canonicalChainProof(raw,decoded,document=null,recording=null){
 const accepted=raw.filter(r=>r.kind===12&&r.tag===29&&r.values?.[0]==='1'),unresolved=[],chains=[],initialization_chains=[],accepted_records=[];
 const receipts=document?.interactions.flatMap(i=>i.canonical_receipts)??null;
 const record=seq=>{const found=raw.filter(r=>r.seq===seq);return found.length===1?found[0]:null;};
 const completed=decoded.qualified_completions??[],initialized=decoded.initialization_completions??[];
 const nativeChain=(r,c,initial)=>{
  const frames=(decoded.frames??[]).filter(f=>f.work===c.work&&same(f,c,initial));
  if(frames.length!==1)return 'decoded completion lacks a unique full-identity frame';
  const f=frames[0],draw=record(f.draw_seq),submit=record(f.provider_seq),capture=record(f.capture_seq);
  if(!Array.isArray(c.provenance)||c.provenance.some(seq=>!record(seq)))return 'missing or ambiguous native chain provenance';
  if(!draw||!submit||!capture||draw.kind!==5||draw.tag!==26||draw.values.length!==10||submit.kind!==11||submit.tag!==19||submit.values[6]!=='0'||capture.kind!==11||capture.tag!==29||draw.job!==submit.job||draw.basis!==submit.basis||draw.job!==c.job||draw.basis!==c.basis)return 'successful draw/provider/capture record provenance missing or foreign';
  const fields=[[draw,['view','frame','sceneGeneration','sceneRevision','camera','surface','metrics']],[submit,['view','frame','sceneGeneration','sceneRevision','surface','metrics']],[capture,['work','view','frame','semanticGeneration','sceneGeneration','surface','metrics']]];
  if(fields.some(([event,names])=>names.some((k,n)=>event.values[n]!==c[k]))||['work','runtime','lifetime','attachment','view','frame','semanticGeneration','sceneGeneration','surface','metrics'].some((k,n)=>r.values[n+1]!==c[k]))return 'actual accepted/draw/provider/capture identity differs from decoded native chain';
  const seqs=[draw.seq,submit.seq,capture.seq,r.seq],times=[draw.at_ns,submit.at_ns,capture.at_ns,r.at_ns];
  if(seqs.some(s=>!uint(s))||!seqs.every((s,n)=>!n||BigInt(seqs[n-1])<BigInt(s))||times.some(s=>!uint(s))||!times.every((s,n)=>!n||BigInt(times[n-1])<=BigInt(s)))return 'actual native sequence/time chronology invalid';
  return {work_id:c.work,basis:Object.fromEntries(Object.entries(identities).map(([key,field])=>[key,c[field]])),draw_job:c.job,draw_basis:c.basis,attempt_jobs:f.attempt_jobs,draw_seq:f.draw_seq,provider_seq:f.provider_seq,capture_seq:f.capture_seq,completion_seq:r.seq,provenance:c.provenance};
 };
 for(const r of accepted){
  const member={completion_seq:r.seq,work_id:r.values[1],classification:'UNRESOLVED'};accepted_records.push(member);
  const reject=reason=>unresolved.push({completion_seq:r.seq,reason});
  if(accepted.filter(a=>a.seq===r.seq).length!==1){reject('duplicate accepted raw sequence');continue;}
  const commits=completed.filter(c=>c.work===r.values[1]&&c.provenance.includes(r.seq)),initials=initialized.filter(c=>c.work===r.values[1]&&c.provenance.includes(r.seq));
  if(commits.length+initials.length!==1){reject('accepted raw completion lacks a unique qualified decoded chain');continue;}
  const initial=initials.length===1,c=initial?initials[0]:commits[0],chain=nativeChain(r,c,initial);
  if(typeof chain==='string'){reject(chain);continue;}
  if(initial){
   const basis=initializationBasis(c,decoded,raw,recording);
   if(basis.status!=='PROVEN_FRESH_EMPTY_INITIALIZATION'){reject(basis.reason);continue;}
   initialization_chains.push({...chain,operation_reason:'NOT_APPLICABLE_INITIALIZATION',initialization_basis:basis});member.classification='INITIALIZATION';continue;
  }
  if(receipts){
   const metrics=receipts.filter(receipt=>receipt.qualification==='QUALIFIED'&&Object.entries(identities).every(([key,field])=>receipt.basis[key]===c[field]));
   if(metrics.length!==1){reject('canonical metric receipt missing or foreign to the actual native chain');continue;}
  }
  chains.push(chain);member.classification='COMMITTED';
 }
 if(decoded.errors?.length||decoded.loss?.critical)unresolved.push({reason:'critical loss or decoded identity error'});
 if(completed.length!==chains.length||initialized.length!==initialization_chains.length)unresolved.push({reason:'decoded and classified native completion populations differ'});
 if(receipts&&receipts.length!==chains.length)unresolved.push({reason:'committed native and compiled mutation receipt populations differ'});
 return{schema:'g5-ur0b-canonical-chain-proof-v2',scope:document?'NATIVE_AND_METRICS':'NATIVE_ONLY',status:unresolved.length?'MISSING':accepted.length?'QUALIFIED':'NOT_OBSERVED',
  accepted_raw_count:accepted.length,qualified_initialization_count:initialization_chains.length,qualified_committed_count:chains.length,qualified_decoded_count:chains.length,observed_qualified_decoded_count:completed.length,metric_receipt_count:receipts?.length??null,
  unresolved_accepted_count:accepted_records.filter(m=>m.classification==='UNRESOLVED').length,accepted_records,initialization_chains,chains,unresolved,
  timing_scope:'native accepted/software submission correlation; no physical display, pure GPU duration or performance acceptance'};
}
