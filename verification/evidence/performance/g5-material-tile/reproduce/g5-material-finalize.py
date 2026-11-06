from pathlib import Path
import json,gzip,shutil,hashlib
D=Path('verification/evidence/performance/g5-material-tile')
S=Path('out/g5-material-evidence-staging');assert S.exists() and not D.exists();S.rename(D)
def read(p):return json.loads(Path(p).read_text())
def write(n,v):(D/n).write_text(json.dumps(v,indent=2)+'\n')
def copy(src,dst):
 src=Path(src);dst=D/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or src.name.endswith('CMakeCache.txt') or (src.suffix=='.json' and src.stat().st_size>131072):dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
def tree(src,dst):
 for f in Path(src).rglob('*'):
  if f.is_file():copy(f,Path(dst)/f.relative_to(src))
r=read(D/'result.json');active=read('out/g5-material-rollback.json')
assert r['status']=='BLOCKED' and not any(r['guards'].values())
r.update(withdrawnExperiment=True,activeCandidate=active,activePerformanceAcceptance='NOT_ACCEPTED')
write('result.json',r);write('retained-candidate.json',active)
for f in Path('out').glob('g5-material-retained-*'):
 if f.is_file():copy(f,Path('retained-checks')/f.name)
tree('out/g5-material-retained-root','retained-root')
copy('out/g5-material-rollback.json','retained-checks/rollback.json')
for n in ['g5-material-finalize.py','g5-material-retained-build.py','g5-material-retained-package-checks.py','g5-material-verify-template.py']:
 copy(Path('out')/n,Path('reproduce')/n)
shutil.copyfile('out/g5-material-verify-template.py',D/'verify_evidence.py')
# Independent obligation evaluation applies to the tested experiment,not the restored default.
gpu=read('out/g5-material-window-gpu-results.json');material=next(g for g in gpu if g['mode']=='material')
variants=[json.loads(x) for x in material['observation']['logs'] if x.startswith('{"variant"')]
assessment={
 'testedSourceRevision':r['sourceRevision'],'activeSourceRevision':active['activeSourceRevision'],
 'capacity':{'status':'DEMONSTRATED_EXPERIMENTAL','qualified5MiBGpuCases':sum(x['variant']<8 for x in variants),'fullScratchBytes':max(x['scratch_peak_bytes']for x in variants if x['variant']<8),'boundedLayerPeakBytes':max(x['material_peak_bytes']for x in variants)},
 'parity':{'status':'DEMONSTRATED_EXPERIMENTAL','gpuCases':len(variants),'changedChannels':sum(x['changed_channels']for x in variants),'cpu':'Original path retained;11 native variants pass. Broad CPU layer prototype had30/35 changed channels and was rejected.'},
 'lifetime':{'status':'DEMONSTRATED_EXPERIMENTAL','oracle':'Material mode includes real admission denial,resume,cancel,actual GPU epoch retirement and delayed-poll retained-sharp integration;no physical GPU-OOM injection.'},
 'regression':{'status':'NO_NEW_FAILURES_EXPERIMENTAL','native':r['native'],'gpuModes':len(gpu)},
 'performance':{'status':'FAILED','guards':r['guards'],'cold':r['dpr4Cold'],'runs':r['boundedBrowserRuns']},
 'terminal':'FROZEN_VERIFICATION_FAILURE','decision':'Restore stable default. Experimental code remains reachable in Git;no production adoption or Gate PASS.'}
write('assessment.json',assessment)
c=r['dpr4Cold'];comparisons=read(D/'comparison.json');gap=max(p['afterGapMs']for x in comparisons for p in x['phases'])
(D/'README.md').write_text(f'''# G5 bounded material isolation experiment

Result: **BLOCKED / FROZEN_VERIFICATION_FAILURE**. The experiment is withdrawn from the default path. This is an implementation/performance finding,not an environment blocker or a Gate/P34 result.

- Tested corrected candidate: `{r['sourceRevision']}`.
- Paired installed-SDK baseline: `{r['baselineRevision']}`.
- Earlier one-layer-perRAF regression: `4a09233437cd88b92bd5c22f121e6d36bf99aed6` (preserved separately).
- Active restored source: `{active['activeSourceRevision']}`,production code exactly equal to `{active['codeEquivalentRevision']}` across the checked paths in retained-candidate.json.
- Stable restored Demo: {active['demo']}. Experimental4236/4237 remain separate and are not recommended defaults.

## What was demonstrated

Ganesh-only material layers can replace the full isolated scratch for qualified v4 overflow ink. Explicit integer material clipping preserves the original coverage grid,global matrix,shader input and order. Independent actual-GPU Direct comparisons have zero changed bytes in all {len(variants)} cases. The constrained1024x768 positive fixtures complete within5MiB with no full-frame scratch;the conservative per-layer peak is532512 bytes. No mutable pooled-surface/COW cycle or partial canonical publication is used.

Per-layer leases are reserved before draw and remain charged until actual GPU completion. Failure keeps ready coverage and does not advance its cursor. The protocol tests cover real quota denial,resume,cancel and delayed completion without losing the qualified sharp source. Physical driver OOM was not injected. CPU raster retains its original path:the rejected broad prototype changed30/35 one-LSB channels in fractional shear/rotation. Pixel tolerance was never relaxed.

## Why the default was restored

The first isolated-layer candidate needed503 exact attempts and8.49s DPR4 cold completion. Correcting working headroom removed that regression (75 attempts in the pilot),but did not satisfy the paired baseline target:

| Frozen measurement | Baseline | Corrected experiment | Decision |
|---|---:|---:|---|
| DPR4 after-freeze exact wait | {c['beforeWaitMs']:.1f}ms | {c['afterWaitMs']:.1f}ms | <15% improvement |
| DPR4 after-freeze time-weighted low-density fraction | {c['beforeLowDensity']:.4f} | {c['afterLowDensity']:.4f} | >2 percentage-point regression |
| Maximum candidate input submission gap over matrix | — | {gap:.1f}ms | >150ms |

Strict final pixels,document state,selection,Operations and receipt checks remain consistent. The corrected candidate still spends roughly0.53s CPU-inclusive time in exact coverage on the representative DPR4 cold trace. Saving scratch capacity alone does not reduce that replay cost. Retaining a full sharp image while reclaiming detailed navigation cells also exposes a cache-value/lifetime tradeoff. This resource explanation is an inference from measured resource/coverage traces;the quality fraction is a source-density proxy,not optical blur.

## Verification boundary

The complete corrected comparison contains {r['boundedBrowserRuns']} bounded browser workers (11 pairs),DPR2/3/4,diagnostics ON/OFF,selected/unselected pan/zoom,60s foreground idle in the DPR2 selected-zoom cases,and5s CDP frozen lifecycle. DPR3/4 extra cases use1s idle. No build/test workload ran concurrently with performance sampling. Source and artifact stability,worker cleanup,physical framebuffer dimensions and final pixels are checked independently.

Native: {r['native']['passed']}/{r['native']['tests']} pass,exactly the same five baseline failures;see result.json. All {len(gpu)} experimental GPU modes,31 Node tests,118 boundary contracts,normal installed root packages and actual root Demo smoke pass. The restored production code has separate fresh package,9 GPU-mode,8 focused native,Node,boundary and root Demo observations under retained-checks/ and retained-root/.

This is local Chrome/ANGLE synthetic SDK evidence. It does not establish physical input-to-photon,unique GPU memory,physical sleep recovery,Android/Windows parity or100K/1M qualification. The32/96MiB defaults,8MiB entry limit,CPU/GPU quotas,semantic/Skia locks,SDK public interface and previous G5/P34 results remain at the retained baseline.

Run `python3.12 verification/evidence/performance/g5-material-tile/verify_evidence.py` to recheck the archive. Exit0 verifies integrity and recomputed classification;it does not turn BLOCKED into a performance pass. Exact build/measurement recipes are in reproduce/. The tested experimental source is reachable through the branch history;the installed artifacts are bound by SHA256 in source-artifact-bindings.json and package checks.
''')
(D/'next-step-review.md').write_text('''# Return to Render resource-policy review

The material-isolation hypothesis is functionally demonstrated but insufficient for the frozen performance/clarity closure. Do not repeat P33 with increasingly larger working windows or relaxed thresholds. Preserve the exact rejected source/evidence and retained default.

The earlier owning design/package review should address the measured competition between a retained whole-frame image,an unpublished exact composite and useful high-density navigation regions. Evaluate bounded/shared retained regions and resource-value/lifetime policy together,under the existing hard budget. Separately account for the remaining exact coverage replay cost before promising a cold-wait gain. These are review directions,not a new authorized implementation contract or a revived A2 frontier-replay scope.

Carry forward the same canonical pixel/order/receipt rules and the local input-gap/clarity oracles. Reconcile the next P31 closure before coding any wider policy. Formal100K/1M qualification remains downstream. No semantic authority or old P34 evidence is changed.
''')
# Hash every durable file,including this verifier. No artifacts are silently excluded.
files={str(p.relative_to(D)):hashlib.sha256(p.read_bytes()).hexdigest()for p in sorted(D.rglob('*'))if p.is_file()and p.name!='manifest.json'}
write('manifest.json',{'schema':'g5-material-tile-evidence-v1','files':files})
print('Finalized',len(files),'files',r['status'])
