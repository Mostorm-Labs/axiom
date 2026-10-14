# FC-00X Runtime Minimum Closure — P31 implementation context

## Execution identity

- Repository: `Mostorm-Labs/axiom`
- Starting revision and task anchor: `201cc8841a0e6de4cd4b8a3dfc6c337820d35704` (`ancestor`)
- Execution branch: `codex/fc00x-runtime-minimum-closure`
- This is a new shared-runtime package. It does not reuse the FC-01 package or authorize FC-01 feature work.
- FC-01 worktree remains at `C:/Users/Chado/.codex/worktrees/fc-01-creation-tools-p32/canvas` and is protected.

## Current flow found at the anchor

`Platform input adapter -> RuntimeAppBinding/PlatformInteractionIngress -> Host-owned InputRouter and InteractionSessionManager -> CreationSession/BrushSession/Selection/Viewport -> existing semantic/history owners`.

`Host -> SurfaceLifecycle/RenderTarget -> PresentationTracker and CanonicalHandoffEvaluator -> PreviewSurfaceController/Skia provider -> platform acquire/present feedback`.

`Debug UI/Product Shell -> CanvasRuntimeFacadeAdapter -> CanvasControlService -> CanvasControlBinding -> InkPlaygroundHost and its existing Document/Selection/Camera/History owners`.

The shared pieces already exist and must be reused. The gaps are boundary gaps: `RuntimeAppBinding::notifySourceLost()` discards the normalized cancel, unregister does not deliver a cancel to the Session owner, Host still composes frame/preview/canonical decisions, and `CanvasControlBinding::snapshot()` hard-codes availability/writability/busy instead of reading the real owner.

## Owner and port rules

- Keep one Document, one Selection owner, one Session owner set, one Frame owner and one RuntimeScene per View.
- Core remains manual-poll/single-owner; it creates no OS thread or message loop.
- Host remains the composition root and supplies a narrow port for session cancel, surface execution, and owner readback.
- Platform owns native surface/GPU acquire/present/context and reports real metrics and qualified feedback.
- Runtime owns input identity, dirty/stale/retry/handoff and retirement qualification.
- Canonical and Preview are independent surface roles. Accepted/flush/fence is not automatically physical Presented.

## S0 → S3 implementation slices

### S0 — Owner/Port Design Preflight

Record exact current call paths, port signatures, close/rebind order, immutable async captures, and owner-safe publication. Bind every S1–S3 obligation to a test and evidence artifact. Stop if any semantic or authority decision remains open.

### S1 — Input/Session Closure

Reuse `PointerKey`, `PlatformInteractionIngress`, `InputRouter`, `InteractionSessionManager`, `CreationSession` and `BrushSession`. Deliver source-scoped Cancel to the actual owner on source loss/unregister, reject late generations, isolate equal pointer IDs from different sources, and preserve zero-operation cancel semantics.

### S2 — Runtime Frame/Preview/Canonical Coordinator

Reuse `SurfaceLifecycle`, `PresentationTracker`, `CanonicalHandoffEvaluator` and `PreviewSurfaceController`. Add only a narrow coordinator/port that owns dirty/stale/retry, real metrics/DPR, role-independent loss/rebind and qualified handoff retirement. Async work owns immutable captures and cannot retain Host or Surface pointers after close/rebind.

### S3 — Control/Diagnostics Closure

Reuse `CanvasControlService` queue/receipt and existing facade. Preserve the request target/context fence, distinguish primary-only and selection-set intents, invalidate deleted/restored/rebound targets, merge style leaves without overwriting unrelated canonical values, and make snapshot/readback reflect real owner availability/permission/resource/busy/confirmed state. Keep Clear-style reset OPEN.

## First incomplete execution step

After Control Review accepts the exact package, P32 starts with RepositoryIdentity, PackageBindingPreflight, EvidenceContractPreflight, then S0 ImplementationDesignPreflight and the two frozen RED oracles. No production mutation is authorized before those observations are durable.

## Non-goals

No FC-01 Shape/Connector work; no public C ABI delivery; no complete Editor extraction; no background schema/object-list/export/Sync; no Android APK or physical evidence; no G5 performance work; no historical Gate/Authority/Evidence rewrite.
