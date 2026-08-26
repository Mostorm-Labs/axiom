# Implementation Contract Freeze v0.1

> Status: **Current First-Batch Implementation Contract Authority — Proposed Freeze Candidate**
>
> Notion source: `3c44c57a-590c-8121-9784-d246bbd9ccb6`
>
> Snapshot date: 2026-08-26

## Exact current source set

The First-Batch implementation contract MUST resolve exactly to:

1. ICF-00 Binding IDL Profile + First-Batch IDL Bundle **v0.3**
2. ICF-01 AxiomDataBridge vNext Contract **v0.1**
3. ICF-02 Axiom Platform Runtime Port **v0.2**
4. ICF-03 CanvasDocumentBinding Contract **v0.2**
5. ICF-04 PresentedFeedback Contract **v0.2**
6. ICF-05 Document Close Barrier Contract **v0.1**

Superseded ICF pages are historical comparison material only. A generator or implementation agent MUST NOT read them to fill missing current types, enum values, projection rules or behavior.

## Current gate

`Source Contract Closure Ready -> Repo Rematerialization / Reverification Pending -> Target Binding Evidence -> Frozen v1 Review`

This set is **not Frozen v1 yet**. Existing pre-reconciliation compile/ABI PASS evidence is stale for current authority and cannot promote the gate.

## Common ABI conventions

- C11-compatible stable boundary and `extern "C"` wrappers.
- Fixed-width integers; no C++ exceptions/STL/RTTI objects across ABI.
- Opaque integer handles; `0` invalid.
- Structs begin with `struct_size` and `abi_version` where applicable.
- Enum storage uses `uint32_t`.
- ABI extension is append-only; existing field offsets/meaning and function-table prefixes do not change silently.
- Async input spans remain immutable through completion unless copied/retained internally.
- Callbacks do not synchronously re-enter JS/host from Input/Render hot threads.
- Owned output buffers have explicit release semantics.

## Source-of-truth order

```text
00–04 accepted semantic authority
-> 05 ownership/dependency
-> 06 subsystem detailed design
-> 06 closure/integration resolution
-> Implementation Contract Freeze
-> generated headers / TS / ABI artifacts
-> platform realization
```

A convenience in ABI/codegen cannot redefine upstream semantic meaning.

## Freeze exit criteria

Promotion to Frozen v1 requires, at minimum:

- exact-current-source closure with no Superseded dependency;
- deterministic IDL lint for complete types, `ordered_by: input.*`, method I/O and projection routing;
- no duplicate PresentedFeedback or DocumentClose standalone APIs;
- rematerialization to Skeleton v0.2;
- separate AuthoritySet/NormalizedIDL/CAbiShape/GeneratedTSBinding/BehaviorVector fingerprints;
- regenerated ABI layout/function prefix review against historical comparison oracle, with unexpected shape differences BLOCKED rather than auto-blessed;
- behavioral vectors for stale binding, live surface/metrics/visibility presentation correctness, Approximate no-clear, DataBridge no-echo and close publication barrier;
- native C/C++ and TypeScript current-source compile/review;
- WASM and RN/JSI target binding evidence before final Frozen v1 review;
- dependency graph checks and no reopened top-level ownership gap.

## Non-changes

This freeze does not reopen 04 semantic authority, top-level module ownership, or 07 runtime data-flow. ABI numeric major remains 1 unless a current child explicitly changes it.
