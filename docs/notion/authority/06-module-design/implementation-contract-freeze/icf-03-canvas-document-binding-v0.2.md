# ICF-03 CanvasDocumentBinding Contract v0.2

> Status: **Current Proposed Freeze Candidate — CanvasDocumentBinding Implementation Contract**
>
> Notion source: `3c84c57a-590c-8157-839a-fb8778815537`
>
> Supersedes: ICF-03 v0.1.

## Change reason

v0.1 exposed `detach(binding, reason)` but its older IDL annotation attempted to order by CanvasHandle. v0.2 closes that machine inconsistency without changing the stable detach signature, ownership, cardinality or generation model.

For current codegen, ICF-00 v0.3 further canonicalizes the annotation to the concrete path `ordered_by: input.binding`.

## Ownership/cardinality

```text
Shared Data Runtime owns DocumentSession
-> issues LocalReady attachment capability
Platform Host owns CanvasDocumentBinding association state
-> Axiom realizes Canvas <-> DocumentHandle attachment
```

- Canvas: 0..1 active binding.
- Document: 0..N Canvas bindings.
- binding state is process-local and is not persisted or synced.
- `Canvas.destroy != DocumentSession.close`.
- `SurfaceLost != detach`.
- `DocumentHandle.destroy` requires active binding count zero.

## BindingGeneration

`bindingGeneration` is Host-assigned monotonic u64 per Canvas; 0 invalid. Every Unbound->Attaching and X->Y switch advances it. Delayed callbacks from an older generation are ignored. SurfaceGeneration is a separate namespace.

## Ordering/lifecycle

```text
Unbound -> Attaching -> Bound -> Detaching -> Unbound
```

Attach/detach enter the Axiom ordered runtime. Host must not allow two simultaneous mutable binding transitions on one Canvas.

Machine ordering:

```yaml
attach_document:
  ordered_by: input.canvas

detach_document:
  ordered_by: input.binding
```

Before detach mutation, runtime MUST resolve the live BindingHandle to its owning Canvas binding record. A stale/unknown old BindingHandle cannot detach a later binding on the same Canvas.

## Stable ABI non-change

Stable v1 surface remains logically `attach(canvas, desc)` and `detach(binding, reason)`; no CanvasHandle is added to detach solely to make ordering easier.

DetachReason mapping:

```text
SWITCH_DOCUMENT = 1
CANVAS_DESTROY   = 2
DOCUMENT_CLOSE   = 3
HOST_UNMOUNT     = 4
```

## Required conformance

- ordering key mechanically resolves from method input;
- old BindingHandle cannot detach a new Document binding;
- delayed old-generation callbacks cannot roll visible state back;
- Host queue/reject policy preserves final bindingGeneration correctness;
- stale detach is terminal/idempotent and cannot cross-talk into a new binding.
