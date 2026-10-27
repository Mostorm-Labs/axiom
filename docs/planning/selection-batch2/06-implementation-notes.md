# Batch 2 implementation and coverage notes

Execution base is Batch1 receipt 5daf623eba9a943706d813f8e197e00a9f6a8b2e, not main or an old bootstrap branch. Batch1 prerequisites were present and ten focused baseline CTests passed. Legacy SelectionSession::cancel still means explicit clear. All acquisition cancellation remains in the existing narrow region seam. Existing isolated worktree is reused on codex/selection-hierarchy-coverage-batch2; Batch1 branch/ref and prior evidence are preserved.

## Ownership

A: SelectionSession::resolveRoots is a pure projection over current canonical hierarchy; it rejects missing/cyclic ancestry explicitly, resolves Sticky/Group identity, observes Group scope, deduplicates and removes covered descendants in stable input order. The formal selection still has one owner.

B: Scene::hitTest defers Section interior backgrounds behind eligible content; border hits retain normal paint priority. Existing selection consumers share a scope-filtered point query. No canonical Section command changed.

C: Existing Shape analytic predicates remain. Complex visible geometry belongs to Render, using shared canonical Skia path/stroke constructors. Interaction composes current Scene candidates, current world transforms, published route/TextLayout and the same root resolver. No host performs selection; no second measurement, route planner or selection model was added.

D: Selection/no-write tests and existing transforms/Section/Text/Crop/Connector/viewport regressions. Actual Chrome script extends every first-batch check and adds Brush/Section coverage using DOM mouse input and the existing typed Section transport. No new browser transport is added.

## Named oracles / bounded coverage

| Kind/profile | Narrow-phase oracle | Automated coverage |
|---|---|---|
| Shape Rectangle/Ellipse/Triangle | Existing analytic predicates; explicit style uses renderer path/stroke geometry | First-batch independent coordinates and real Native/Chrome input; new stroke-only interior/border case |
| VectorPath | Shared semantic command-to-Skia path and boolean intersection, accepted fill rule | Literal quadratic arch peak and even-odd hole examples |
| VectorStroke v1 | Same nominal-width butt-cap segments as renderer | Diagonal hit and empty large-AABB corner |
| VectorStroke v2 | Persisted canonical Brush outline, not reconstructed samples | Literal outline/masks plus actual Brush producer and region input |
| DabStroke v1 | Separate square dabs, matching legacy paint | Dab hit and gap in union bounds |
| DabStroke v2 solid simple package | Same disk footprints as renderer | Disk versus corner/gap examples |
| DabStroke v2 textured/chalk/membrane/unsupported package | No pixel-alpha oracle fabricated | Explicit Unsupported test; not profile PASS |
| Connector | Existing published Ready route, styled line segments and derived arrowhead | Literal orthogonal body/empty-corner/arrowhead examples; pending/unavailable is explicit Unsupported |
| Image | Accepted crop frame and imageCropSourceBounds for Fit placement | Independent letterbox/trim coordinates and actual decoded import/Crop ownership regression |
| RichText | Existing TextLayout line fragments (typographic regions, not glyph-alpha outlines) | Literal fragments/gaps plus real font and real input; absent metrics remain Unsupported/Partial |
| Sticky/RichText | Sticky visible frame and existing child-to-parent identity | Pure identity fixture and real font/text child creation produces one root |
| Group | No pixels of its own; selected through eligible member geometry and existing scope promotion | Nested scope/roots and external reparent/foreground-overlap input regression |
| Section | Authored transformed boundary/background; interior is fallback behind content | Independent boundary example, real point/border/cancel, area and ancestor pruning |
| Erase masks | Same filled-path and max-radius round cubic stroke geometry as current painter, subtracted before intersection | Literal retained/erased points and transformed outline |

Skia boolean queries include boundary contact within 1e-4 local units. Existing Shape epsilon remains1e-9 and inverse determinant cutoff1e-12. Area fill/crossing remains accepted even-odd; no AABB-only success. Conservative missing-text extent handling may retain Partial for otherwise distant regions until resources arrive. This requires a projection scan and is not G5 performance qualification.

Section regions wholly inside its boundary use background only when no eligible foreground geometry is hit. Regions intersecting its boundary still target Section, then ancestor-root pruning removes its represented descendants. This is the region counterpart of content-before-background point targeting.

## Preserved failures and contract transitions

A's missing helper first failed compilation (API RED, not behavioral RED). B reproduced Section-background and out-of-scope foreground swallowing. C reproduced real Brush returning Partial/no selection and stroke-only interior falsely hitting. D reproduced nonempty text with no metrics being silently treated as empty; missing extent now yields Partial and retains S0. Initial invalid Ink tool id4100/stale binary and a Crop-cancel fixture were setup mistakes: corrected to existing4101 and existing cancel-discards-session behavior. Their raw failures are retained separately.

Two Batch1 unit expectations explicitly evolve with authorized provider coverage: mismatched VectorStroke+Shape content is now Invalid rather than unknown-provider Unsupported; Section's former missing-provider assertion becomes a Complete boundary query. No frozen corpus/oracle, historical evidence, SDK lock, schema or canonical operation changed.

Native and actual Chrome execution totals, exact subject/receipt revisions, commands and hashes are recorded only after final execution. Physical mouse/pen/touch, true IME, other platforms, full multi-edit, Group feature expansion, Snap/Alignment, G5/fault qualification and full302 runner remain NOT_RUN/out of scope. No Gate PASS or main merge is authorized.

## Independent review and fix pass

One fresh read-only whole-branch review at 6b62a443 identified three Important defects. Real mixed available/unavailable font input reproduced a Complete result with an incomplete layout; both the global unknown-extent fence and precise text provider now reject diagnostic-bearing layouts. A literal NoFill Ellipse plus published label fragments reproduced a missed label; the provider now applies the published scales and shape clip, with no second measurement. A real input rectangle over a Triangle miter reproduced a broad-phase false negative; Scene visual bounds now include admitted acute outer miters without changing authored geometry. The existing positive real-font label test alone was non-discriminating; the literal geometry test establishes its RED→GREEN.

Actual Chrome/WASM also reproduced styled Ellipse loss after an epsilon stroke was unioned into an even-odd selection. An isolated locked-Skia experiment proved that union collapsed the selection interior. Exact intersection now runs first and boundary contact is tested separately. The independent filled-Ellipse coordinate test failed before the change and passes afterward.

The broader Native matrix exposed an existing arrow raster fixture defect: no published Connector route was supplied. Compiling and linking the unchanged Batch1 renderer separately reproduced the same green-pixel assertion. The fixture now supplies the real derived route; the expected pixel is unchanged. A direct-SkPath test also consumed locked SDK headers as project headers; it now links the imported SYSTEM SDK target, retaining own-source /WX. Failed attempts and the baseline probe are preserved.

Review rulings: unsupported textured/chalk/membrane Dabs and pending/unavailable Connector routes remain explicit Unsupported; unavailable metrics remain Partial, never AABB-only success. Typographic fragments are the declared text oracle, not glyph-alpha outlines. Physical input/IME, other platforms, full multi-edit, Group expansion, G5 and Gate qualification remain NOT_RUN. Existing nested Section interior fallback policy stands; a different targeting policy requires a later product decision. Existing unrelated point-geometry behavior is not redefined here. Costs are conservative selection denial and possible later policy refinement, not silent partial mutation.

Deferred minor: repeatedly unioning segment/dab/mask paths on each region Move can scale poorly. No hang was demonstrated. No G5 performance claim is made; caching/acceleration is deferred to G5 rather than introduced in this batch.

## Real-consumer creation/topology regression

The first clean final Native matrix passed55/55, but actual Chrome browser-002 reached23 checks and exposed Section kCommandRejected after real Shape creation. Empty-scene Section creation succeeded; the same request with three real Shapes failed with and without Brush. A real Native producer→Facade regression reproduced the rejection and proved Shape2/3 had identical sibling keys. Existing CreationSession::makeOrder masked away the seed low bit. Section atomic validation correctly rejected the duplicate sibling keys. The narrow producer repair retains all64 seed bits and a nonzero suffix, above the maximum legacy creation prefix; historical objects/keys and Section validation are unchanged. It is included under D's real-consumer/topology regression, not a new semantic model. Browser failures, diagnostic probes, fixture setup mistakes and the Native behavior RED are preserved. The final matrix is rerun on the repair revision and now includes focused Creation/quick-create tests as well.
