# FC-04 Phase 2 P31 v0.2 — targeted binding correction

This versioned correction preserves v0.1 and the D07 cursor. It is a draft, not an executable P32 package. D06-B remains accepted, and the D07 functional P34 and separately blocked OOM/release finding remain unchanged. No Group/Ungroup implementation is authorized here.

## Resolved source and test deltas

- The real shared Facade and Selection input integration test are added to the proposed modification surface and locked to the D07 cursor in source-delta.json.
- Preserve all 160 CTest IDs, including FC01ASelectionInputIntegration. Supersede only assertions that require an intentionally supported multi-root command to be rejected; retain and explicitly test rejection of unsupported commands, invalid inputs and zero-mutation failure.
- Restore MSE-25 Escape/IME-first semantics from the accepted decision addendum. Preserve the owning-root-after-commit/cancel scenario as an additional distinct assertion under the same case, rather than renaming the existing accepted case.
- Preserve the eight property rows and MSE-01..MSE-27; do not relabel untested cases as passed.

## Blocking issue before P32

The proposal in projection-binding.md specifies the required data, owner and safety boundaries, but no exact ABI-preserving Windows/Web wiring has yet been established and accepted. DesignPreflight intentionally fails closed until a versioned owner decision binds the concrete runtime-to-Debug UI projection and per-target receipt path, associated source owners, test oracles and public ABI constraints. Do not change RuntimeFacade, C ABI, FieldId, wire schema or registry merely to make the package executable. The package author or upstream architecture owner must make that binding reviewable; a passing structural check is not P32 authorization.

The original 14 D07 IDs, 160 Selection IDs, 30 expanded D07 tests, 46 fault tests and two Scene regression IDs remain required when the eventual implementation package is authorized. Original evidence, known OOM fault injection and test history are preserved.
