# Selection Batch 2 — Implementation Plan and Verification

## Entry gate: reconcile Batch 1

Before changes:
1. Record repository, branch, exact HEAD, working-tree status, and first-batch PR/commit if available.
2. Compare the actual HEAD against the first-batch work package and identify which first-batch contracts are already implemented.
3. Run the focused existing selection/interaction/scene/Section tests to establish a baseline.
4. If required first-batch primitives (area session, shared region geometry path, Shift/add policy, or safe gesture cancellation) are absent and Batch 2 cannot be implemented without inventing them, stop with `BLOCKED_UPSTREAM_BATCH1_INCOMPLETE` and list the exact missing prerequisite. Do not silently absorb all Batch 1 into Batch 2.

## Work slice A — hierarchy reduction

- Add or refine a pure helper that reduces a selected set to deterministic operation roots.
- Rules: remove duplicates; when a selected ancestor semantically owns/covers a descendant under current Group/container policy, omit descendant from operation roots; retain unrelated objects; honor active Group scope; preserve stable ordering and a deterministic primary policy.
- Do not mutate canonical Placement or hierarchy.
- Add tests for parent + child, nested Group, sibling children, independent objects, deleted/missing ancestors, and active/inactive Group scope.

## Work slice B — Section target resolution

- Inspect actual Scene hit priority and Section model/operation behavior.
- Separate Section boundary/background targeting from selection of objects inside its bounds.
- Ensure a Section hit cannot swallow a more appropriate eligible contained object solely because its bounds overlap.
- Preserve intentional ability to select Section itself on its border/background and preserve Section boundary drag/resize.
- Add tests: click contained object, click Section background, click Section boundary, overlapping sibling object, nested Section/Group if currently supported, locked/hidden child, and pointer cancel.

## Work slice C — delivered-object region coverage

- Use `Scene::query(region)` only for broad-phase candidate collection.
- Implement/reuse precise region-vs-geometry predicates for the object types actually delivered on the branch.
- At minimum cover Rectangle, Ellipse, Triangle, VectorPath, VectorStroke, DabStroke, Connector route/arrowhead, Image crop, RichText/Sticky, Group, Section.
- Where geometry is unavailable or deferred, define and test the explicit conservative fallback (or mark unsupported); never label AABB-only as precise.
- For lasso, use the existing accepted containment/intersection policy from Batch 1. Do not invent a new policy in this task. If policy is not present in current authority/package, stop and report an upstream contract blocker.
- Deduplicate user-facing object identities after Sticky/RichText and Group promotion.

## Work slice D — regressions and closure

- Ensure selection-only gestures do not change semantic generation, operation count, or History canUndo/canRedo state.
- Ensure selection preview and cancel are transient.
- Ensure existing single-click selection, object drag, resize/rotate handles, Section boundary interaction, Connector quick-create, image Crop, viewport pan/zoom remain unchanged.
- Run focused CTests and any relevant Windows/Web selection contract suites available on the branch. Report not-run platform tests explicitly.

## Required test matrix

### Hierarchy
- Parent + descendant reduces to one root.
- Two siblings remain two roots.
- Nested Group selection follows active scope.
- Group scope does not select objects outside the scope.
- Stable/deterministic order across repeated runs.

### Section
- Selecting an object inside a Section works.
- Selecting Section background/border still works according to hit policy.
- Section does not mask an overlapping topmost independent object incorrectly.
- Section boundary drag and cancellation still work.

### Geometry
- Ellipse AABB corner does not count as precise hit.
- Triangle exterior inside AABB does not count as precise hit.
- Lasso concavity/self-intersection follows Batch 1 policy.
- Connector body and arrowhead are selectable; empty AABB region is not.
- Long stroke does not become selected solely because its large bounds intersect if precise policy says otherwise.
- Erased/masked stroke area follows visible geometry where supported.
- Cropped-out image region is not selected as visible image content.
- Sticky/RichText produces one user-facing identity.

### No mutation / regressions
- Selection-only operation leaves semantic generation and operation count unchanged.
- Cancel leaves prior selection intact and creates no history.
- Existing drag/resize/rotate, Section, Connector, Crop, pan/zoom regressions pass.

## Terminal success

Batch 2 is complete only when:
- Hierarchy root reduction and Section targeting tests pass.
- Every currently delivered object kind in the agreed coverage matrix has a named oracle and result (PASS / explicit unsupported / blocked), with no silent gaps.
- Focused test commands, exit codes, and environment are recorded.
- No new ObjectKind, semantic schema, or canonical operation was introduced without separate authority.
- Exact implementation revision and PR are returned. No merge or Gate PASS is implied.

## Terminal blockers

- `BLOCKED_REPOSITORY_IDENTITY`
- `BLOCKED_UPSTREAM_BATCH1_INCOMPLETE`
- `BLOCKED_SELECTION_POLICY_AUTHORITY`
- `BLOCKED_GEOMETRY_ORACLE`
- `BLOCKED_BUILD_OR_PLATFORM_ENVIRONMENT`

Continue through all executable slices until terminal success or one of the blockers above is proven.
