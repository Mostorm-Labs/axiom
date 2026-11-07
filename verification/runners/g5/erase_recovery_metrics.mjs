// Private verification ledger;never shipped through SDK exports.
export function canonicalRecoveryTiming(raw){
  const canonical=raw.nativeRecords.filter(r=>r.at>=raw.upAt&&r.text.startsWith('AXIOM_FRAME_TIMING ')).map(r=>JSON.parse(r.text.slice(19)));
  // Inclusive coverage owns nested compile/draw/replay;sum the parent once.
  return{diagnosticCanonicalAttempts:canonical.length,cpuCoverageMs:canonical.length?canonical.reduce((a,f)=>a+(f.stages.cpu_coverage?.inclusive_ns??0)/1e6,0):null,cpuCoverageExclusiveMs:canonical.length?canonical.reduce((a,f)=>a+(f.stages.cpu_coverage?.exclusive_ns??0)/1e6,0):null,maxCanonicalAttemptMs:canonical.length?Math.max(...canonical.map(f=>f.stages.attempt.inclusive_ns/1e6)):null,recoveryAfterInputMs:raw.ended-raw.inputEnded,wholePhaseMs:raw.ended-raw.started};
}
