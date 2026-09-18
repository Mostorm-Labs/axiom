# GT-G3-07 implementation context

G3-06 is closed, integrated through PR #92, and recorded in canonical `main`
at `6bb6c21ac62f0baef55f01ee0867ac7de08e501b`. G2 Scene/RuntimeScene and G3
Render Core already own every production stage needed for a Headless frame:
immutable `FrameState`, `VisibilityResolver`, `DirectReferenceSource`,
`FramePlanBuilder`, `FrameOrchestrator`, the private `SkiaHeadlessBackend`,
renderer-neutral `SurfaceLifecycle`, and `PresentationTracker`.

G3-07 adds only a fixed-fixture Headless Canvas Demo CLI. It must decode a
checked-in canonical `SemanticSnapshot`, restore it through the public semantic
bootstrap boundary, publish the corresponding production `Scene` and
`RuntimeScene`, then call the existing Render Core stages in order. The CLI is
not allowed to construct a final visibility result, draw list, reference
command, or frame plan itself, and it must not use test-only friend seams.

The fixture contains exact IDs 501 through 509 and one instance of each V1
`ObjectKind`. Its 256x256 transparent RGBA oracle reuses the accepted G3-05
nine-kind pixel semantics, including eight distinguishable pixel regions, one
supported VectorStroke erase hole, Group traversal with zero pixels, and digest
`fnv1a64:296a704f0d0b9165`. Unlike the backend unit test, G3-07 owns an
independent checked-in snapshot and expected descriptor; neither the CLI nor
the backend may generate or rewrite that oracle. The snapshot's local geometry
and transform literals are frozen so the production bounds/transform ordering
reproduces those regions; P32 must not substitute a different camera or
coordinate convention.

The executable contract is:

```text
axiom_headless_canvas_demo \
  --fixture runtime/render/fixtures/g3-07/nine-kind-v1.axsnap \
  --output-dir <directory>
```

The requested output directory must not already exist. A successful run builds
exactly `render.rgba`, `render-digest.txt`, and `render-evidence.json` in one
same-filesystem sibling staging directory, then publishes the complete directory
with one rename. Any input, decode, projection, render, or output failure exits
nonzero and leaves an absent output path absent; an existing output path is
rejected before staging and remains byte-for-byte unchanged.

First action: persist the frozen design preflight, add only the process-level
test/CMake target declaration needed to observe the missing-target RED, and
record that RED before adding the fixture or executable. Then implement the
smallest CLI/fixture boundary, execute the independent success/failure corpus,
run all predecessor matrices with the accepted published Skia SDK, and
materialize exact evidence under `.aegis/results/GT-G3-07/`.

This package does not authorize `CanonicalVisible`, `FrameCoverageManifest`,
Arc, camera interaction, hit/select, platform hosts, scheduling, tiles/caches,
or any G3-08+ behavior.
