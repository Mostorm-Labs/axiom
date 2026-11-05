import gzip,json,shutil
from pathlib import Path
D=Path('verification/evidence/performance/g5-canonical-budget');P='g5-budget-closed'
matrix=json.loads((D/'matrix.json').read_text());rev=json.loads((D/'result.json').read_text())['result_revision']
rows={r['profile']:r for r in matrix if r['dpr']==4};cold={k:r['phases'][-1]['summary'] for k,r in rows.items()}
old=(Path(__file__).parent/'README-template.md').read_text().replace('1cf3f46e8d1176de4fed14e697aa6ae65c40e860',rev).replace('g5-budget-final-matrix.py','g5-budget-closed-matrix.py').replace('4229 serves','4231 serves').replace('4230 serves','4232 serves').replace('Instrumented4230','Instrumented4232')
metrics=[('Canonical completion wait',lambda k:f"{cold[k]['settle_ms']:.1f}ms"),('Longest exact owner call',lambda k:f"{rows[k]['phases'][-1]['maxExactAttemptMs']:.1f}ms"),('CPU coverage total',lambda k:f"{cold[k]['exactStages']['cpu_coverage']['inclusive_ms']:.1f}ms"),('Exact attempts',lambda k:str(cold[k]['exactAttempts']))]
for label,fn in metrics:
 start=old.index('| '+label+' |');end=old.index('\n',start);old=old[:start]+'| '+label+' | '+' | '.join(fn(k) for k in ['baseline','default','expanded'])+' |'+old[end:]
console=json.loads(gzip.decompress(Path('out/'+P+'-dpr4-default/wheel.console.json.gz').read_bytes()));denials=sum('AXIOM_RENDER_RESOURCE_RETRY ' in x.get('text','') for x in console)
q=lambda k:100*cold[k]['navigation']['quality']['timeWeightedLowResolutionFraction']
peak=lambda k:max(p['maxSharedChargeBytes'] for p in rows[k]['phases'])/1048576
start=old.index("The default's six observed");end=old.index('## Verification and failed candidates')
b3=next(r for r in matrix if r['dpr']==3 and r['profile']=='baseline')['phases'][-1]['summary']['settle_ms'];a3=next(r for r in matrix if r['dpr']==3 and r['profile']=='default')['phases'][-1]['summary']['settle_ms']
old=old[:start]+f'''The default run contains{denials} observed coverage-quota retries that now complete
instead of terminating the SDK frame loop. The smaller quota regression also
rejects composite-only work that cannot hold its required coverage tile.
Expanded capacity improves the DPR4 post-freeze low-density source time fraction
from approximately{q('default'):.1f}% to{q('expanded'):.1f}%. This is a source-density
proxy,not an optical blur score or a physical GPU-memory reading. Maximum shared
charge observed at navigation opportunities rises from about{peak('default'):.1f}MiB
to{peak('expanded'):.1f}MiB. Canonical preparation peaks are not continuously sampled
by that navigation counter. Process-family RSS is retained separately;it can
count shared pages twice and cannot isolate GPU residency.

Keep32/96 as the default.64/192 remains a reproducible experiment;the measured
capacity/quality tradeoff is not a universal device-profile recommendation.
Cold exact wait and navigation detail remain open. DPR3 cold wait is
{b3:.1f}->{a3:.1f}ms in this ordered pair;do not promote these local results to
statistical latency equivalence. Next work should inspect remaining exact
CPU/checkpoint and scheduling costs,then supply/retain navigation detail under
explicit resource admission. Formal scale acceptance remains deferred.

''' +old[end:]
old=old.replace('Mandatory composite/scratch buffers use the same shared manager and existing','This slice covers supported raster/WebGL completion providers;existing Direct\nbehavior for unqualified GPU-completion providers is unchanged.\n\nMandatory composite/scratch buffers use the same shared manager and existing')
(D/'README.md').write_text(old)
shutil.copyfile(Path(__file__).parent/'verify-template.py',D/'verify_evidence.py')
measurements=[]
for r in matrix:
 c=r['phases'][-1];w=json.loads(Path(r['output']+'-worker.worker.json').read_text())
 measurements.append({'dpr':r['dpr'],'profile':r['profile'],'cache_mib':r['cacheMiB'],'shared_hard_mib':r['sharedHardMiB'],'cold_wait_ms':c['summary']['settle_ms'],'cold_max_exact_attempt_ms':c['maxExactAttemptMs'],'cold_exact_attempts':c['summary']['exactAttempts'],'cold_cpu_coverage_ms':c['summary']['exactStages'].get('cpu_coverage',{}).get('inclusive_ms'),'maximum_sampled_navigation_shared_charge_bytes':max(p['maxSharedChargeBytes'] for p in r['phases']),'process_family_peak_rss_mib':w['peak_process_family_rss_kib']/1024,'cold_quality_proxy':c['summary']['navigation']['quality']})
(D/'assessment.json').write_text(json.dumps({'measurements':measurements,'decision':'Keep default32/96;64/192 remains experimental.','limits':['Single ordered local run,not statistical latency equivalence.','1000ms foreground idle and5s CDP freeze are not physical sleep or OS GPU eviction.','Navigation charge observations are not continuous canonical peaks or physical GPU memory.','Process-family RSS may double-count shared pages.','DPR3 cold wait variation/regression and all cold exact tails remain open.','Source-density fraction is not an optical blur score.']},indent=2)+'\n')
shutil.copyfile(Path(__file__),D/'reproduce/docs.py')
shutil.copyfile(Path(__file__).parent/'build.py',D/'reproduce/build.py')
print('Docs bound to',rev,'DPR4 cold waits',[cold[k]['settle_ms'] for k in ['baseline','default','expanded']])
