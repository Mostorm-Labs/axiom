# Navigation detail and cold exact tail

SkiaRenderCache retains a completed full-frame composite,while SkiaRenderer separately captures an optional navigation framebuffer image. DPR4 frame35.83MiB exceeds optional32MiB and therefore loses that sharp navigation source. Share the completed immutable composite and its one pixel lease;validate existing complete stamps before use. A dedicated derived-resource class distinguishes prior shared-image reads from new composite writes,without another pool. Only actual image sampling extends use.

Canonical v4 jobs retain checkpoints under hard admission,but replay SkPictures use soft admission. Two mandatory full-frame buffers already exceed soft32MiB,so cold work repeatedly rebuilds/draws contour blocks. Add canonical-only replay policy using bounded free hard headroom;do not reclaim navigation for this optional acceleration. Existing standalone/legacy policy remains unchanged.

No unresolved semantic decisions. Extend existing Render owners;normal SDK wrappers/DTOs unchanged. Preserve all prior demos,locks,authority and Gate evidence. Baseline source90ef6b69;installed out/g5-budget-closed-{on,off}/js.

## Amendment A1

Pilot676aad1e improves cold low-density time fraction0.946→0.724 but cold completion1446→1312ms misses the frozen15% improvement guard. Preserve that intermediate evidence and the guard. Inspection found navigation planning still multiplies every resident tile by its transient3-buffer peak despite completed GPU tiles shrinking to one image;actual admission already accounts staging and pending GPU release. Plan resident capacity with one additional transient peak,while retaining the actual allocator as the final authority. Also allow navigation to discard obsolete camera-bound exact jobs after complete private stamp comparison. A pending job for the same demand must survive;new navigation never increments canonical submissions. No public API,default budget,quota,rasterization,receipt or semantic changes.

## Amendment A2

Candidate903761e4 yields DPR4 cold1477.7→1376.4ms and low-density fraction0.948→0.633,still misses the cold guard. Interrupted selection reports547ms submission gap versus70.9ms baseline. Trace shows five prepared-geometry denials and repeated prefix scans while completed mandatory frame pixels occupy the soft budget. Existing criticalReplay only bypasses soft admission for huge sources;extend its already bounded source-bound checkpoint fallback to small sources when expanded staging/path soft admission fails. Background/optional get() remains soft-admitted. Do not raise budgets or change geometry. Preserve failed903 results as corroborative,not final acceptance.
