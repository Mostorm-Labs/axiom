# Prepared mask sharing publication

Canonical and navigation rendering reuse the same immutable prepared mask primitives. Drawing-mask residency is admitted on demand at `min(32 MiB, maxEntryBytes, maxBytes / 4)`. The existing 8 MiB single-plan and synchronous construction guards, larger exact-query entitlement, global limits, identity checks and GPU retirement remain unchanged. Same-budget query/strict confirmation transitions retain prepared discovery.

The commit materializes the reviewed Runtime delta on `4d015193`, including its navigation-mask preparation dependencies. The original working tree and frozen 4342–4346 demos remain intact. Only 24 Runtime Render files differ substantively; pre-existing line-ending differences elsewhere are left untouched.

The independent control review is **PASS_WITH_FINDINGS**, scoped to MPR1–MPR5. Verification includes 42 focused native tests, 9 standalone private-opt-in tests, 10 Node tests, 4 Python checks, 122 boundary contracts, four ordinary Web windows and two correctness windows. The 24 original relative checks, eight pixel comparisons and 12 same-stage snapshots passed. New test targets default OFF and use matching private observation definitions when explicitly enabled; the Metal fixture is restricted to Apple.

Known limits remain: approximately one-second hot-route content-submission proxy gaps, dense FilledPath triangulation, no physical-trackpad or other-platform ABI qualification, and no overall G5 or performance PASS. The after-erases capture's attached navigation marker is older than its generation-3 state; current receipt/onFrame/canonical-framebuffer readback provide separate scoped evidence. The informational factorial link in the frozen correctness report is erroneous; the structured producer result binds the parent factorial. Frozen reports are copied verbatim, including these findings and host-local evidence references; raw captures and generated SDKs remain local.

See `publication.json` for exact source provenance and `control-review.json` for the independent findings.
