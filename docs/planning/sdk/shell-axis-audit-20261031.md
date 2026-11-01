# SDK Shell / Axis interface audit and implementation — 2026-10-31

Branch: `codex/sdk-shell-axis-bridge`. Execution base: `4fc3ebfe1325c9197024f1cd7137e54473a1c76d`.
The exact materialized revision is the commit containing this report. This is a
technical source/consumer review and bounded implementation, not P34/Gate/PHYS
acceptance. Main, SDK locks, canonical schema and historical descriptor locks are
unchanged. Published preview assets are unchanged.

## Result

The interface adapter has real Windows and Chrome/WASM proof, including explicit
local-log replay. **Overall recovery-followed-by-editing is blocked** by a
reproduced Runtime identity defect in the already published clean DLL. See
[evidence/result.json](evidence/20261031-shell-axis/result.json) and
[the integration guide](shell-axis-integration.md).

## Findings and delivered changes

| Finding | Resolution / owner |
|---|---|
| The DLL had only creation-time `on_operation`; no dynamic subscribe/unsubscribe | Additive C/POD `axiom_subscribe`, `axiom_unsubscribe`, `axiom_dispatch_events`, caller-side C++ facade and corresponding JS subscriptions |
| The old callback discarded CanonicalCommitRecord metadata | Versioned packet metadata carries document/operation identity, source, before/after generation and actual commit stamp |
| Listener failure left a committed Operation without retry custody | Optional bounded data port encodes/reserves nodes and capacity before existing local canonical/Intent/History apply, splices at commit, retains bytes until explicit data-owner release; overload rejects before mutation |
| Axis status was always unavailable through the public Composition sink | External exact-ID reports now reach the existing LocalOperationSink status projection, with bounded receipts and no false document Saved/Synced assertion |
| Shell notifications were incomplete and callbacks could throw unhandled | Deferred/coalesced state subscriptions; explicit error/retry and unsubscribe lifetime; native callback reentry rejects |
| Remote apply was the only SDK-named incoming lane | Separate replay entry points use existing RestoreReplay source 4; network uses RemoteSync source 5. Both preserve IDs, deduplicate, avoid local echo/History; ordered batches have per-item outcomes |
| External data owner could not enumerate/retrieve held resource blobs | Public manifest snapshot and full-token validated owned byte copies; native tokens additionally reject foreign SDK instances |
| Snapshot lacked a true canonical through fence; close could discard an async handoff | Snapshot fence reads the actual canonical clock; close completes native composition, stops new ingress and waits for retained custody release; abandon remains explicit |
| Runtime identity repeats after snapshot restore | **Unresolved high-impact owner repair**, independently reproduced against the immutable published `4fc3ebfe1325c9197024f1cd7137e54473a1c76d` DLL; no callback-side ID rewriting |

The legacy construction-time callbacks retain their compatibility behavior. Use
new subscriptions/data custody for an Axis integration. Axis remains responsible
for storage, blob obligations, protocol frontiers, outbox/inbox and qualified cloud
ACKs. The SDK does not implement those systems or infer their success.

## Actual verification

- Strict Windows /MT DLL and /MD out-of-repository consumers: build exit 0;
  actual interactive consumer test, C LoadLibrary test and Shell/Axis bridge test
  each exit 0. Import/export inspection passed: 16 C/POD exports,
  no component/CRT DLL dependency. Commands/raw results:
  [qualified-external.json](evidence/20261031-shell-axis/qualified-external.json).
- Actual headless Chrome/WebGL2: 52 existing SDK checks,
  18 Shell/Axis checks, five frame scenarios and
  shipped-example CDP mouse/pixel checks passed; process exit 0. Synthetic DOM
  input is not physical IME/touch. Raw:
  [qualified-browser.json](evidence/20261031-shell-axis/qualified-browser.json).
- `SDKPublicResources` and `SDKPublicDocumentOperations`: 2/2 PASS, exit 0,
  including publication rejection with unchanged canonical state, status source,
  resource copy/token guards, actual empty commit fence and RestoreReplay source.
- TypeScript 5.9.3 strict `api_types_test.mts`: exit 0. Node public API checks:
  exit 0. Release packaging unit tests: 7 PASS, exit 0.
- Broader native regression: 93 actually executed, 91
  passed, 2 failed, exit 8; no zero-match or skipped-test success.
  Failures: FC02WindowsTouchShapeLabelEntry, G4InkPlaygroundSelection. The old selection assertion matches the retained
  pre-cleanup baseline comparison. The touch entry test passed two isolated reruns
  but failed both larger runs; it is not silently marked resolved. Raw JUnit and
  prior baseline comparison are retained alongside the result.
- Published-DLL recovery identity probe: **FAIL, exit 1**. Different payloads and
  distinct target objects produced the same OperationId `00000000000000800000000000000000`
  before checkpoint and after restore; two objects are accepted in memory.
  The independent wire-field reader uses the published proto fields and no
  production expected-value codec. Repro source:
  `verification/sdk_windows_dll/known_failures/recovery-identity/`.

Initial RED observations for missing data/replay/resource entries remain retained.
Intermediate cached native builds reported an old configure-time source stamp;
those observations are not used as exact-source proof. Both final consumers were
explicitly reconfigured and report `4fc3ebfe1325c9197024f1cd7137e54473a1c76d` + `dirty=true`, correctly describing
this uncommitted implementation at test time. `source-files.json` binds the tested
source bytes to the materialized tree; these are not clean release artifacts.

## Exact command families

Executed in this worktree with VS 2026 / MSVC 19.51.36246 and locked SDK Store
packages, including offline native/Web resolution. Native configurations consume
existing emitted AXIOM_PROTOC / target runtime; no Semantic dependency stack was
rebuilt.

```powershell
python tools/setup_build_environment.py --core --semantic --facts-output out/sdk-shell-axis-facts.json --status
python tools/setup_build_environment.py --semantic --offline --status
python tools/setup_build_environment.py --semantic --target web-wasm32 --facts-output out/sdk-shell-axis-web-facts.json --offline --status
cmake -S . -B out/preview-fresh-windows
cmake --build out/preview-fresh-windows --target axiom_sdk axiom_sdk_public_document_operations_tests --parallel 5
cmake --install out/preview-fresh-windows --prefix out/sdk-shell-axis-native --component AxiomSDK
python -X utf8 verification/sdk_windows_dll/run_external.py --prefix out/sdk-shell-axis-native --output out/sdk-public-api-audit/qualified-external.json
ctest --test-dir out/preview-fresh-windows -R '^SDKPublic(DocumentOperations|Resources)$' --no-tests=error --output-on-failure
ctest --test-dir out/preview-fresh-windows --tests-from-file <absolute regression-names.txt> --no-tests=error --output-on-failure --parallel 1
cmake -S . -B out/preview-fresh-web
cmake --build out/preview-fresh-web --target axiom_js_runtime --parallel 4
node verification/sdk_js/verify_browser.mjs out/preview-fresh-web/sdk/web <actual Chrome executable> out/sdk-public-api-audit/qualified-browser.json
npx --yes --ignore-scripts --package typescript@5.9.3 -- tsc --noEmit --strict --target es2022 --module nodenext --moduleResolution nodenext verification/sdk_js/api_types_test.mts
```

## Remaining work

Repair Runtime-owned identity allocation across checkpoint restore, including
new edits after recovery and persistent Axis deduplication, then rerun the same
known-failing consumer unchanged. This is not D07 OOM remediation, another Group /
Section design or an Axis callback workaround. Preserve the existing independent
OOM and MSE debt and resolve the touch-test failure before claiming its closure.
No new release, production readiness or physical acceptance is claimed.
