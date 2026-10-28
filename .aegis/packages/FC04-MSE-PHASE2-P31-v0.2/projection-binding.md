# Narrow Phase 2 projection and receipt binding — candidate

## Accepted facts

The current RuntimeFacade read path exposes SelectionSummary with three aggregate slots; the existing CanvasControlReceipt has no frozen structured per-root outcome. The Windows Minimal host wraps the composition facade in MinimalRuntimeFacade; Web passes RuntimeComposition::controlFacade() to DebugController. The v0.1 allowlist omits the shared Facade and its actual Windows/Web integration path.

## Required behavior

An immutable feature-owned projection must bind the document/runtime identity, selection revision, semantic generation and Scene publication to normalized root IDs. For each property it must carry applicable/total counts, known/unknown state, Uniform/Mixed separately from Partial, and each excluded root's typed semantic reason. A missing provider/query failure is Indeterminate/Unavailable, not Unsupported. The UI may show and request an explicit eligible cohort; it must not derive applicability from object-kind switches or receive mutable Runtime internals.

Admission rechecks the binding and all eligible targets for lock, permission, conflict, validity and payload. One typed existing operation or D07 Intent commits the entire eligible cohort; a failed admission changes no Document, Scene, History or redo state. A truthful typed result identifies changed and semantically skipped roots and reasons. Selection changes after a commit do not alter the Undo target.

## Candidate, not yet a frozen interface

Investigate an internal debug-only typed provider and immutable snapshot, supplied by the composition owner to both hosts while commands remain on the existing owner queue. It must not alter RuntimeFacade's existing vtable, public C ABI, canonical schema, registry or persisted state; it must not repurpose generic error strings as an undocumented protocol or introduce a global registry. Windows and Web must consume the same semantics. Static linkage of the existing debug UI is evidence for feasibility, not proof that changing any public C++ layout is ABI-safe.

Before this can be bound: identify exact headers, CMake dependencies and host adapters; establish whether their C++ layout is public/stable; specify lifecycle, owner thread, staleness, optional-provider behavior, exact result representation and independent test inputs/oracles. If this cannot be achieved within the accepted ABI restriction, route a narrowly scoped Authority/architecture decision rather than weaken the accepted rule or guess a type.

The source snapshots needed for that work are locked in source-delta.json. This candidate is not implementation authorization.
