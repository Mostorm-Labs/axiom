# SDK C++-first public multi-selection owner baseline — 013

Baseline `b63adbb7c5d2059f0d6dfd5bf976f6c03c386ab1`. The latest user direction, verified in source thread `01a13d78-2852-7342-b02a-c5fcf62ce0c1`, prioritizes a C++ Runtime API and platform thin adapters rather than complete stable C ABI. `user-direction.md` preserves the supplied supplement; old C ABI and v0.4 contracts are not altered.

A real `RuntimeComposition` test is prepared to use its borrowed `RuntimeFacade` for permission, selection projection, exact-target fill submission, terminal receipt and Undo. Its seed uses the existing real Runtime owner; the action under test does not call the internal multi-selection port. It is not a source-tree-independent installed SDK test.

MSVC `/W4 /WX` build of `axiom_sdk_cpp_public_multi_owner_test` exited 1. The actual errors include missing `RuntimeFacade::readMultiSelection`, `submitMultiSelectionProperty`, `setMultiSelectionPermission` and the public permission type. This is a compilation RED for the missing intended public C++ entry; no product test executed. Raw build output and exact input hashes are recorded.

A revised scope and authorization must be materialized before production changes because the previous P31 v0.4 targets the older C ABI direction and has `p32_authorized: false`. Existing tests/evidence and the separate D07 OOM finding remain intact.
