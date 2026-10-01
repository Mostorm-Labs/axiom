# GT-G4-5-C2 P31 v0.9 implementation context

This is the repaired P31 package. v0.4 was not executable because its authority
lock was stale and its closure contract simultaneously forbade and required the
approved brush schema release. v0.8 resolves that boundary: P32 may materialize
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


## v0.7 exact scope repair

The v0.7 package adds the existing common host/controller, semantic codec/model, scene/render, CMake, and regression-test files required by the frozen closure. Each group is bound to named obligations and verification commands in execution-contract.json. No production mutation has begun.


## v0.7 authority freeze

P31 froze the complete ResolvedBrushState → R1 StrokeOptions mapping in the
Authority contract and authority.lock.json. `last` is lifecycle-only: false for
preview and true for seal. Existing BrushDefinition v1 has no production R1
mapping; new authoring rejects it before begin, while V1 documents remain
read-only decode/render.


## v0.9 RED oracle materialization

The executable anchor oracle is materialized at `apps/ink_playground/tests/c2_red_oracle_test.py` and bound by exact SHA-256 in package.json. It first materializes the exact task-anchor revision into a temporary source tree, then runs only against that legacy surface, records raw command observations, and must exit 0 with every frozen pre-change gap RED before production mutation.
