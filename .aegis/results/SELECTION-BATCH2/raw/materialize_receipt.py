from pathlib import Path
import json,subprocess,hashlib,shutil,xml.etree.ElementTree as ET,re,zipfile
root=Path.cwd();out=root/'out/selection-batch2';dest=root/'.aegis/results/SELECTION-BATCH2';git='D:/Softwares/Git/cmd/git.exe'
def g(*args):return subprocess.check_output([git,*args],text=True).strip()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(name,value):
 p=dest/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
subject=g('rev-parse','HEAD');base='5daf623eba9a943706d813f8e197e00a9f6a8b2e'
assert subject=='6ab8d388e425a8c28a136dadf661ac7ad31f48aa';assert not g('status','--porcelain')
native=json.loads((out/'release-native-regression-commands.json').read_text());assert len(native)==3 and all(x['exit_code']==0 for x in native)
tree=ET.parse(out/'release-native-regression.xml').getroot();cases=tree.findall('.//testcase');assert len(cases)==77 and all(c.get('status')=='run' for c in cases)
assert not tree.findall('.//failure') and not tree.findall('.//skipped')
browser=json.loads((out/'browser-004/results.json').read_text());assert browser['exitCode']==0 and browser['assertions']==28 and browser['failed']==0
assert browser['initial']['revision']==subject and browser['initial']['dirty']==0
web=json.loads((out/'release-web-commands.json').read_text());python=json.loads((out/'release-python-commands.json').read_text());assert all(x['exit_code']==0 for x in web+python)
assert not dest.exists();dest.mkdir(parents=True)
# Accepted package documents are immutable; compare their bytes, not prose.
execution=json.loads((root/'docs/planning/selection-batch2/execution.json').read_text());archive=Path('C:/Users/Chado/Downloads/axiom-selection-batch2.zip');assert sha(archive)==execution['zip_sha256']
for d in execution['documents']:assert sha(root/'docs/planning/selection-batch2'/d['path'])==d['sha256']
changed=g('diff','--name-only',base,subject).splitlines()
assert not any(p.startswith(('runtime/semantic/','schema/','.aegis/results/SELECTION-BATCH1/')) or p.endswith('.lock.json') for p in changed)
def owner(p):
 if '/tests/' in p:return 'Regression/independent coordinate oracle/actual browser harness'
 if p.startswith('runtime/interaction/'):return 'Existing Interaction/SelectionSession and creation owner'
 if p.startswith('runtime/scene/'):return 'Existing Scene broad phase and point hit priority'
 if p.startswith('runtime/render/'):return 'Render visible-geometry provider and shared Skia constructors'
 return 'Accepted supplied package or scoped implementation documentation'
write('source-files.json',[{'path':p,'sha256':sha(root/p),'owner':owner(p)} for p in changed])
# Preserve stdout/stderr byte-for-byte, including setup failures and RED attempts.
for p in out.rglob('*'):
 if not p.is_file() or p.suffix.lower() not in {'.log','.json','.xml','.txt','.png','.py','.mjs','.cpp','.rsp'}:continue
 relative=p.relative_to(out);target=dest/'raw'/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
shutil.copyfile(root/'.superpowers/sdd/selection-batch2/progress.md',dest/'implementation-ledger.md')
commands={'native':native,'web':web,'python':python,'actual_chrome':{'argv':browser['commands'],'exit_code':browser['exitCode'],'checks':28,'raw':'raw/browser-004/results.json'}};write('commands.json',commands)
artifacts=[]
for relative in ['out/selection-batch1/native/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe','out/selection-batch1/web/apps/axiom_canvas_demo/axiom_canvas_demo_web.js','out/selection-batch1/web/apps/axiom_canvas_demo/axiom_canvas_demo_web.wasm','out/selection-batch1/web/apps/axiom_canvas_demo/host.js']:
 p=root/relative;artifacts.append({'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size,'source_subject':subject})
write('build-artifacts.json',artifacts)
review={'review_range':{'base':base,'head':'6b62a443b4e6364f4f9e290d68559e0c4daa5f84'},'reviewer':'fresh read-only gpt-6-astra, high; whole branch review required by executing-plans','assessment':'with fixes: 3 Important, 0 Critical, 1 Minor','independently_reran_tests':False,'re_review':False,'findings':[
 {'id':'R1','grade':'Important','test':'SelectionBatch2Text.PartiallyAvailableMixedFontGeometryAlsoPreservesSelection','red':'raw/review-text-red.log','fix':'diagnostic-bearing partial layouts are Unsupported; global unknown-extent query remains Partial and preserves S0'},
 {'id':'R2','grade':'Important','test':'SelectionCoverage.UnfilledShapeLabelUsesPublishedClippedAndScaledFragments','red':'raw/review-label-unit-red.log','fix':'reuse published label fragments/scales and shape clip; earlier real-font positive was not discriminating'},
 {'id':'R3','grade':'Important','test':'SelectionInput.MiterStrokeOutsideNominalBoundsStillReachesPreciseRegionQuery','red':'raw/review-miter-red.log','fix':'accurate acute miter bounds before indexing; preserve geometry-centered attachment transverse axes'},
 {'id':'M1','grade':'Minor','disposition':'deferred to G5','description':'repeated segment/dab/mask path unions per region Move may scale poorly; no demonstrated hang or benchmark claim'}],
 'additional_regressions':['styled Ellipse exact-intersection before contact ring (real Chrome+literal RED)','created sibling OrderKey low-bit loss (real Native+Chrome RED)','legacy arrow raster fixture missing derived route (expected pixel unchanged)','legacy Snap test assumes free endpoint despite FC05 attachment (all prior coordinate/guide/commit/camera checks retained)'],
 'rulings':'implementation-ledger.md; all Ruling lines carried verbatim','green':'raw/release-native-regression.xml + raw/browser-004/results.json'}
write('review-disposition.json',review)
write('result.json',{'task':'SELECTION-BATCH2','status':'BOUNDED_IMPLEMENTATION_COMPLETE_WITH_EXPLICIT_UNSUPPORTED_PROFILES','repository':'Mostorm-Labs/axiom','branch':g('branch','--show-current'),'base':base,'source_subject':subject,'source_tree':g('rev-parse',subject+'^{tree}'),'receipt':'Evidence-only descendant commit containing this directory; final HEAD returned separately','main_rechecked':'2fee75f66a7f96f6aac834228ede372a9abda59a','zip_sha256':execution['zip_sha256'],'batch1_reconciliation':execution['batch1_reconciliation'],'verification':{'native_ctests':{'registered':77,'passed':77,'skipped':0,'exit_code':0,'selection_input_gtests':27,'coverage_gtests':12,'execution':'MSVC19.51 Release /MT /WX; real HWND/WndProc with synthetic input, not PHYS'},'actual_chrome':{'checks':28,'failed':0,'exit_code':0,'execution':'actual Chrome155/WASM/WebGL2, headless SwiftShader, CDP DOM mouse facts; DPR1/2; typed existing Section/Facade transport; no physical DebugUI/IME claim'},'python':{'tests':18,'skipped':0,'exit_code':0},'module_boundaries':{'contract_files':102,'exit_code':0}},'coverage_notes':'../../../docs/planning/selection-batch2/06-implementation-notes.md','unsupported':['textured/chalk/membrane/unsupported-package DabBrush pixel-alpha geometry','pending or unavailable Connector route','missing or diagnostic-bearing text metrics: Partial/Unsupported; never successful empty query'],'not_run':['physical mouse/pen/touch and real IME','other platforms','glyph-alpha text oracle','full multi-object editing/Group expansion','Snap/Alignment product expansion','G5 performance/fault qualification','full302 runner'],'invariants':{'selection_only_changes_canonical':False,'selection_only_adds_operations_or_history':False,'legacy_cancel_toggle_reinterpreted':False,'schema_semantic_implementation_sdk_locks_changed':False,'five_supplied_documents_byte_identical':True,'historical_results_or_global_aegis_registries_changed':False,'main_merged':False,'pushed':False,'pr_created':False,'gate_dev_phys_release_claim':False},'remaining_product_test_failures':[],'review':'review-disposition.json','commands':'commands.json','source_files':'source-files.json','artifacts':'build-artifacts.json'})
summary='''# Selection Batch 2 — implementation receipt

Implemented A–D on the actual Batch1 branch subject; Batch1 prerequisites were PRESENT, not inferred from main. Legacy cancel remains explicit clear; region cancellation stays transient. No main merge or Gate/DEV/PHYS/release claim.

- Base: `{base}` (Batch1 receipt, preserved branch).
- Tested source: `{subject}`; clean before both final builds.
- Branch: `codex/selection-hierarchy-coverage-batch2`.
- Receipt: the evidence-only descendant containing this directory; final HEAD is reported separately. No PR/push created by this task.

## What changed and who owns it

A: one pure SelectionSession root projection handles scoped Group/Sticky identity, deduplication and ancestor pruning. B: Scene defers Section interior backgrounds behind eligible content; point consumers filter active Group scope. C: Interaction queries coherent Scene candidates and delegates visible paths/strokes, published routes, cropped Image and existing TextLayout to Render; geometry is never replaced by broad-phase AABB success. Scene bounds include acute miters. D: actual consumer and no-mutation regressions cover platform ingress and existing Section/Crop/creation behavior. Created sibling keys retain all seed bits; no historical key migration or new topology model. Exact changed paths, hashes and ownership are in [source-files.json](source-files.json).

## Actual final execution

| Execution | Result | Evidence |
|---|---|---|
| Locked Native SDK + Skia consumer, MSVC19.51 x64 Release /MT /WX | configure/build exit0 | commands.json; raw/release-native-* |
| Registered scoped Native CTests (real HWND, synthetic input) |77/77 PASS,0 skipped, exit0 | raw/release-native-regression.xml |
| Actual Chrome155/WASM/WebGL2 Minimal Host, headless SwiftShader |28/28 checks PASS, exit0 | raw/browser-004/results.json; screenshots/CDP |
| Python descriptor/semantic/host/font checks with emitted AXIOM_PROTOC |18 PASS,0 skipped, exit0 | raw/release-python-0.log |
| Runtime boundary checker |102 contract files PASS, exit0 | raw/release-python-1.log |

Commands with exact arguments/exit codes are in [commands.json](commands.json); emitted environment/toolchain identities and selected test names/discovery are preserved under raw/. Build directories were reused from Batch1 but artifacts were rebuilt from the clean tested source; old committed evidence was not modified. This is real browser execution, not Node-only simulation. All browser input facts and Native HWND fixtures remain synthetic, not PHYS acceptance.

## Geometry scope and invariants

The named per-kind oracle/profile table is in [implementation notes](../../../docs/planning/selection-batch2/06-implementation-notes.md). Covered: Rectangle/Ellipse/Triangle, VectorPath curves/holes, canonical vector Brush outline and legacy segments, simple solid Dabs, published Ready Connector body/arrowhead, Image crop/Fit frame, RichText typographic fragments, Sticky child identity, Group scope/promotion and Section boundary/background. Shape label fragments use the published scale/clip, not new text measurement. Filled/swept erase masks match the current painter. Textured/chalk/membrane/unsupported-package Dabs, pending/missing routes and unavailable/partial text metrics remain explicitly Unsupported/Partial, preserving prior formal selection. Text is typographic line coverage, not glyph-alpha precision.

Tests assert unchanged semantic generation, last Operation/object counts, History size/cursor availability and redo state for selection-only/cancel paths; preview is transient. Actual Chrome independently checks operation/object invariance. No host owns semantic selection or canonical mutation.

## Failures retained and review

One independent read-only whole-branch review found3 Important and1 Minor. The Important partial-font, unfilled-label and miter-bounds findings have behavioral RED→GREEN coverage. No second reviewer was used. A styled-Ellipse tolerance-union regression was caught by actual Chrome and a literal coordinate oracle; exact overlap is now checked before the contact ring. Real Native/Chrome ingress also caught old creation-key bit loss, now fixed in the existing creation owner.

Supplemental failures were not hidden: the initial Native matrix hit a locked-SDK header classification error, then a legacy missing-route arrow fixture; its expected green pixel remains unchanged. Expanded77-test execution exposed miter port-axis drift and an old Snap fixture's unconditional free-endpoint access. The former is fixed; the latter now checks FC05 attachment identity using independent reference candidates while retaining coordinate/guide/camera/commit assertions. Isolated old-unit probe commands and failures are preserved, not represented as a full historical baseline rebuild. Unknown-target setup attempts are setup failures, not zero-test passes. Earlier invalid fixture/tool/identity setup attempts are distinguished in the ledger/notes. Final complete77-test and28-check results above supersede these attempts for the tested source, without deleting them.

[Review dispositions](review-disposition.json) and the [complete decision ledger](implementation-ledger.md) preserve every ruling and deferred item. Minor deferred to G5: repeated growing path unions per Move may scale poorly; no demonstrated hang or performance claim. Physical input/IME, other platforms, full multi-edit, Group expansion, G5/fault qualification and full302 remain NOT_RUN. No remaining failed final automated check; unsupported profiles remain explicit limitations.

## Reproduction and integrity

Use locked emitted Semantic/Skia paths, run the saved release Native/Web scripts under the discovered VS2026/Emscripten environments, then the browser command in commands.json and Python checks. `--no-tests=error`, exact-name discovery and nonempty target matching are enforced. Do not assume these local absolute SDK/toolchain paths on another machine. Raw emitter whitespace is retained byte-for-byte; code whitespace validation excludes only raw artifacts. [build-artifacts.json](build-artifacts.json) binds local exe/JS/WASM hashes. manifest.json hashes all evidence payloads except itself. The receipt commit must be an evidence-only descendant and every source-file hash must match the tested subject.
'''.format(base=base,subject=subject)
(dest/'report.md').write_text(summary,encoding='utf-8')
write('manifest.json',[{'path':p.relative_to(dest).as_posix(),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(dest.rglob('*')) if p.is_file()])
print('materialized',dest,'files',sum(1 for p in dest.rglob('*') if p.is_file()),'bytes',sum(p.stat().st_size for p in dest.rglob('*') if p.is_file()))
