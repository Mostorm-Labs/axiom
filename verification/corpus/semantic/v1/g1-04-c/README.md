# GT-G1-04-C Semantic Conformance Trust Boundary

GT-G1-04-C is pre-apply only. Its semantic scope ends at `PreparedApplyPlan`:

`Operation -> Normalize/Canonicalize -> Stateless Validation -> OperationId Idempotency -> Stateful Validation -> PreparedApplyPlan`

Atomic Apply, SemanticGeneration, ChangeSet, canonical commit records, local publication/no-echo, post-apply state, and final applied-state oracles are out of scope.

## Trust roles

The trust path is strictly one-way:

`Current Semantic Authority -> AUTHORITY_MANUAL -> DERIVED_GENERATED -> IMPLEMENTATION_OBSERVATION -> CONFORMANCE_RESULT -> GATE_EVIDENCE`

These five provenance roles are disjoint. Generated fixtures, production output, Reference provider output, Indexed provider output, and existing B test results are never expected truth.

The future single human-reviewed authoring root is exactly:

- `authoring/cases.json`
- `authoring/expected.json`

C0 does not create those files. Production or implementation observations must never write or update them. No bless, update-golden, accept-current-output, capture-to-expected, or equivalent mechanism is permitted.

Every manual expected record carries Current Authority references and `mutationExpected=false`. Later observations must prove that canonical before/after projections are equal.

Connector target delete and `geometry_point_like_elements_per_operation_aggregate` are CLOSED Current Authority decisions; stale OPEN treatment is invalid. B tests/results may be coverage references only, never oracle sources.

These schemas and helper contracts are verification-only. They do not define Product ABI, production wire format, or production semantic error identifiers.
