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
`ObjectKind`. All nine records must remain in canonical storage, `Scene`, and
`RuntimeScene`. Only IDs 501 through 508 have pixel-relevant bounds and enter
`VisibilityResolver`, the Scene draw list, `DirectReferenceSource`, and backend
traversal. Group 509 is a structural record: it remains findable in
`RuntimeScene`, its own bounds may be empty, it is not manually injected into
pixel stages, and it contributes zero pixels. GT-G3-07 does not require Group
subtree bounds. Its 256x256 transparent RGBA oracle reuses the accepted G3-05
pixel semantics, including eight distinguishable pixel regions, one
supported VectorStroke erase hole, Group zero pixels, and digest
`fnv1a64:7b8eb8c70c65afe5`. The VectorStroke is the world-space strip
`[8,46]-[32,50]`; its local erase rectangle `[8,-4]-[16,4]` is transformed to
world `[16,44]-[24,52]`, so it clears exactly `[16,46]-[24,50]` while retaining
stroke pixels on both sides. Unlike the backend unit test, G3-07 owns an
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

Preserved implementation result `5dafe5892f543bfe4ca15bea72b83913c828c5bd`
already contains the CLI, fixture, external process oracle, full argument and
fixture failure matrix, non-integral Shape rejection, and field-wise descriptor
comparison. P31 v0.5 supersedes only the structurally contradictory v0.4
membership wording. The first incomplete action is to align durable evidence
to the nine-structural/eight-pixel contract, rerun the frozen verification at
the exact descendant result, and return the new pushed result to independent
`CONTROL_REVIEW`. No production semantic redesign is authorized.

This package does not authorize `CanonicalVisible`, `FrameCoverageManifest`,
Arc, camera interaction, hit/select, platform hosts, scheduling, tiles/caches,
or any G3-08+ behavior.
