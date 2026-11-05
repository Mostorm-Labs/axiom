# Bounded canonical material isolation

Anchor aabf0615 contains the retained7299f68 implementation and prior BLOCKED evidence. Old overflow isolation needs full composite+scratch;keeping sharp navigation raises DPR4 nominal demand above96MiB. Implement inside the existing Render owner. Semantic/Brush/FramePlan identities are unchanged.

Use existing Skia saveLayer only for final material,after coverage is fully evaluated on its original grid. Explicit integer device clip bounds the layer;original global matrix/shader origin stay unchanged. Reserve a conservative258x258x8-byte single-use lease before creating each layer. No pooled mutable surface/COW cycle is introduced;the current GPU epoch retains released quota until actual completion. Only Ganesh GPU is qualified;new layer creation must retain the same live recording context. CPU retains its original full-frame path. Failure restores layer state,keeps coverage and retries.

Add a private admission kind for qualified streamed isolated v4;it writes material tiles into the unpublished ordered composite and never allocates full scratch. Other admission kinds and command ordering stay intact. No mask/new material acceleration.

## P31 qualification correction

The native characterization rejected the initial all-backend assumption. Scope is now Ganesh GPU only;CPU keeps the original path. This is a package correction before final implementation,not a relaxed pixel oracle. The same8 geometric fixtures must remain exact on both backends;the constrained5MiB capacity obligation applies only to qualified GPU. Earlier experimental evidence remains durable and is not marked PASS.

Mandatory quota mapping:SkiaRenderer owns the navigation alias and reclaim callback. Before dropping it,account already-pending GPU releases and reclaim disposable world tiles. A failure caused by retiring transient material must yield for completion. Add a real GPU delayed-poll test:pending exact work cannot discard the still-qualified sharp navigation frame or emit a canonical receipt. This completes the existing retained-frame/lifetime obligation;no new performance finish line.
