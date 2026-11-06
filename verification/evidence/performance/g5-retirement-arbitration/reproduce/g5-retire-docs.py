from pathlib import Path
import json,hashlib,shutil
D=Path('verification/evidence/performance/g5-retirement-arbitration')
def read(n):return json.loads((D/n).read_text())
def write(n,v):(D/n).write_text(json.dumps(v,indent=2)+'\n')
r=read('result.json');runs=read('runs.json');comparisons=read('comparison.json');gpu=read('checks/g5-retire-gpu-results.json')
retirement=next(g for g in gpu if g['mode']=='retirement');records=[json.loads(x)for x in retirement['observation']['logs']]
r['scope']='GPU-retirement arbitration slice only. Previous global15% cold/150ms navigation targets remain open. Local Chrome synthetic SDK evidence;not all-platform/G5/P34/100K/1M acceptance.'
r['priorGlobalClosureStillOpen']=True
r['candidateDisposition']='Retained on controlled development branch for review;stable4238 unchanged;performance acceptance not granted.'
aa=json.loads(Path('out/g5-retire-aa-result.json').read_text());r['corroborativeAA']={'pairs':3,'sameArtifactFalseReports':len(aa['falseRegressionReports']),'interpretation':'One-frame wait variance is demonstrated by baselineA/A. This does not dismiss A/B input-gap findings or alter frozen guards.'};write('result.json',r)
for folder in Path('out').glob('g5-retire-aa-*'):
 if not folder.is_dir():continue
 for f in folder.rglob('*'):
  if f.is_file():
   dst=D/'corroborative-aa'/folder.name/f.relative_to(folder);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,dst)
shutil.copyfile('out/g5-retire-aa.py',D/'reproduce/g5-retire-aa.py')
write('assessment.json',{'sourceRevision':r['sourceRevision'],'baselineRevision':r['baselineRevision'],'obligations':{'retirement':{'status':'PASS'if retirement['status']=='PASS'else 'FAIL','observations':records},'parity-lifecycle':{'status':'PASS'if r['measurementBindingsAndPixels']and all(g['status']=='PASS'for g in gpu)else 'FAIL'},'regression':{'status':'PASS_NO_NEW_FAILURES'if r['regression']else 'FAIL','native':r['native']},'performance':{'status':'PASS_NONREGRESSION'if all(r['guards'].values())else 'FAIL','guards':r['guards']}},'result':r['status'],'officialGatePassClaimed':False})
c=r['dpr4Cold'];gap_before=max(p['beforeGapMs']for x in comparisons for p in x['phases']);gap_after=max(p['afterGapMs']for x in comparisons for p in x['phases'])
violations=[{'dpr':x['dpr'],'mode':x['mode'],'case':x['case'],'phase':p['phase'],'guard':k,'before':b,'after':a}for x in comparisons for p in x['phases']for k,b,a,limit in [('wait',p['beforeWaitMs'],p['afterWaitMs'],p['beforeWaitMs']+max(16.7,.1*p['beforeWaitMs'])),('input-gap',p['beforeGapMs'],p['afterGapMs'],p['beforeGapMs']+max(16.7,.1*p['beforeGapMs'])),('clarity',p['beforeLowDensity']or 0,p['afterLowDensity']or 0,(p['beforeLowDensity']or 0)+.02)]if (k!='clarity'or x['mode']=='on')and a>limit]
write('performance-violations.json',violations)
(D/'README.md').write_text(f'''# G5 GPU retirement arbitration

Result: **{r['status']}**,for this focused arbitration slice only. Prior material/navigation performance closures remain open. No official Gate,G5 or P34 PASS is issued.

Tested source:`{r['sourceRevision']}`. Paired baseline installed SDK:`{r['baselineRevision']}` (production-code equivalent to the stable task anchor07f55270). Demo: http://127.0.0.1:4239/example.html . Existing artifacts/servers/evidence remain separate. The candidate is retained for review on the development branch;stable4238 remains available and no performance acceptance is claimed.

## Corrected owner behavior

SkiaRenderer previously discarded the qualified navigation frame on critical admission failure and immediately requested cell eviction. SkiaNavigationTiles selected victims against charged bytes,including logically released GPU allocations. A real GPU allocation must remain charged until its actual completion callback,but those already-released bytes can satisfy future capacity without throwing away additional live feedback.

The stable priority is preserved:wait if existing pending quota suffices;otherwise retire the optional whole frame;recheck its released quota before reclaiming cells. Mandatory cell selection stops once enough space will exist after retirement. Optional replay admission/reclaim and its soft limit are unchanged. ResourceBudgetManager.reserve remains the sole actual admission gate;the projected count cannot allocate,shrink a lease,complete a GPU epoch or issue a receipt. Existing lost/stale/provider rules still apply.

No material-tile path,A2 replay extension,new resource pool,new SDK API or larger quota was introduced. Default32/96MiB,8MiB entry and existing CPU/GPU slices remain in force. This fixes avoidable collateral eviction;it is not a claim that all retained-frame value/capacity decisions are optimal.

## Real-owner RED/GREEN

The unchanged stable production owner fails two new acceptance cases:

- Released scratch is physically queued but already sufficient:old Renderer discards the qualified whole-frame feedback.
- A real8MiB GPU frame has been logically released:old Nav owner additionally evicts useful cells.

The corrected owner passes both. The released-frame control evicts0 cells;the live-frame control still evicts a needed cell. Actual allocation remains denied before real completion and succeeds afterward. Feedback does not increment canonical submissions;the resumed current frame exactly matches independent Direct pixels. Delaying clientWaitSync polls never reports false completion and never blocks the CPU. These private tests do not claim physical driver-OOM coverage.

Raw RED/GREEN,workers,source-test hash and exact artifact identities are under checks/. GPU retirement mode is one of10 actual Chrome/Ganesh modes. Core production changes are confined to private Render arbitration;the canonical Brush/Scene/FramePlan algorithms are unchanged.

## Paired measurements

There are {len(runs)} bounded browser workers (11 pairs),DPR2/3/4,diagnostics ON/OFF,pan/zoom,selected/unselected,60s foreground idle in the DPR2 selected-zoom cases,and5s CDP frozen lifecycle. DPR3/4 extra cases use1s idle. No builds/tests ran during this matrix. Final pixels,document/selection/Operation/receipt checks,physical framebuffer dimensions,clean source and stable installed artifact identities are independently verified.

| Representative metric | Baseline | Candidate |
|---|---:|---:|
| DPR4 after-freeze exact wait | {c['beforeWaitMs']:.1f}ms | {c['afterWaitMs']:.1f}ms |
| DPR4 after-freeze low-density time fraction | {c['beforeLowDensity']:.4f} | {c['afterLowDensity']:.4f} |
| Maximum input submission gap over matrix | {gap_before:.1f}ms | {gap_after:.1f}ms |

This slice's frozen nonregression guards require every ON-phase low-density time fraction<=baseline+0.02,and every phase's wait/input gap<=baseline+max(16.7ms,10%baseline). Deterministic delayed-GPU pressure must eliminate unnecessary eviction. The earlier broader15% cold-improvement/150ms goals were not relaxed or reclassified as passing;they remain separate open work. Source-density is a proxy,not optical blur;CDP freeze is not physical sleep;these timestamps are not input-to-photon/GPU execution timing.

Three predeclared identical-baseline A/A pairs were run after the complete A/B matrix,using the same parameters as DPR2 unselected pan and the same locked baseline artifacts on both sides. They also trigger the wait gate in {len(aa['falseRegressionReports'])} phases (about17ms). This demonstrates a one-frame wait-noise floor for the current single-pair method. It neither proves the candidate harmless nor dismisses A/B input submission-gap findings. A/A is corroborative and never replaces the frozen A/B decision;see corroborative-aa/ and checks/g5-retire-aa-result.json.

Guards:`{json.dumps(r['guards'],sort_keys=True)}`. Exact per-phase violations,if any,are in performance-violations.json. Do not interpret evidence-verifier exit0 as performance acceptance when result.json is BLOCKED.

## Regression and reproduction

Native:{r['native']['passed']}/{r['native']['tests']},the same5 known failures:{', '.join(r['native']['failed_names'])}. No new native failure. All10 private GPU modes,31Node tests,118 boundary contracts,normal installed ON/OFF root packages and actual root Demo smoke pass. Root packages expose only'.',omit private Runtime headers/archives and keep developer components OFF.

Run `python3.12 verification/evidence/performance/g5-retirement-arbitration/verify_evidence.py`. It recomputes the frozen guards and verifies SHA256,package authority/input locks,RED/GREEN,workers and source/artifact bindings. Build and worker recipes are in reproduce/. The code revision and evidence are durable in the controlled branch. This is not native Android/Windows evidence or100K/1M qualification.
''')
(D/'next-step-review.md').write_text('''# Remaining optimization boundary

This slice removes collateral eviction caused by counting already-released GPU bytes as if more live resources must be discarded. It preserves the stable source-before-cell priority and all admission/retirement gates.

First reconcile the timing evidence:the frozen A/B wait/input-gap guards fail,and identical-baseline A/A also crosses the wait threshold. Review repeated/ordered pairing and noise controls before attributing one-frame differences. A/B input-gap findings are not dismissed by the A/A wait control. Do not edit this archive or retrospectively relax its thresholds.

Continue toward exact coverage cost only after this slice is independently reviewed:the representative DPR4 path still has substantial CPU-inclusive contour/coverage replay. Review precise working-set/replay reuse under the existing limits,including SkiaRenderCache's own optional-source cleanup on actual mandatory pressure. Do not revive withdrawn material/A2 paths or promise a broad15%/150ms closure from the arbitration test alone. A new P31 should bind that wider cost work and its baseline before coding.
''')
shutil.copyfile('out/g5-retire-verify-template.py',D/'verify_evidence.py')
shutil.copyfile('out/g5-retire-docs.py',D/'reproduce/g5-retire-docs.py')
write('manifest.json',{'schema':'g5-retirement-arbitration-evidence-v1','files':{str(p.relative_to(D)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(D.rglob('*'))if p.is_file()and p.name!='manifest.json'}})
print('Finalized',len(read('manifest.json')['files']),'files',r['status'])
