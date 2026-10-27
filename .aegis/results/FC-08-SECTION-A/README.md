# FC-08 Section A — implementation receipt

Source result: `ad256785add190e7a83ef89122b7d9473e63064c`.
Materialized ref: `refs/heads/codex/fc08-section-foundation`.

The direct human implementation instruction is materialized in package
`FC-08-SECTION-A-v0.1` at `48282ee83f70dcbfa0b7db3c7d252797fcb91fe9`.
Its exact package SHA-256 is `d83cbc26289764a96a5d4e0c2152d281fa8c3ea331163195f29e3b3fc2cb9683`.
The mechanical private-validation and legacy-diagnostic path amendment is committed
at `62d9c619a261f11c3eabf086f85d948931a58783`. It adds no B-E behavior.
Both package preflights were run with `--repo .` in this real worktree and passed.
The original product P31 package still has `P32_authorized=false` and
`may_start_code_execution=false`; it and earlier evidence were not rewritten.

## Implemented and owner boundaries

- `schema/axiom/v1/proto/**`: accepted Section10/version1/content12 and 17/18
  IDL identities; current descriptor publication only. Existing Shape label4 and
  already implemented operation16 are reflected in publication. Independent
  accepted descriptor comparison and historical-field preservation tests pass.
- `schema/axiom/v1/registry/**`: Section kind and only opacity/fill applicability;
  17/18 are explicitly marked schema-only pending B.
- `runtime/semantic/include/**`: typed SectionContent, appended content variant and
  ObjectKind10. Membership stays in Placement; no parallel membership model.
- `runtime/semantic/src/**`: existing mapper/codec, shared normalization/domain
  validation, property admission, and Group-like parent capability. Required
  presence, finite positive boundary/extents, UTF8/title byte limit, negative zero,
  forbidden properties/defaults/masks are enforced. Large wire kinds cannot wrap
  through the narrowed enum. Existing InsertObjects and staged graph validation
  accept a Section parent without changing child local coordinates.
- `snapshot.cpp` rejects Section until its explicit capability is implemented.
  The legacy G1 diagnostic projection throws on Section; all old output branches
  remain unchanged. This is diagnostic compatibility, not Section snapshot support.
- Tests and `tools/fc08/section_a_*.py` exercise the real codec/OperationEngine.
  Frozen expected values and reference oracles are unchanged. The test carrier
  inserts frozen leaf bytes verbatim, preserving invalid UTF8/duplicates/unknowns.
  The frozen model-writer SCW06 uses its exact input and expected canonical hex.

## Actual results

At the exact source result, configure/build exit0 with MSVC19.51.36246 x64,
warning level4, warnings as errors and static release CRT. Published locked Semantic
SDK is consumed. Qualification hooks remain OFF; no dependency rebuild/lock edits.

- Section A: 48/48 PASS,0 skipped, including all41 W02 variants and7 additional tests.
- Selected native matrix: 400 PASS,1 FAIL,0 skipped (401 total); CTest exit8.
- The one failure is
  `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds`:
  old test expects Shape to reject RichTextTarget while released Shape-label code
  accepts it. A separate build against unmodified427580ae reproduces the same
  failure (exit8). Its Semantic/Foundation/schema/build sources equal the A start.
  This old test is retained unchanged. No all-regressions-pass claim is made.
- Python current descriptor/semantic contract tests:10 PASS,0 skipped, exit0.
- PackageBindingPreflight/EvidenceContractPreflight: exit0 each.
- Source-scope/preservation audit: exit0; all302 frozen variant input/expected
  bindings retained, historical descriptor locks and accepted authority unchanged.

Exact argv, cwd, exit codes, XML and raw output are in
`qualified-001/invocations.json`, `qualified-python-001/invocation.json`,
`qualified-preflight-001/invocations.json` and `baseline-regression-001/`.
Toolchain, real test executable/library hashes and compile commands are in
`qualified-environment-002/identity.json`; raw compiler version is in001.
The evidence manifest hashes every supplied artifact.

## Failures and evidence limits retained

- Initial core setup hit an existing imgui Git pack permission failure; the
  Semantic-only resolver and offline Store path succeeded. Core GTest/JSON heads
  were checked against locks; no Protobuf/Abseil/protoc/Skia source rebuild occurred.
- `red-002` records42 tests/5 failures against pre-Section runtime. Its old test
  source was not snapshotted at the time, so this is corroborative RED, not an
  exact-revision complete product RED. Accepted bootstrap-003 baseline RED remains
  the previously bound evidence. The early test version did not consume every
  frozen predicate; the final tests do. It is not promoted as full W02 proof.
- `green-001` is a real mapper compile failure, corrected using explicit title
  byte assignment. `red-kind-001` caught actual enum truncation and a test harness
  assumption that decode alone performs domain admission; the final harness uses
  real OperationEngine admission. Frozen expected values were not changed.
- `regression-001` failed compiling the exhaustive old diagnostic visitor;
  fail-closed compatibility is recorded in the path amendment.
- `audit-002` incorrectly read the historical flag name; audit003 onward reads
  actual `P32_authorized` and verifies immutable scope. No original flag changed.
- Environment001 incorrectly recognized only `/MT` and `/c`; actual CMake commands
  use equivalent `-MT` and `-c`. Environment002 corrects metadata, no options changed.
  Early build journals also had a case-sensitive environment-key check reporting
  MSVC initialization false. Exact final build and compiler command evidence confirm
  the actual toolchain; earlier records are preserved rather than rewritten.

No operation17/18 runtime execution, History, Scene, Section snapshot, facade or
host UI is delivered here. Those behaviors and Chrome/WASM/PHYS stay NOT_RUN.
The complete E01-E10/302 product runner remains incomplete; enumerated IDs in
coverage.json are not passes. PH01-PH04 remain FC13 and INT-01 retains FC04/FC10
ownership. No DEV/P34/PHYS/release verdict is claimed.

Next bounded slice is B: actual17/18 payloads, guards, staged atomic mutation and
rejection through existing Semantic owners. A authorization does not expand to B-E.
