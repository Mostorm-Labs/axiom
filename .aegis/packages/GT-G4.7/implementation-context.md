# GT-G4.7 — RichText Rendering Foundation

## Execution identity
- repository: `Mostorm-Labs/axiom`
- execution branch: `codex/g4-7-development`
- trusted task anchor: `68b925e53b092657f3415c3d3992a451a8515598`
- required relation: ancestor
- terminal implementation state: `READY_FOR_COMBINED_G4_PHYSICAL_QUALIFICATION`

## Why this package exists
The old roadmap required formal G4.6 Gate PASS before G4.7. The accepted execution-order amendment now permits G4.7 P32 from the exact post-G4.6 automated/evidence boundary while all G4.6 physical obligations remain open for a later combined physical qualification.

## Confirmed repository reality at anchor
- canonical RichText schema: `schema/axiom/v1/proto/auditoryworks/axiom/v1/rich_text.proto`
- current `BoundsSystem` still computes heuristic RichText width/height from string bytes/font size and must be replaced by production layout-derived bounds
- RuntimeScene / DirectReferenceSource already preserve RichText semantics in the reference path
- locked `r1-full-skia-sdk.lock.json` profile `r1-full-v1` exposes richtext, HarfBuzz and ICU capability
- `deps.lock.json` pins Roboto Regular and Noto Sans CJK subset resources
- POC-04 under `pocs/rich_text/**` is reference implementation/test material only; reuse lessons/fixtures, do not promote its G6 TextEditSession/IME ownership into production G4.7

## First incomplete action
Run PackageBindingPreflight/EvidenceContractPreflight, then inspect the anchor descendant for exact production module seams and execute G47-A. Ordinary file/API uncertainty must be resolved from the repo; only true Authority/scope/verification conflicts may block.

## Continuous execution
Do not stop after G47-A/B/C intermediate success. Continue through G47-D automated qualification-artifact readiness in the same P32 unless an explicit frozen terminal blocker occurs.

## Physical boundary
Do not wait for user-operated physical verification. After code, automated verification and qualification artifacts are complete, return `READY_FOR_COMBINED_G4_PHYSICAL_QUALIFICATION`. The later combined phase owns the remaining G4.6 physical obligations plus G4.7 text/font/zoom/DPR physical checks.
