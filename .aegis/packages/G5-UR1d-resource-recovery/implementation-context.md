# UR1d implementation context

Start from published commit `9994c8492b46e8672dde0901003352b119d19e96` on `codex/geometry-navigation-integration`. C1, C2 and the P36 working-set repair are accepted within scope. The implementation thread remains the sole product and Git index writer. This is a development continuation, not an SDK release or permission to activate UR2.

## Current owners and execution points

- `SkiaRenderer` owns its prepared geometry, source retention, render cache, resource budget and GPU retirement queue. Keep these owners.
- `SkiaGpuLifetime` begins and ends GPU resource epochs and installs the Skia finish callback. Inspect every early return and the current ordering of provider present versus lifetime finish. A failed submit must not be hidden by a destructor after a successful result has already been chosen.
- `GpuResourceRetirement` retains completion service, ticket metadata and budget state. Its callback must not capture a Runtime, renderer or logical provider. Service each qualified completion-service lifetime once per owner pass, rather than repeating the same Skia poll for every ticket. Two generations sharing a numeric handle are not the same lifetime.
- `ResourceBudgetManager` keeps staging and pending-release bytes charged until ordered GPU epochs retire. A completed later epoch can still wait behind an older epoch. Do not confuse that state with an unobserved completion.
- `RuntimeCompositionCore::serviceFrameOpportunity` already calls the resource service. `CanvasEditingRuntime` only forwards to the canonical and preview renderers. Any forwarding change is limited to read-only resource progress and existing frame/retry scheduling.
- `WebRuntimeAdapter` owns the canonical/preview fences and captured `CanonicalWork`. `sdk/web/axiom.mjs` translates DOM/context events and schedules the existing private frame transport. Preserve public exports, types, arguments and semantics.

## Required state distinctions

```text
Admitted -> Recorded -> Submitted -> Completion observed -> Ordered retirement
    |           |           |                  |
 unused       failure    logical cancel     waits for older epochs
(no GPU       is not     does not undo      while quota remains charged
 submit)      success    submitted GPU work

Logical resize/loss != confirmed physical context loss.
Unknown abandon/deleted handle != successful completion.
A real physical-loss cleanup is labelled loss, never CanonicalVisible.
```

Keep full purpose/job/attempt/frame/context-lifetime/surface/resource identity. Capture immutable value metadata for callbacks; never restamp an old callback with current state. GPU submission, observed completion, quota retirement, platform presentation and canonical mutation remain separate facts.

## First incomplete work

Record the existing flow and resolved design before editing. Extend the current private observation and service paths, then exercise cancellation, out-of-order completion, pressure, target failure, physical context loss and rebind. Where an invariant already passes, preserve it; where it fails, retain a behavioral RED before fixing it. An undefined new method or missing counter is not a behavioral RED.

The previous working-set solution must remain: qualified dependencies survive discovery eviction; excessive working sets prepare, consume and recycle through the existing staging owner. The exact original one-long plus 512-short probe now completes in 4263 one-step calls, 513 builds, one present and zero legacy support/compile. Preserve this regression.

Use qualification-only fault injection at the actual allocation/result boundary when necessary and label it honestly. After a withheld completion observation is released, necessary resource work must drain through owner opportunities without another user edit. Resident caches, WASM heap and RSS need not become zero. Permanently impossible requests must reject distinctly; do not create endless RAF retries for an unknown or terminal condition.

## Scope and verification

Keep 32/96 MiB renderer budgets, 8 MiB entries, 512 discovery entries, aggregate preparation allowances, the existing 1024 epoch/ticket capacity and navigation's two preparation-ticket guard. These are existing structural guards, not newly measured latency thresholds. No tuned high/low watermark or latency SLO is authorized.

The frozen native selector preserves 54 existing tests and adds two resource tests. Reuse the original independent CPU/null-cache oracle and accepted GPU AA rule. Run matching new normal/qualification SDKs through four serial browser workers: two original S0/S2 core workers and two actual WebGL retirement/context recovery workers. All browser cases use DPR2; native resource/pixel cases cover DPR1/2/4. Do not claim real Web DPR4 or all-platform qualification.

Freeze sources, driver and artifacts before reserving a worker. Do not reopen old windows, add smoke runs or change a driver midway. Preserve every completed cold buffer and failure prefix. Return exact manifests, raw evidence, closed accounting and a proactive terminal callback. The implementation contract owns the complete finish line; do not stop at native-only success while required browser work remains executable.
