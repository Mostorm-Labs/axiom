# Proposed next slice: bounded canonical material isolation

Status: DESIGN REVIEW INPUT ONLY. This note does not amend semantic authority,
freeze a new public API,authorize a new budget,or claim an implemented fix.

The paired903761e4 and c8bc77ec traces show that sharper resident navigation can
compete with canonical replay. In the c8 DPR4 cold pair,CPU coverage calls grow
2651→14376 and inclusive coverage534→721ms. The source image improves navigation
quality,but an early recording denial can leave an entire canonical job on
checkpoint replay. The final result records every measured pair and failed guard.

At636x923 CSS,DPR4,one2544x3692 RGBA frame is37,569,792 bytes(35.83MiB).
A retained sharp frame + new ordered composite + full-frame isolated scratch is
approximately107.5MiB before coverage,metadata or replay. That cannot coexist
under the unchanged96MiB hard cap. Dropping sharp content or refusing replay is
therefore a structural tradeoff in the current realization.

Recommended review direction:

1. Keep the original full-device coverage grid,matrix,AA,clip,contour order and
   Brush geometry. Continue creating the ordered offscreen canonical composite;
   it remains unpublished until all commands finish and normal stamps qualify.
2. Evaluate a256-device-pixel material-isolation intermediate for qualified v4
   solid ink instead of an entire framebuffer per isolated stroke. Preserve
   the existing two-stage premultiplication/quantization and target colorspace.
   A direct draw onto the destination is not an equivalent replacement.
3. Account material pixels,Skia copy-on-write/staging and snapshots in the same
   manager. Retire on actual GPU completion. Admit a current exact job and useful
   frontier content before optional prewarm,within the original total limit.
4. Preserve the proven full-frame/cooperative path for cases outside the narrow
   qualifier. No larger per-frame CPU budget,no synchronous whole-scene escape,
   and no approximation may qualify a canonical receipt.
5. Before implementation,freeze the qualifier and fallback in a new P31 slice.
   The oracle must compare exact Direct pixels at tile boundaries and large
   origins,rotations/reflections,clip edges,overlaps,opacity,color spaces,erase
   and mixed objects. Also cover quota denial,cancel,present failure,generation
   changes and actual GPU staging/retirement. Then rerun the same paired matrix.

The open design decision is whether this material-isolation change is the next
slice,or whether the product accepts a different resource profile. Existing64/192
results remain an experimental control;they do not resolve the default-budget
structural overlap or prove a universal device setting.
