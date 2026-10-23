# FC05 P20 freeze readiness

Status: **REVIEW_INPUTS_READY / P20_NOT_FROZEN / P31_NOT_ISSUED**.

Role: P20 readiness support, not a Gate verdict. Authority: accepted user scope and
the explicitly pending FC05 successors. Objective: prepare a finite verification
target, not make implementation invent it. Non-goals: P32, independent approval,
historical evidence edits. Quality gate: do not freeze unaccepted wire, atomicity or
routing truth. Handoff: independent successor review, then final P20/P31.

## Ready inputs

- Shape/2 and Shape/3 compatibility math: 337 exact-rational checks pass in
  `docs/planning/fc05/evidence/shape-compatibility/math-results.json`.
- User scope: straight plus automatically avoided orthogonal Connector routes,
  and Sticky with RichText, move/resize, History and connectability to all
  released Shape versions, Image and Sticky.
- FC03 is on `origin/main` through PR #151; FC05 remains on its own stacked line.
- Existing semantic, Scene, TextLayout and hierarchy seams were inspected against
  the proposed obligations.

## Materialized successor proposals

1. [Sticky mutation](FC05_STICKY_MUTATION_CONTRACT.md): proposed restricted
   EditSticky/tag17, aggregate guard, one PreparedApplyPlan, child restore and History.
   Defaults: 200×200, 12 padding, #FFF59D, Body16 with explicit admitted font.
2. [Routing](FC05_ROUTING_CONTRACT.md): conservative obstacles, clearance, cardinal
   escape, finite domains, explicit failures, query/search/quantum/arena limits.
   Timing remains corroborative; work bounds and responsive cancel are correctness.
3. [Capability successor](FC05_CAPABILITY_SUCCESSOR_REVIEW.md): exact vectors,
   typed false-default support and complete-interval capability preflight. The
   historical unsorted mixed schema3 recovery issue remains explicitly open.
4. [Public consumer](FC05_PUBLIC_CONSUMER_CONTRACT.md): proposed typed actions,
   receipts/readback and Windows/Web obligations. No mutable runtime exposure.

The user's overflow behavior is confirmed; these exact implementation/semantic
choices remain author-prepared review candidates. Do not claim independent review,
Current Authority or production readiness from document existence.

## P20 obligations once decisions are accepted

The existing draft obligations CON-01..CON-08 and STK-01..STK-03 remain the
starting matrix. Add explicit blocking oracles for:

- Sticky parent/child atomic commit failure injection and identity-preserving
  Undo/Redo;
- all successor capability vectors and missing-composite reader rejection;
- no-route/budget diagnostics with zero canonical side effects;
- third-obstacle corridor invalidation and shared route render/bounds/hit;
- ordinary RuntimeFacade consumer versus Debug UI parity;
- native/WASM canonical equality and route topology/tolerance comparison.

Accept/reject these concrete successors in independent design review, resolve the
historical capability-order policy, then freeze exact wire/typed fixtures and
commands and bind their hashes in P31. Candidate test names in the P20 draft are
deliverables, not currently existing tests. No product implementation starts here.

The available review connector reads CI diagnostics; it does not invoke an
independent reviewer. No independent reviewer has been invoked for this iteration.
An author-side consistency check cannot replace that review or promote `.aegis`
Current/Gate state. Existing control-manifest validation is only VALID/STATE_OK
for the pre-existing project records, not an FC05 verdict.

`codex/fc05-architecture-p20` descends from design `d6a47d53`. A later accepted
P31 materialization commit becomes the exact P32 branch base; do not merge,
force-push, or silently rebase these candidates onto a moving main/FC03 line.

