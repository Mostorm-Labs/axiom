# GT-G1-04-B B5 B-AUTH-02 Fixture Revalidation P31 Addendum

## Scope

This addendum authorizes a docs mirror and a test-only repair of historical B5
DeleteClosure fixtures after Current Authority B-AUTH-02 was approved. No B5
production algorithm change is authorized or required. The P35 classification
is `TEST_DEFECT` for fixture drift and `EVIDENCE_GAP` for the current
source-bound evidence; no B5 implementation defect has been established.

Current Authority: Notion source `3cb4c57a-590c-815a-b8fa-cd785a837da7`, with
approval token `APPROVE_B_AUTH_02_HIERARCHY_CAPABILITY_V1_OPTION_B`.
The repository authority document is only a mirror of that Notion authority.

## Frozen hierarchy capability

Root accepts every released V1 kind. A Group accepts every released V1 kind
and zero or more direct children. A Sticky accepts only RichText and at most
one direct RichText child (including zero children). All other kinds are
non-parenting. No child-role metadata, ordering heuristic, schema/protobuf
field, second hierarchy, or exactly-one Sticky rule may be added.

The former B5 assumption that Connector may parent a hierarchy child, and the
associated Connector-to-child multiwave fixture, is superseded. Re-express the
fixture with `Group G0 -> Group G1 -> Shape S`, with Connector C1 referencing
G1 and Connector C2 referencing S. Deleting G0 must produce hierarchy closure
G1,S and connector cascade closure C1,C2 in two discovery waves plus the
terminal empty wave.

## Authorized test-only work

Modify only `runtime/semantic/tests/delete_closure_test.cpp`. Add an
independent test-only capability checker implementing exactly the frozen rules;
it must not call delete-closure production logic or depend on a future
capability validator. Use it to prove all repaired fixtures contain no invalid
Shape/Connector/non-container parent edge. Preserve deterministic requested
IDs, hierarchy and Connector cascades, reason partitioning, staged overlay
visibility, failure atomicity, no mutation, Reference/Indexed parity, and
runtime trace observability.

Do not modify `delete_closure.cpp`, its internal/header APIs, ObjectStore,
ObjectIndex, B3/B4 production, schema/protobuf/wire, B6+, PreparedApplyPlan,
OperationEngine, GT-G1-04-C, or GT-G1-05.

## Evidence and lifecycle

Run a genuine RED using the independent checker against an unchanged historical
fixture, then replace invalid fixtures and run the focused B5, regression, full
semantic, runtime-boundary, docs, and diff checks. Commit the test repair
separately from this docs commit, then bind a new source-bound B-DELETE.json in
an independent evidence commit. Push all commits normally. The final status is
`READY_FOR_INDEPENDENT_P34_REREVIEW`; only independent P34 review may restore
downstream acceptance. B6 and later work remain untouched.
