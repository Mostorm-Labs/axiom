# FC-00-LEGACY-IMPORT - P31 Implementation Context

## First read

Use package.json first, then this file and execution-contract.json. Resolve the repository identity `github:Mostorm-Labs/axiom` and the exact P31 materialization commit in THIS repository before inspecting the task anchor. The immutable historical source revision is `6fc1508f1a971abc5a90e068d0cc5f6ae82c064b`; the Aegis v0.6 tool revision is `00d3690ca2cdde15b3f919c54ccc8ef6290f55ba`.

## Current reality

Axiom's six root `.aegis` JSON manifests at the pinned source have a legacy pseudo-v0.6 shape. Official Aegis v0.6 requires a nested project object, independent Gate Contracts/Decisions, evidence.type, and decision IDs formatted as `<gate-id>::decision::0001`. The legacy authored state has 27 Gate rows: 25 formal PASS/PASS_WITH_FINDINGS and 2 READY_FOR_P31; 29 Bound Integrations (27 integrated, 2 closed_unmerged); 34 Evidence records; four historic Authorities. The legacy `state.json` is manually generated and stale, not canonical.

## Bound implementation flow

1. Preflight: verify exact repository/anchor/P31 package and all six SHA-256 lock refs, P23 Current, P20 obligations and expected-crosswalk.json against original source blob SHAs.
2. Record an ImplementationDesignPreflight: original read->legacy parsing->normalization->official Aegis validation->computed publication boundary; list proposed changes, abort path, independently verified oracles; unresolved semantic decisions must be empty.
3. Run RED: official Aegis v0.6 `validate` on untouched legacy source must report INVALID and exit 2. If it reports valid, stop as verification design conflict; do not manufacture a RED.
4. Preserve all original legacy files as immutable source revision with exact Git blob SHA provenance (lossless archive of all six is allowed). Do not modify source anchor or historic Gate results. Keep the two complete readiness raw records available in a read-only legacy audit.
5. Implement only a deterministic first-import authored-manifest converter, using the P23-frozen stage and decision mapping. Do not reimplement official state computation; the output state must come from pinned Aegis v0.6.
6. Independently compare old/new source facts against frozen expected-crosswalk.json. Include a wrong-but-valid PASS binding and three other frozen negative controls. Do not use importer code to derive oracle expected values.
7. Run official v0.6 `validate`, `recompute --write`, `check`, stable repeated recompute/check, and strict source/scope diff review.
8. Materialize all blocking evidence on a remotely reviewer-resolvable Axiom ref and return exact revision/evidence identity, not a P34 verdict.

## Important owner and lifecycle constraints

- First import has exactly four historical Authorities. A repo-local FC Track Current Authority is a DIFFERENT accepted post-P34 append-only v0.6 change, outside this task.
- Legacy `g2-final-convergence` stage remains `G2_FINAL_CONVERGENCE_REVIEW`; 24 G3/G4 formal Gates use `P34`. Never invent a new Gate PASS.
- Old superseded G4 authority remains provenance for original G4-01..11. G4-12 and G4 completion use v0.3.
- Original G4 Completion is functional PASS_WITH_FINDINGS; physical/pressure remains deferred. PX0-07/G4.6-C/G4.7 physical Gate blockers remain.
- Do not edit Runtime/ARC/Skia/Brush/Shape/RichText/Product Shell or existing legacy Gate/Result files.

## First incomplete action

Perform repository, package, lock, and evidence-contract preflights against the exact P31 materialization ref. Continue through the frozen closure contract until either the full implementation/evidence return boundary is satisfied or an explicit terminal blocker exists.
