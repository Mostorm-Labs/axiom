// Qualification validation only. No consumer/render/driver override.
export function qualifyMaskConsumers(records,tags){
 const counters=records.filter(r=>r.kind===10&&r.tag===tags.mask_history);
 const upJobs=new Set(records.filter(r=>r.tag===tags.eraser_up&&r.kind===2).map(r=>r.job));
 const same=(a,b)=>a.job===b.job&&a.attempt===b.attempt&&a.basis===b.basis&&a.job!=='0';
 const owner=records.filter(r=>r.kind===5&&r.tag===tags.input&&r.values?.length===8);
 const commits=records.filter(r=>r.kind===6&&r.tag===tags.canonical_commit&&r.values?.length===8&&BigInt(r.values[5])>BigInt(r.values[4]));
 const bodies=records.filter(r=>r.kind===10&&r.tag===tags.exact_body&&r.values?.[0]==='1'&&BigInt(r.values[5]??0)>0n&&BigInt(r.values[6]??0)>0n&&r.values[7]==='0');
 const provedUp=new Set(commits.filter(c=>upJobs.has(c.job)&&owner.some(o=>same(o,c)&&o.values[3]===c.values[0]&&o.values[4]===c.values[1])).map(c=>c.job));
 const exact=counters.filter(r=>r.values?.[0]==='2'&&BigInt(r.values[4]??0)>0n&&BigInt(r.values[5]??0)>0n&&provedUp.has(r.job)&&bodies.some(b=>same(b,r)&&b.values[1]===r.values[1]&&b.values[2]===r.values[2]));
 const canonical=counters.filter(r=>r.values?.[0]==='3'&&BigInt(r.values[4]??0)>0n);
 const retained=counters.filter(r=>r.values?.[0]==='1'&&BigInt(r.values[4]??0)>0n);
 const published=records.filter(r=>r.tag===tags.scene_apply&&r.kind===7&&r.values?.length>=3&&r.values[0]===r.values[1]);
 const generations=new Set(published.map(r=>r.values[0]));
 const scene=records.filter(r=>r.kind===10&&r.tag===tags.mask_scene&&r.values?.[0]==='1'&&upJobs.has(r.job)&&generations.has(r.values[2])&&BigInt(r.values[7]??0)>0n&&commits.some(c=>same(c,r)&&c.values[5]===r.values[2]));
 const canonicalJoined=canonical.filter(r=>scene.some(s=>s.values[3]===r.values[1]&&s.values[4]===r.values[2]&&s.values[7]===r.values[3]));
 const state={owner_commit_Up_jobs:[...provedUp],canonical_new_history_Scene_joins:canonicalJoined.length,exact_history_Up_rows:exact.length,canonical_history_rows:canonical.length,retained_history_rows:retained.length,published_mask_only_Up_rows:scene.length,actual_Up_jobs:[...upJobs],geometry_source:'Existing immutable canonical ordered mask history; unchanged exact kernel and sequential Skia clear'};
 return{status:exact.length&&canonicalJoined.length&&scene.length?'OBSERVED':'MISSING',...state};
}
