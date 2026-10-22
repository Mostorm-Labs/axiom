# Font Catalog Facade and Shape Embedded Rich Text Implementation Plan

> Execute this plan in the isolated FC-02 worktree. Preserve the existing FC-02 text,
> resource, debug UI, and platform contracts. Use the locked Semantic and Skia SDKs.

## Goal

Expose manifest-backed font/resource state through `RuntimeFacade` and add a canonical
RichText label embedded in Shape content, with versioned persistence, existing Text
Runtime editing, Scene projection, canonical/preview rendering, and focused regression
coverage.

## Phase 1: RED tests and contract fixtures

1. Add semantic fixtures for Shape v1/v2 legacy content, Shape v3 with absent label,
   empty label, and mixed-style label.
2. Add tests for protobuf round-trip, canonical encoding/digest, validator capability
   requirements, Snapshot bootstrap, Operation replay, History restore, and resource
   dependency collection.
3. Add Scene tests proving label changes invalidate content/frame state while outer
   bounds, hit testing, selection identity, and Shape leaf hierarchy remain unchanged.
4. Add Text Runtime tests for entering a Shape label session, formatting, composition,
   cancel, commit, undo/redo, and write rejection after delete/lock/lost/close.
5. Add Render tests for rectangle, ellipse, and triangle clipping and for preview versus
   canonical label placement/style/font identity.
6. Add Facade tests for the read-only font catalog and per-resource status, including
   rejected publish/bind no-side-effect behavior.

## Phase 2: Semantic model and wire contract

1. Extend `semantic::ShapeContent` with `std::optional<RichTextContent> label` while
   preserving aggregate construction sites through an explicit compatible constructor
   or updated fixtures.
2. Add the Shape v3 capability identifier and update the frozen registry/validation
   source used by this branch without changing existing IDs or versions.
3. Add an optional `label` field to protobuf `ShapeContent` using a new field number;
   regenerate through the repository's locked Semantic SDK path.
4. Update protobuf mapping, structural validation, operation-state validation,
   snapshot/bootstrap, canonical encoding, digest, and resource dependency traversal.
5. Preserve absent versus explicit-empty label presence and reject v3 labels without the
   required capability or with invalid RichText/layout/resource references.

## Phase 3: Runtime text targeting and font catalog

1. Introduce a text target descriptor that can address an ordinary RichText object or a
   Shape label field. Keep `TextEditSession` as the sole editing state owner.
2. Route label reads/writes through the existing owner sequence and existing text delta
   and history machinery. Do not create a hidden object or host-side text state.
3. Add public read-only `FontCatalogSnapshot` and resource status types to the foundation
   runtime headers, then implement them in `CanvasRuntimeFacade` from the admitted
   manifest/resource state.
4. Ensure catalog entries expose resource ID, family, revision, content hash, byte
   length, state, and diagnostics. Rejected resource operations leave the previous
   catalog and document state unchanged.
5. Expose label selection/edit geometry through the same text geometry read path.

## Phase 4: Scene, reference, and render integration

1. Project Shape label metadata into Scene records without creating a child node.
2. Add label layout/resource digest to Scene invalidation and dependency tracking while
   keeping Shape bounds and hit/selection geometry unchanged.
3. Extend DirectReferenceSource/ReferenceDrawList to carry an optional label payload.
4. Render labels with the shared `RichTextLayoutService`, local `[0,width]×[0,height]`
   layout, and shape-specific clipping in canonical Skia and headless paths.
5. Reuse the same label payload and clip contract for preview rendering.

## Phase 5: Facade/Debug UI integration

1. Replace any direct resource/font container reads in Debug UI with the new Facade
   catalog methods.
2. Keep all mutations on the existing TextFormatPatch/TextIntent path.
3. Add Shape label entry/edit affordances through the existing text tool and ensure the
   selected Shape remains the selection target.
4. Verify the catalog drives font choices and reports failed/missing resources without
   exposing semantic internal headers.

## Phase 6: Verification and delivery

1. Run `git diff --check`, focused semantic/scene/text/composition/render tests, and the
   existing FC-02 Python tests.
2. Configure a clean build using `setup_build_environment.py` facts and the locked Skia
   SDK; run strict Clang/MSVC builds with warnings as errors.
3. Run the full relevant CTest target list and compare legacy Shape v1/v2 evidence.
4. Record changed files, exact source revision, test commands/results, and remaining
   physical validation items. Do not alter SDK locks, historical Gates, Authority, or
   PHYS state.

## Commit boundaries

- Commit 1: RED fixtures/tests and semantic/wire contract.
- Commit 2: Runtime text target and Facade catalog.
- Commit 3: Scene/render/Debug UI integration and regression fixes.
- Commit 4: verification evidence only if repository convention requires it.

## Completion criteria

The implementation is ready for review when all 12 design verification items pass,
legacy Shape behavior remains byte/digest compatible, ordinary RichText behavior is
unchanged, and the new Facade/catalog and Shape-label paths are exercised through the
same Runtime owner and frame lifecycle.
