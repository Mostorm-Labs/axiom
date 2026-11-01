# Sketch appearance v1 — accepted scoped implementation

Authorization: user's explicit hand-drawn v1 implementation request, 2026-10-31.
Base: Mostorm-Labs/axiom af5b1778284211c3834957084dd0a03e48308d69.
This additive contract leaves historical frozen authority/descriptors identifiable.
It does not declare a formal Gate or authorize SDK publication.

Canonical owner remains Semantic Operations; CanvasEditingRuntime owns commands,
seeds and transient sessions. Scene owns immutable derived paths; shared Skia paints
native, Web, previews and export. SDK returns owned DTOs only. Table/1 is preserved.

## Versioned values

Append property 0x102, SketchStyleV1 {version:1, seed:uint64 nonzero,
roughness:finite [0,3], enabled:bool}. Absence means regular rendering. Disabling
retains the record/seed; roughness zero is not an elidable default. Clear removes
the record; normal UI disable uses enabled=false. Re-enable, color changes, copy,
Undo/Redo and round trips preserve seed. Only explicit reseed replaces it.

Append PatternFillV1 to FillStyle (tag3): version1, color, kind diagonal1/cross2,
angle degrees [-180,180], gap [2,1024], width [0.1,64]. Defaults -45°,8,1 resolve
at command time. Outline width never changes these parameters. Pattern is valid
only on Shape/Sticky; sketch on Shape/Connector/Sticky. Other kinds are excluded.
Unknown versions reject. No new Operation kind: PatchProperties remains atomic.
Snapshot v3 reader bit128 and explicit sketch-style-v1/pattern-fill-v1 requirements
cover the two extensions; current all-reader profile255. Old bit127 rejects them.

Sticky's previously hardcoded yellow plate becomes its kind-specific absent fill
default (1,1,.5,1); explicit None must remain canonical so users can remove it.
No absent stroke is invented. Sticky explicit None requires a SketchStyleV1
record (it may be enabled=false): the first changed appearance patch materializes
that metadata/Runtime seed even if only paint is being edited. Future-configured
Sticky creation and the legacy appearance adapter use the same rule. This ensures
old closed Operation decoders reject the new field/value, and old Snapshot readers
reject bit128, instead of eliding None back into yellow. A bare explicit Sticky None
without versioned metadata rejects. No-op patches do not materialize metadata. Existing Sticky text continues through text layout.

## Generation revision1

Reference: Rough.js4.6.6 commit56a2762171b1294d643501e8d14f120db6b27bd7,
renderer.ts line bow/control-offset/multiple-pass family and math.ts seeded random.
This is a documented limited C++ adaptation, not full Rough.js compatibility.
Our fixed RNG is SplitMix64, separate domain constants for outline and hatches;
64-bit integers never transit JS Number. Fixed random coefficients are multiplied
by roughness; geometry does not change when only color/opacity/camera/transform
changes. Geometry is local; ellipse uses four cubic arcs. Segment noise is capped
by min(2,length/10)*roughness; connector endpoints/tangent directions stay exact; each route segment is subdivided into two display cubics so a straight line can bow without changing terminal directions.
Dashed outlines use one pass and unchanged configured width; solid double pass
uses (1-.25*min(roughness,1))x configured width, continuous at zero. Roughness0 is exact single-pass and does not disable
pattern fill. Separate outline/fill streams prevent density edits resampling ink.

Hatches are clipped to the canonical convex body. Maximum8192 hatch segments,
32768 commands,4096 connector segments; excessive density uses a deterministic
integer stride ceil(estimatedScanlines*families/8192), globally aligned with local origin.
This explicit versioned budget policy never allocates an unbounded intermediate.
No wall-clock-dependent output. Visual bounds include cubic control hull and stroke;
geometry/chrome, transform-handle scaling and snapping use authored geometry. Rough ink bounds are not snap targets and cannot feed jitter back into canonical sizes. Filled interiors hit as a whole. Connector decoration
follows routing; obstacle-sensitive segments attenuate in bounded steps then use
an exact segment if their control hull would intrude. Router semantics never change. Terminal routing failures retain the existing diagnostic renderer and explicit status; Pending retains its interactive style.
Appearance preview reuses the existing incremental connector preview owner only when stroke enable/width changes its clearance inputs; roughness/color/fill edits retain the admitted route. Transient reference records and chrome use preview bounds without publishing canonical scene changes.
Cache input includes geometry, seed/version/roughness, pattern geometry, dash and admitted obstacle envelopes;
color/opacity/transform/camera are excluded.

## Public appearance capability

Shared Session appearance commands: read, patch, defaults, begin, preview, commit,
cancel. JS root runtime.appearance and Windows caller wrapper expose owned JSON
DTOs; no mutable owner/scene/path handles. User parameters are optional patch fields;
unmentioned fields and individual seeds survive. Readback contains eligible/excluded
roots, the complete existing MultiSelectionStamp and per-parameter mixed states.
Group does not recurse. Same eligibility/permission/lock/stamp checks as existing
multi selection apply. Defaults are separate from document History; seeds are
reserved for creation preview and materialized into the object on commit.

Begin captures exact roots, full stamp and preimages. Preview changes only transient
render entries; commit checks the captured stamp and targets, applies one atomic
PatchProperties and one History item. No-op commits publish nothing. Cancel clears.
Foreign/expired/stale tokens reject and clear preview; no silent retarget. Tokens
include instance/lifetime/selection/semantic/scene/permission identities losslessly.
Commands run through existing owner-thread/reentry checks and report actual apply;
publication is distinct from external durability. Lifecycle invalidates the session.

SDK version remains the unreleased0.1.0 feature iteration (last published0.0.1);
styleVersion1 is a distinct rendering/data contract. No release is performed here.


Pinned primary reference sources:
- [Rough.js renderer4.6.6](https://github.com/rough-stuff/rough/blob/56a2762171b1294d643501e8d14f120db6b27bd7/src/renderer.ts)
- [Rough.js seeded math](https://github.com/rough-stuff/rough/blob/56a2762171b1294d643501e8d14f120db6b27bd7/src/math.ts)
- [Excalidraw shape integration reference](https://github.com/excalidraw/excalidraw/blob/master/packages/element/src/shape.ts)
