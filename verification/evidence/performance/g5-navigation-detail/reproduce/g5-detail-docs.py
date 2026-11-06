from pathlib import Path
import json,hashlib,shutil
D=Path('verification/evidence/performance/g5-navigation-detail')
active=json.loads((D/'retained-candidate.json').read_text());r=json.loads((D/'result.json').read_text());cs=json.loads((D/'comparison.json').read_text());runs=json.loads((D/'runs.json').read_text())
rows=[]
for c in cs:
 if c['case']!='selected-zoom':continue
 p=next(x for x in c['phases'] if x['phase']=='after-freeze')
 rows.append(f"| {c['dpr']} / {c['mode']} | {p['beforeWaitMs']:.1f} → {p['afterWaitMs']:.1f} ms | {100*p['beforeLowDensity']:.1f}% → {100*p['afterLowDensity']:.1f}% | {p['beforeGapMs']:.1f} → {p['afterGapMs']:.1f} ms |" if p['beforeLowDensity'] is not None else f"| {c['dpr']} / {c['mode']} | {p['beforeWaitMs']:.1f} → {p['afterWaitMs']:.1f} ms | diagnostics OFF | {p['beforeGapMs']:.1f} → {p['afterGapMs']:.1f} ms |")
failed=[k for k,v in r['guards'].items() if not v]
worst=max((p['afterGapMs'],c['dpr'],c['mode'],c['case'],p['phase']) for c in cs for p in c['phases'])
limits=['Local ordered pairs are not statistical latency equivalence.','Time-weighted low-density fraction measures source density,not optical blur.','1000ms/60000ms foreground idle and5s CDP freeze do not reproduce OS sleep or physical GPU eviction.','Shared charge is allocator accounting,not unique physical GPU residency;Skia may perform internal copies.','Process-family RSS may count shared pages twice.','Large zoom resolution debt and cold exact CPU raster work remain observable.','No Android/Windows/iOS or100K/1M Gate qualification is inferred.']
assessment={'status':r['status'],'guards':r['guards'],'failedGuards':failed,'worstAfterInputGap':{'ms':worst[0],'dpr':worst[1],'mode':worst[2],'case':worst[3],'phase':worst[4]},'limits':limits}
(D/'assessment.json').write_text(json.dumps(assessment,indent=2)+'\n')
text=f'''# G5 navigation detail and restoration tail

Result: **{r['status']}**. The c8 experiment below was withdrawn. Active branch
source is `{active['activeSourceRevision']}`;its Runtime/SDK/App code is byte-identical
to pre-A2 `{active['codeEquivalentRevision']}` across the checked paths. The proposed
critical-frontier small-source fallback remains an unfulfilled package obligation.

Measured experiment source `{r['sourceRevision']}` versus paired baseline
`{r['baselineRevision']}`. This is shared Render engineering evidence;it does not
replace an official Gate review. Frozen guards are retained:15% DPR4 cold wait
reduction,10 percentage-point low-density reduction,and<=150ms input submission gap.
Failed guards: {', '.join(failed) or 'none'}.

## Change and ownership

- SkiaRenderCache publishes one completed immutable canonical image and its charged
  pixel owner to navigation,instead of recapturing another optional full-frame image.
  DPR4 sharp overlap therefore survives the32MiB duplicate-image gate. Actual reads
  extend GPU lifetime;retention alone does not. No extra pool or default budget.
- Canonical v4 job-owned SkPicture replay uses bounded free hard headroom while
  preserving checkpoint fallback. It never reclaims navigation to admit this
  optional acceleration. Exact AA,original device grid,material and order stay fixed.
- Density planning uses resident images plus one transient preparation peak.
  Actual per-job admission,uncompleted GPU charges and two-batch backpressure remain
  authoritative. Matching exact jobs resume;obsolete camera-bound jobs release
  their offscreen buffers without a canonical submission.
- Withdrawn A2 experiment:critical missing-coverage navigation used source-bound
  replay when small-source staging/path admission failed. The cold pair regressed;
  this extension was removed from the active branch. Existing large-source policy
  and background optional soft admission remain.

No consumer DTO/API/version,Brush algorithm,semantic/schema/SDK lock or receipt
contract changed. Defaults remain32MiB optional /96MiB shared hard,8MiB entry;
8ms exact preparation,existing4096 coverage steps,128 GPU primitives and two
GPU preparation batches are unchanged.

## Paired local measurements

Corpus:636x923 CSS,DPR2/3/4,five2048-point scribbles,two shapes,image,sticky,table;
selected/unselected pan and zoom,public UI readbacks,foreground idle and CDP freeze.
All22 bounded workers use matching installed root SDK artifacts. Final pixels,
document/selection/operation state and receipt checks are bound in raw traces.

| DPR / diagnostics | Post-freeze exact wait | Low-density source time | Post-freeze input gap |
| --- | --- | --- | --- |
'''+'\n'.join(rows)+f'''

Worst candidate input submission gap across the full matrix:{worst[0]:.1f}ms
(DPR{worst[1]},{worst[2]},{worst[3]},{worst[4]}). See `comparison.json` for every
phase,including failed/slow cases. No finding is discarded from qualification.

## Verification

112 selected native tests:107 pass,the same five failures as the pinned baseline
(canvas_rf01_full_rebuild,canvas_fc02_shape_label_scene,G3HeadlessCanvasDemo,
FC00XRuntimeComposition,SketchSemantic). Nine real Chrome/WebGL2 private GPU modes,
31 G5 Node tests,118 Runtime boundary contracts,normal installed package checks and
root Demo smoke pass. RED logs cover sharp sharing,canonical replay,avoidable
density debt,obsolete exact buffer starvation and critical frontier admission.

The intermediate676aad1e pilot and pre-A2 903761e4 pairs remain separate from
measured c8bc77ec observations. No measured candidate passed all performance guards. The stopped903 dense orchestration is not used
as a complete matrix. Original P34,G3 fixtures and dependency locks are unchanged.

## Reproduce and inspect

1. For the withdrawn experiment,check out c8bc77ec;resolve accepted Semantic/Skia SDKs
   through `tools/setup_build_environment.py` and existing locked supply chain.
2. Use `reproduce/g5-detail-build.py` with prefix `g5-detail-final`;the script reuses
   configured host/Emscripten consumer builds. Captured CMake facts and install logs
   describe this machine's configuration. It never rebuilds Skia/Protobuf.
3. Build the target list and run the112 names in `checks`;run Node/boundary checks.
4. Stop builds/tests before `reproduce/g5-detail-final-matrix.py`. Baseline installed
   root SDK directories must match90ef6b69. Workers cap180sec/3GiB and own cleanup.
5. Recompile the evidence with `reproduce/g5-detail-collect.py` and regenerate docs/
   manifest with this script. Run `python3.12 verify_evidence.py`. Verifier success
   proves evidence integrity;`result.json` still owns READY/BLOCKED.
6. For the retained candidate,check out7299f68 and build prefix `g5-detail-retained`.
   `red-oracles.json` records native mutation steps and excludes invalid exploratory
   fixtures. GPU logs are GREEN conformance;no RED GPU run is claimed.

Interactive matching diagnostics-OFF Demo:
http://127.0.0.1:4235/example.html
Prefilled mixed scene:http://127.0.0.1:4235/mixed-navigation.html?dense=1
This serves restored7299f68,not withdrawn c8bc77ec. Current native focused tests,
GPU modes and root-SDK build identity/smoke are in `retained-checks`/`retained-root`.
The source-equivalent903 full112-test result is retained explicitly;it is not
mislabelled as a fresh112-test run at7299f68.
Older servers/artifacts were preserved. A blocked result makes this a review
candidate,not an accepted performance baseline.

See `next-step-review.md` for the proposed bounded material-isolation design.
It is review input,not an implementation amendment.

## Limits

'''+'\n'.join('- '+x for x in limits)+'\n'
(D/'README.md').write_text(text)
shutil.copyfile('out/g5-detail-verify-template.py',D/'verify_evidence.py')
shutil.copyfile('out/g5-detail-docs.py',D/'reproduce/g5-detail-docs.py')
files={str(p.relative_to(D)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(D.rglob('*')) if p.is_file() and p.name!='manifest.json'}
(D/'manifest.json').write_text(json.dumps({'schema':'g5-navigation-detail-evidence-v1','sourceRevision':r['sourceRevision'],'baselineRevision':r['baselineRevision'],'files':files},indent=2)+'\n')
print('Wrote',r['status'],'manifest',len(files),'files;failed guards',failed)
