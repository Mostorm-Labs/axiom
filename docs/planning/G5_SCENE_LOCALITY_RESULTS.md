# Scene locality slice results

This result records the bounded Scene locality implementation on the
`codex/scene-locality` branch, based on `bacb93cf31a7aebe4fe9055b95f230ba304e7799`.
It is an engineering result only; it does not issue G5 or P34 acceptance.

## Implemented

- Flat, independent leaf edits use the existing RuntimeScene evaluator only for
  changed records and feed the prepared projection into the production editor
  Scene delta path.
- Generation-only ChangeSets advance the RuntimeScene stamp without rescanning
  the semantic store.
- Hierarchy, Group, Section, Connector, Table, resource manifest, document
  background, text/layout, image resources, texture-backed materials and brush
  snapshot resources return `kRequiresFullRebuild`; the coordinator performs one
  authoritative full recovery.
- Existing publication gates, before images, order handling, A6 rollback and
  full-authority projection are preserved.
- Scene delta publication reuses the already-published ordered snapshot. Pure
  content/transform updates avoid a second ordered-cache materialization and
  sort; insert/delete/order changes still perform the required publication sort.

## Evidence

- `G5ProjectionWorkObservation`: 1,000 and 10,000 flat objects, one changed
  object, `allObjects()` calls `0`, copied objects `0`.
- `G5SceneChangeDifferential`, `G5FullAuthorityChangeDifferential`,
  `G5DuplicateScenePublication`, `G5EditorSceneLocality`, and
  `G5EmptySceneDelta`: passed.
- `canvas_g2_a6_incremental_equivalence`,
  `canvas_g2_a6_atomic_publication`, and `canvas_g2_a6_recovery`: passed.
- Ink v2 runtime, compact codec, vector quality, Render damage/cache, and all
  material regression modes run on the same worktree: passed.
- Runtime boundary checker: 116 contract files passed.

`canvas_g2_a3_full_incremental_equivalence` was initially blocked by stale Image
and hierarchy expected rows. The fixture now matches the current FC03 Image
digest (`content_mode`/`source_rect`) and current world-space bounds authority;
the test passes. This fixture-only repair does not change semantic or descriptor
authority.

## Remaining cost and boundary

The Scene publication and ordered-cache layers still materialize/copy contiguous
record vectors for some reads and mutations. This slice removes unnecessary
semantic-store scans and per-object geometry/layout reevaluation; it does not
claim constant-time publication or eliminate the final published-snapshot copy,
persistent/chunked Scene storage, GeometryChunk,
Tile, Scheduler, or ResourceBudget completion.

No SDK/API/ABI, semantic schema, BrushEngine, Skia lock, or platform contract was
changed. No official G5/P34 PASS is claimed.
