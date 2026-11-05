# Navigation detail and cold exact tail

SkiaRenderCache retains a completed full-frame composite,while SkiaRenderer separately captures an optional navigation framebuffer image. DPR4 frame35.83MiB exceeds optional32MiB and therefore loses that sharp navigation source. Share the completed immutable composite and its one pixel lease;validate existing complete stamps before use. A dedicated derived-resource class distinguishes prior shared-image reads from new composite writes,without another pool. Only actual image sampling extends use.

Canonical v4 jobs retain checkpoints under hard admission,but replay SkPictures use soft admission. Two mandatory full-frame buffers already exceed soft32MiB,so cold work repeatedly rebuilds/draws contour blocks. Add canonical-only replay policy using bounded free hard headroom;do not reclaim navigation for this optional acceleration. Existing standalone/legacy policy remains unchanged.

No unresolved semantic decisions. Extend existing Render owners;normal SDK wrappers/DTOs unchanged. Preserve all prior demos,locks,authority and Gate evidence. Baseline source90ef6b69;installed out/g5-budget-closed-{on,off}/js.
