# Bounded canonical material isolation

Anchor aabf0615 contains the retained7299f68 implementation and prior BLOCKED evidence. Old overflow isolation needs full composite+scratch;keeping sharp navigation raises DPR4 nominal demand above96MiB. Implement inside the existing Render owner. Semantic/Brush/FramePlan identities are unchanged.

Use existing Skia saveLayer only for final material,after coverage is fully evaluated on its original grid. Explicit integer device clip bounds the layer;original global matrix/shader origin stay unchanged. Reserve a conservative258x258x8-byte single-use lease before creating each layer. No pooled mutable surface/COW cycle is introduced;the current GPU epoch retains released quota until actual completion. GPU layer creation must retain the same recording context;CPU layer must expose real top-layer pixels. Failure restores layer state,keeps coverage and retries.

Add a private admission kind for qualified streamed isolated v4;it writes material tiles into the unpublished ordered composite and never allocates full scratch. Other admission kinds and command ordering stay intact. No mask/new material acceleration.
