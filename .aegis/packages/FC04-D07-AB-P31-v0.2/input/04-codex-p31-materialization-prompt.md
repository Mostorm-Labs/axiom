# Codex - Materialize FC-04 D07 P31 Scene Scope Correction v0.2

Repository: Mostorm-Labs/axiom
Worktree: C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas
Branch: codex/fc04-d07-selection-audit
Reported current result: 15e76458f3872be4d1b51d4aac05eb8402719014
Feature baseline: c7f874395d55952e9d5288e2a336a21747f37f11

## Objective
Materialize a new versioned P31 package correcting the Scene-owner scope omission. This task is package-only. Do not modify product source or implement the Scene seam.

## Preflight
1. Verify repository identity, actual branch/HEAD, clean/dirty state and ancestry from the feature anchor, previous task anchor and current P32 result.
2. Read the exact P32 report and commands.json at 15e76458f3872be4d1b51d4aac05eb8402719014.
3. Preserve the OOM fix, 11 passing D07 checks, 160 Selection regressions and all previous results/hashes as immutable history.
4. Confirm accepted D07 scope and current 07-14 binding. Do not reopen accepted product scope; do not close D06-B.
5. Inspect the existing P31 package and preflight scripts. Previous preflight PASS is historical only.

## Required package change
Create a new immutable package directory, suggested .aegis/packages/FC04-D07-AB-P31-v0.2/. Do not overwrite v0.1 or historical correction packages. Bind task_anchor to the exact verified current result and retain the feature anchor. Rebuild content-addressed source/Authority/Verification/evidence bindings and update design, closure, implementation context, test matrix, evidence contract, source lock and terminal blockers.

Authorize only the primary Scene-owner files and focused tests in 01-scope-correction.md. Conditional participant-owner files require a concrete design-preflight finding and must be added to the source lock before any mutation. If an unlisted owner is required, stop and return to P31.

Bind the lifecycle and semantics from 02-scene-seam-design.md and add SCENE-PREP-01 through SCENE-PREP-06 plus D07 grouped-intent and OOM obligations from 03-verification-matrix-delta.json. Planned tests are NOT_RUN until P32 executes them.

## Required checks
Run PackageBindingPreflight, EvidenceContractPreflight, DesignPreflight and D07 package preflight against the new package. Record exact commands, exit codes, raw outputs, hashes and reviewer-accessible result paths.

## Strict boundary
No product source edits, no tests added/changed in this task, no branch reset, no merge to main, no D06-B implementation, no global Transaction, no public ABI/schema/wire changes. Do not rewrite historical evidence.

## Terminal return
Return new package version/path, task anchor, source/package hashes, exact preflight commands/results and report links. If a binding is missing or the Scene publication/07-14 obligations conflict, stop with the precise blocker. Do not claim D07-A/B PASS.
