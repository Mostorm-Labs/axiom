# FC-08 product P31 — implementation context

## Package and repository position

- Repository: `Mostorm-Labs/axiom` (repository ID `1306396623`).
- Product P31 package: `.aegis/packages/FC-08-PRODUCT-P31-v0.1/package.json`.
- Materialized package branch: `refs/heads/control/fc08-product-p31-package`.
- Trusted task anchor: `a1e8819abe245ab7487f18771970721f480ff487` (`relation: ancestor`).
- Reconciled preparation subject: `427580ae13b60a39e88959b282cbffc695c9aabb`, a descendant of the anchor. The product baseline is not qualified merely because M01-M04 completed.
- This package is pending independent control review. It does not authorize P32 by itself; no product code has been changed in this P31 package commit.

## Current flow and ownership

Use the existing runtime owners; do not create a parallel container runtime or a second membership truth:

`Product Shell / Debug UI / ordinary consumer`
→ typed public `RuntimeFacade` intent and immutable readback
→ `CanvasRuntimeFacade / RuntimeComposition` owner admission and ordered ingress
→ Interaction planner/session plus existing `EditorHistory` and real E01 command admission
→ Semantic `OperationEngine` validation, staged graph and atomic apply
→ Scene coherent publication, derived geometry/query/render/damage
→ Windows/Web host presentation of receipt/readback.

The owner map and exact source anchors are frozen by P14-P17 and ARC-CR-C01/C02 in `authority.lock.json`. The accepted wire and semantic meaning are frozen by the accepted P12/P13 exports and the P23 D01-D08 semantic authority. If implementation finds an unresolved meaning, stop and route to the owning Authority/Verification layer; do not infer a new semantic rule.

## Known incomplete boundary

- The accepted current descriptor diff is source-complete and additive, but the production source tree still lacks the accepted `section.proto` and `container_operations.proto` files and the runtime implementation for Section/1 and opcodes 17/18.
- Existing baseline observations explicitly classify representative product positives as `CAPABILITY_ABSENT`. These are valid RED observations, not product PASS.
- The qualification-only `Q-SEM-*` / `Q-HIST-*` hooks bind existing baseline stages only. They do not implement or prove future F/U admission or H06.
- The accepted materialization has all 60 logical cases, 302 unique variants, independent reference helpers, exact inputs/expected refs and real baseline profile bindings. Do not modify the corpus, expected values, P20 CR01 or historical failure records.

## First action after P31 package acceptance

1. Resolve the exact same-repository package materialization ref and verify the anchor is an ancestor of the accepted starting revision.
2. Run `PackageBindingPreflight` and `EvidenceContractPreflight` against this exact package. Do not use floating labels or infer permission from a prior readiness snapshot.
3. Complete `ImplementationDesignPreflight` for the semantic staged apply, real F/U + History reservation, owner-bound Composition admission, Scene publication/recovery, preview handoff and Windows/Web adapter flow.
4. Run `FC08-RED-01` against the unmodified production subject using the frozen case/variant IDs before production mutation. If the old implementation passes a required positive while the capability is absent, return `VERIFICATION_DESIGN_DEFECT`; do not weaken the oracle.
5. Implement evidence-gated vertical slices and continue through every frozen E01-E10 requirement until success or an explicit terminal blocker.

## Non-negotiable boundaries

- Preserve the historical `descriptor.lock.pb` and `descriptor.lock.sha256`; only the current descriptor pair may be updated for the accepted additive publication.
- Keep Section identity `kind=10/version=1/content tag=12`, opcodes 17/18, the three mutually exclusive topology effects, CAP-01..06 and fail-closed snapshot/journal capability behavior.
- Real F/U and History resources must be reserved before canonical publication. Rejected precommit operations must not mutate canonical state or History cursor/redo.
- Keep INT-01 under FC04/FC10 ownership and explicitly reject mixed-default-delete before write. PH01-PH04 remain FC13 physical debt.
- Do not expand into a generic FC04 inspector, advanced FC06/FC07 feature completion, FC10 clipboard, Frame, arbitrary transaction, or mock production dependencies.
- Do not modify `.aegis/authorities.json`, Gate/Evidence/Integration registries, the accepted catalog/corpus, prior materialization results, or qualification hook semantics.
