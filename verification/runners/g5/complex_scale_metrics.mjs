// Engineering observations only: completion is scoped to the measured layer.
export function assessStage(row,expected){
 const reason=row.worker?.reason;
 const count=expected.axis==='objects'?row.objects:row.points;
 const valid=row.status==='completed'&&row.scope===expected.scope&&count===expected.count&&
   Number.isSafeInteger(count)&&count>=0&&row.checks&&Object.keys(row.checks).length>0&&Object.values(row.checks).every(x=>x===true)&&
   (!reason||reason==='completed')&&row.worker?.cleanup_complete!==false&&
   (expected.dpr===undefined||expected.dpr===row.dpr);
 return {classification:valid?'MEASURED':'BLOCKED_EVIDENCE',reason:valid?null:reason&&reason!=='completed'?reason:'incomplete_or_invalid_observation',last_stage:row.worker?.last_stage??null};
}
export function assertTierCoverage(rows,axis,tiers){
 for(const n of tiers)if(!rows.some(r=>r.axis===axis&&r.count===n))throw Error(`missing ${axis} tier ${n}`);
}
export function summarizeCache(rows){
 if(!rows.length||!rows.every(r=>Number.isFinite(r.budget_used_bytes)&&Number.isFinite(r.budget_recorded_bytes)))return null;
 return {peakUsedBytes:Math.max(...rows.map(r=>r.budget_used_bytes)),peakRecordedBytes:Math.max(...rows.map(r=>r.budget_recorded_bytes))};
}
