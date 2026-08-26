# Authority Reconciliation — 2026-08-26

## Purpose

Reconcile the historical GitHub Notion mirror with the Current Notion authority without merging stale implementation/materialization state into current `main`.

## Branch/base

- Repository: `Mostorm-Labs/axiom`
- Reconciliation branch: `docs/authority-reconciliation-20260826`
- Base: `main @ d857bd342fd63b922a048f5348b0be6864196ad8`
- Historical source branch: `docs/notion-bridge-bootstrap @ 0304539df219cb73909d5b3e6602d5308733365a`

The historical branch was already far behind current main and contained docs plus schema/verification/workflow materialization. It was therefore not merged or whole-commit cherry-picked.

## Controlled import boundary

Only the historical `docs/notion/**` subtree was imported as a reconciliation baseline. Historical `schema/**`, `verification/**`, `.github/workflows/**`, runtime and other code were intentionally excluded.

The imported files are not automatically current merely because they exist in the branch. `docs/notion/manifest.yaml` v2 is the current machine index.

## Reconciliation findings

### UPDATE — Architecture baseline

Historical mirror exposed Axiom Architecture Baseline v0.3 as frozen/current. Notion Current Architecture Authority is v0.4. v0.4 is now published and the manifest current graph points only to v0.4. The v0.3 file remains provenance only.

### MISSING -> PUBLISHED — First-Batch Implementation Contract

Historical GitHub mirror did not contain the current Implementation Contract Freeze child set. Current exact source set is now published:

```text
ICF-00 v0.3
ICF-01 v0.1
ICF-02 v0.2
ICF-03 v0.2
ICF-04 v0.2
ICF-05 v0.1
```

Current gate remains `Source Contract Closure Ready -> Repo Rematerialization / Reverification Pending` and the set remains Proposed Freeze Candidate, not Frozen v1.

### MACHINE-SOURCE RECONCILIATION

ICF-00 v0.3 is the machine IDL/profile/projection authority. It closes historical ambiguity by requiring concrete ordering paths (`input.document`, `input.canvas`, `input.binding`), exact current-only source closure, explicit standalone/embedded/reuse projection routing, generated-binding vs ergonomic-wrapper separation and five independent fingerprints.

Where a child page retains an older illustrative type-name-only `ordered_by` excerpt, ICF-00 v0.3 concrete `input.*` syntax is normative for current codegen.

### REPRESENTATION CLOSURE

ICF-02 v0.2 freezes current cross-language mappings:

```text
SurfaceVisibility: VISIBLE=1 HIDDEN=2 OCCLUDED=3; 0 invalid/reserved
PresentationState: RUNNING=1 SUSPENDED=2; 0 invalid/reserved
DeviceLossReason: u32 diagnostic; 0 UNKNOWN; nonzero common taxonomy not frozen
```

This is intended to preserve existing ABI shape while removing generated-code ambiguity.

### BEHAVIORAL CORRECTIONS WITHOUT ABI LAYOUT CHANGE

ICF-03 v0.2 requires stale BindingHandle isolation and binding-owner resolution before detach mutation.

ICF-04 v0.2 requires CanonicalVisible to validate both frame-vs-feedback generations and frame-vs-live Surface/MetricsGeneration plus live visibility == Visible and eligible coverage. Approximate evidence cannot clear preview.

These corrections mean an old ABI-layout PASS cannot prove current behavioral correctness.

### DERIVED / VERIFICATION CLASSIFICATION

Implementation Contract Skeleton v0.2 is a **Derived Materialization Specification / Repo Blueprint**, not verification evidence.

Contract Linter / Golden Matrix v0.2 is a **Verification Specification**, not current proof.

Historical Skeleton v0.1 and its compile/ABI review remain historical/stale evidence only.

## G0 / G1 impact

No reopening of G0 or G1 is required by this reconciliation:

- G0 remains the verification substrate and can host additional current contract suites.
- G1 is the Semantic Kernel scope and does not consume the Platform/Binding/Presented implementation contract changes being rematerialized here.
- Active G1 implementation remains isolated on its own branch and is not modified by this documentation reconciliation.

## Current Codex consumption rule

Codex MUST begin with `docs/notion/manifest.yaml`, consume only current documents listed for the target scope, treat historical/superseded/stale files as non-normative and report an authority gap instead of deriving missing semantics from historical material.

## What this branch does not prove

This branch does not claim current contract materialization has been generated or verified. It does not run/approve:

- current IDL generator output;
- source-closure/projection linter evidence;
- current ABI sizeof/offsetof/function-table manifests;
- generated TypeScript snapshot;
- behavioral vectors;
- reproducibility gate;
- WASM/JSI target binding evidence.

Those belong to the later `codex/icf-rematerialization-v0.2` implementation branch after this authority reconciliation is merged to main.

## Exit criteria for this reconciliation PR

- manifest resolves current architecture + exact ICF source set only;
- no current machine rule requires Superseded authority;
- current Architecture v0.4 and ICF/Skeleton/Golden Matrix snapshots exist in repo;
- README clearly defines current-only Codex start rule;
- historical evidence cannot be interpreted as current PASS;
- diff contains no runtime/schema/verification/workflow backfill from the historical branch.
