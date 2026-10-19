# FC-01 current-source closeout verification

Result source: `e75b49c9eb14809d4ae584986fd9c9439dd78803`  
Execution branch: `codex/p32-windows-minimal-touch`  
Repository: `Mostorm-Labs/axiom`  
Frozen package: `c98110c015f975012f25db5efa66c9679da35630:.aegis/packages/FC-01/`  
Execution status: **BLOCKED_EVIDENCE**. This is an implementation-producer return,
not an independent Control Review, Gate verdict, or physical acceptance.

The user authorized the three closeout activities: frozen-contract reconciliation,
reuse plus necessary reverification/narrow repair, and source/artifact/evidence
alignment for review. FC-01A remains deferred for evaluation with FC-04 after
FC-02/03. No roadmap Authority was changed.

## Current observations

| Verification | Expected | Actual | Result |
|---|---|---|---|
| Native selected FC-01 and affected regressions | All selected tests run and pass at clean source | 129/129; [XML](native/ctest.xml), [commands/binary hashes](native/result.json) | PASS |
| Independent wire comparison | Frozen raw bytes match exactly; malformed/version/presence negatives reject | 25/25 [wire checks](wire/result.json), including exact 20/113/163-byte frozen vectors | PASS |
| Source-loss behavioral oracle | Anchored old implementation RED, candidate GREEN | Expected assertion RED at anchor; candidate and two ingress fixtures GREEN | PASS for behavior |
| Windows Minimal Host | Real locked-ABI build and running binary identity | MSVC 19.51.36260.0, D3D12 providers ready; exact source/dirty=0/PID logged | PASS for build/startup |
| Web Minimal consumer, Debug ON/full | Real WASM build, shared RuntimeComposition/Facade, Chrome regression | 13/13; correct source, dirty=0 | PASS |
| Web Minimal consumer, Debug OFF/off | Real WASM build, no Debug UI/Playground library linked, Chrome regression | 13/13; correct source, dirty=0 | PASS |
| Debug UI width | Bounded to 410 CSS px and fit narrow stage | 410/410/344 px for 1200/800/360 px stages; backing width 410 | PASS automated |
| New human touch/visual matrix | Actual per-item human observations | No new human operation this round | PHYSICAL_PENDING |

**25 wire checks do not mean all SH2-G01..G25 scenarios are closed.** The
[case mapping](shape2-case-mapping.json) separates executed comparators from partial
coverage. Scripted browser PointerEvents include capture stubs; they do not prove
physical digitizer, browser-trusted pointer capture, or human visual behavior.

## Narrow repairs and added verification

Only test/build registration and verification code changed in this round. Runtime,
input, rendering and host production behavior were preserved. Full file scope is
in [scope-audit.json](scope-audit.json).

- Fixed existing E09 `/WX` unsigned-to-float conversion and the isolated Shape2
  test's missing semantic-library linkage. No diagnostic suppression was added.
- Fixed a dispatcher whose generic argument branch prevented the Arrow/Line
  sequence tests from executing. Registered the real scenarios with execution markers.
- Updated stale fixture assumptions: unknown Snapshot version, frame allocation,
  owner-queue interleaving and stale-target Apply count. Snapshot1/Triangle remains
  a negative; rejected controls must leave all objects and Operation counts unchanged.
- Strengthened publication verification: submit alone leaves Presented at zero;
  approximate and duplicate feedback reject; only exact deterministic qualified
  feedback advances it. Axis failures and failed rendering preserve already Applied
  state. This is a deterministic feedback adapter, not an observed OS present.
- Added reference/indexed, typed Insert/Restore, four-source atomic negative
  admission; collision preservation; legacy snapshot and attached-Triangle negatives.
- Added full-record selected-action preimage/Undo/Redo comparison across 33 graphs,
  and remote child/dependent Connector conflict checks. Existing Group semantics
  appear only as fixture data; no product Group command was introduced.
- Added full 128-bit selected ID readback with equal low bits, and 2,100 independent
  scanline/affine Triangle hit probes with frozen vertices, intrinsic resize and
  retained geometry under None styles.

Original failures, their classification and the exact correction are retained in
[diagnostic-findings.json](diagnostic-findings.json). Failed observations were not
converted to PENDING. Harness invocation errors are distinguished from candidate
implementation failures. The final source-qualified results supersede no historical
Gate, Golden, Authority, or evidence record.

## E01–E10 reconciliation

| Family | Current producer observation | Remaining closure requirement |
|---|---|---|
| E01 | Wire/typed/version/atomic comparators pass | Complete cross-family Shape2 and historical SH-G01..18 mapping |
| E02 | Snapshot/capability/replay/idempotency tests pass | Live mixed-version writable-session admission owner/seam and independent scenario |
| E03 | Current behavior GREEN; anchored behavior RED | Original pre-first-mutation RED chronology still unproven |
| E04 | Current independent Snap comparator passes | Independent review of exact current evidence |
| E05 | Analytic/affine hit, CPU probes and Scene regression pass | Complete edit-specific Full/Incremental and pinned raster matrix |
| E06 | Current selected/preimage/conflict comparators pass | Independent review of exact current evidence |
| E07 | Owner queue, stale target, full-ID readback and Facade subset pass | Complete frozen owner-phase/readback/two-view mapping |
| E08 | Current Windows/Web automation passes | Android remains explicitly deferred; no full-profile closure |
| E09 | Current publication/feedback/Axis comparator passes | Independent review of exact current evidence |
| E10 | Package, ancestry and this-delta scope audit pass | Overall terminal closure depends on the remaining families |

Each frozen requirement, oracle, expected/actual result and residual detection gap
is in [obligation-matrix.json](obligation-matrix.json). A missing label alone is not
treated as a new blocker. The listed gaps concern original failure modes, including
mixed-client loss, render/Scene divergence and owner/control coherence.

## Unified artifact identity

Native executable:

```text
C:\Users\NearHub\.codex\worktrees\p32-windows-touch\axiom\out\fc01-closeout\build\apps\minimal_runtime_host\axiom_minimal_runtime_host_windows.exe
SHA-256: 549d6f1f44c760713f06016b68975b8ba0d5be68bece02f87a02abeab4babfcc
Observed PID: 20228
Logged source: e75b49c9eb14809d4ae584986fd9c9439dd78803, dirty=0
Debug UI compiled: ON/full; runtime log debug_ui_enabled=1
```

[Startup log](native-startup/input.log) and [process-path/hash observation](native-startup/native-startup.json)
refer to the actual running binary. The hidden startup process was stopped after
inspection; a clean interactive shutdown is not claimed, and it is no longer running.
No touch input or F12 action was performed during this startup smoke.

Machine: HJS-OPS-ADLP-A08; Windows 11 Pro 10.0.22631; Intel i5-12450H; Intel UHD
Graphics driver 32.0.101.7082 and OrayIddDriver 17.50.19.949; HID-compliant touch screen.
The application window logged DPI 192 / DPR 2, physical 998×697 and logical
499×348.5. The separate PowerShell system inventory reports DPI 96 and one 1280×720
display. These are distinct process/window observations; per-monitor physical
coordinate validation remains pending. Details: [machine](provider/machine.json),
[display](provider/display.json), [toolchain/SDK](provider-facts.json).

Web binary hashes and source identity: [ON artifacts](web/on/artifacts.json),
[OFF artifacts](web/off/artifacts.json). [ON observations](web/on/browser-result.json)
and [OFF observations](web/off/browser-result.json) retain input source/pointer/generation,
object/Operation/frame counts, retry state and per-check PASS/FAIL.

## Commands

Actual commands and return codes are retained with the results. SDKs were consumed
from the locked Store/prebuilt Skia; no Semantic SDK, protobuf, or Skia rebuild.

```powershell
python tools/setup_build_environment.py --core --semantic --facts-output out/fc01-closeout/build-environment.json --status
python tools/setup_build_environment.py --semantic --offline --status
cmd /d /c out\fc01-closeout\final_native.cmd
python verification/fc01/verify_shape2_wire.py --probe out/fc01-closeout/build/runtime/semantic/tools/canvas_semantic_golden_probe.exe --output out/fc01-closeout/committed-wire
python verification/fc01/run_source_loss.py --vcvars "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" --label closeout-committed-e75b49c9
cmd /d /c out\fc01-closeout\final_web.cmd off
python out/p32-web/capture_variant.py off
node out/p32-web/run_browser_test.mjs fc01-closeout-off-e75b49c9 axiom_canvas_demo_off
cmd /d /c out\fc01-closeout\final_web.cmd on
python out/p32-web/capture_variant.py on
node out/p32-web/run_browser_test.mjs fc01-closeout-on-e75b49c9
```

Staging helpers are preserved under `provider/`; reusable tracked runners are
`verification/fc01/run_closeout.py`, `verify_shape2_wire.py`, and `run_source_loss.py`.
They intentionally preserve failed attempts and do not convert aggregate test success
into FC-01 closure. The committed-native run contains the full target list and regex.

## Remaining blockers and handoff

The original contract still requires missing exact matrices, live incompatible-session
admission, original RED chronology and the supported Android profile. Android was
explicitly deferred by the user; its state is preserved. The current absence of a
qualified mixed-client integration scenario is recorded as unproven integration,
not as an observed user-visible failure or a fabricated refusal PASS.

[Ownership handoff](ownership-handoff.json) returns the unresolved integration
owner/applicability and historical-order disposition to the controlling layer.
Remaining executable verification is explicit in the family/case matrices. Do not
waive frozen obligations, invent a peer registry, rewrite original RED chronology,
or issue READY_FOR_CONTROL_REVIEW/FC-01 Gate/PHYS PASS from these green subsets.

Earlier Windows/Web work and high-level user feedback are listed in
[inherited-evidence.json](inherited-evidence.json) and [user-feedback.json](user-feedback.json).
The inherited 79 native tests remain qualified at their original source; they were
not renamed as new-revision runs. No new per-item human PASS is inferred.
