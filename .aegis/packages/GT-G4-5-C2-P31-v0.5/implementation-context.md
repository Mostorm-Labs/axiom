# GT-G4-5-C2 P31 v0.5 implementation context

This is the repaired P31 package. v0.4 was not executable because its authority
lock was stale and its closure contract simultaneously forbade and required the
approved brush schema release. v0.5 resolves that boundary: P32 may materialize
only the exact v0.4 brush_engine.proto/registry carriers, with ObjectKind (5,2),
content tag 10, AddStroke payload v2 and snapshot v2; all unknown or unapproved
versions remain rejected.

Execution order is: repository/package binding -> implementation design
preflight -> RED oracle -> package/resolve -> plan -> R1 VectorPathNode ->
session/preview/commit intent -> schema/codec/operation/replay -> renderer and
platform ownership -> legacy read-only and regression evidence. Runtime Core owns
AddStroke construction; the Brush Engine never constructs or mutates an Operation.

The package is ready for control review and P32 re-entry review. It does not
claim P32 authorization or Gate PASS.
