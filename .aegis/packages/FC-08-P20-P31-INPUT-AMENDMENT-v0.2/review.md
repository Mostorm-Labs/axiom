# FC-08 P20/P31 input and profile amendment — candidate 02

Status: **PROPOSED_PENDING_ACCEPTANCE**. This is a local, versioned verification
input proposal, not an accepted P31 lock or product P32 authorization.

## Requested control decisions

Accept or return changes on both the source-of-truth proposal and the exact
profile applicability matrix bound by `package.json`. Neither audit success nor
this package's existence records authority acceptance. The supplied Blocker
Closure handoff requires that acceptance before downstream revalidation.

## Source truth and correction

The ShapeKind Registry declares 1=RECTANGLE and 2=ELLIPSE. The materializer's
`selection_variant` explicitly authors `kind_geometry.b=ellipse`; its generic
`objects`/`record(Shape)` helper drops that specialization and emits kind1.
This explains the conflict, without presuming canonical or reference precedence.

Proposed correction is case-specific:

| Source decision | Variants | Proposed input correction | Independent basis |
| --- | ---: | --- | --- |
| Shared authored ellipse | 13 | Canonical b.shape_kind 1→2; leave reference geometry and all other fields unchanged | Explicit shared authoring and S02 comment; ellipse bbox[40,40,10,10], closest lasso point[41,41] has normalized distance squared32/25>1. A rectangle would invalidate the intended AABB-only counterexample. |
| S04 original literal seed | 1 | Rectangles A[9,9,2,2], B[49,49,2,2] in graph and records; b reference geometry becomes rectangle | Catalog explicitly names concave-region.json. Generator imported its polygon but discarded its literal child rectangles. B lies strictly in the notch; all Section corners inside is insufficient. |
| S05 empty-Group graph | 2 | Replace stale graph S/a/b with authored empty G, matching unchanged canonical records | Catalog requires empty Group. Generator reassigns local graph but retains the old f.graph dictionary. Independent candidate01 audit detects the mismatch. |

All **302 expected files/bindings and old oracle bytes remain unchanged**. The
old corpus, catalog, reviewed obligations, CR01 and historical results remain
unchanged. Sixteen new input files live in attempt
`v0.3-control-remediation-02`; all other input/wire references reuse old files.
The exact field changes and old/new SHA-256 values are in the new change ledger.
No SUT output or production encoder was used to select a truth or expected value.

Candidate01 is retained as an **unaccepted, superseded proposal**, with a failed
audit. It repaired the original14 discrepancies but missed the two empty-Group
metadata variants. It is not an accepted alternative or a failed product run.

## Profile binding

The matrix contains each frozen ID exactly once: **302 variants, 60 logical
cases, all E01–E10 families, 906 explicit profile cells**. Each cell binds its
actual frozen input, predicates, accepted case/method and existing owner source.
No profile is inferred merely from a case prefix.

| Profile | Applicable | Not applicable | Execution state |
| --- | ---: | ---: | --- |
| windows-native | 302 | 0 | NOT_RUN; environment not assessed |
| web-wasm | 302 | 0 | NOT_RUN; environment not assessed |
| headless | 148 | 154 | Applicable entries NOT_RUN; excluded entries explicitly NOT_APPLICABLE |

Each profile includes three shared controls (two W01 host descriptor variants and
G05 quality-at). They are explicitly **not per-platform product executions**:
W01 reuses the exact host descriptor proof under VR-C04; quality-at is the frozen
oracle control, not a product positive. All other Web product observations require
actual compiled WASM in Chrome, never Node-only/mock adapters.

Semantic-only headless includes raw codec/semantic topology, explicit inverse
replay, snapshot/bootstrap/journal and literal common geometry observations. It
does not impersonate EditorHistory, gestures, selection scopes, frame/preview or
resource publication. H04 raw sublimits are included only where the complete
frozen predicate is size-only; operation recipes with command_admitted are
excluded. G03 literal composition differs from its real pose-command variants.
The complete per-ID rationales and source owner bindings are authoritative for
this proposal; this paragraph is only a summary.

Applicability and execution are separate axes. APPLICABLE+NOT_RUN means no test
has run. APPLICABLE+BLOCKED_ENVIRONMENT requires a recorded missing environment;
it is never NOT_APPLICABLE or PASS. This preparation does not claim any new
Windows, Chrome or headless environment qualification.

## Audit and continuation

`tools/fc08/audit_input_amendment.py` independently validates hashes, exact
inventory, schema, accepted-method bindings, source owner bytes at the frozen
production subject, selection graph/record agreement and literal expectation
identities, plus rational geometry witnesses and preservation of all prior
tracked files. It does not import the generator or product implementation.

The audit and its negative-control unit tests are **verification tooling checks,
not Axiom product tests**. They cannot establish product RED or close the P31 gap.

After explicit acceptance: rerun corpus audits; materialize the exact revised P31
bindings without silently rewriting f67; run PackageBindingPreflight and
EvidenceContractPreflight at that same exact revision; run ImplementationDesign
Preflight; establish actual product runner/tests; then execute FC08-RED-01 against
unmodified427580ae. Missing decisions, environments or seams remain blockers.
No product runtime/schema/global registry was changed and main is not merged.
