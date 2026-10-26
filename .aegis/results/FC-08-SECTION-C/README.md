# FC-08 Section slice C result

Source/result revision: `4a4c0b419a57fdaae12e8d7454f5463361f1c501`  
Materialized ref: `refs/heads/codex/fc08-section-history`  
Source baseline: `c4fde4d4b1843673a71cce0546921f4494b16635`  
Bounded C authorization: package revision `5a67b3db427e355fc7cfd07ca5a0f67bb5e06a0c`, `.aegis/packages/FC-08-SECTION-C-v0.1/package.json`, SHA-256 `ed127493889d543e57808cae37a596272da50bab3cdb7481d76d4c1eab5a054c`.

Status: **C code materialized; verification acceptance blocked by two frozen H03 input recipes.** This is not a DEV/P34/PHYS/release or full-runner PASS. The original product P31 package and its authorization flag remain immutable; the separately bounded C authorization records the user's instruction to continue C only.

## Implemented owners and behavior

- Semantic OperationEngine prepares the actual F, exposes a synchronous closed-domain local admission phase, checks the unchanged generation/ordinal basis, and reserves actual ChangeSet, ledger and store/index nodes before canonical publication. Reused local container Insert/Restore/Delete/SetTransforms/PatchProperties participate in the same reservation path.
- Interaction constructs actual inverses for 17/18 and those reused operations from coherent S/S1, validates U through the real OperationEngine, proves restoration of the touched state, and measures complete F/U wire and nested profile limits.
- The existing EditorHistory reserves and retains the actual pair before publication. Rejection preserves cursor/redo. Undo/Redo use fresh identities, retained exact operations, expectations, footprint checks and bidirectional admission again. Unrelated leaf content is preserved; late membership/dependency changes reject.
- The existing Runtime commits the retained History/cursor before calling its existing Scene publication owner. Derived publication failure cannot turn a committed canonical mutation into rejection. Section-specific Scene geometry, selection and public Windows/Web interaction remain D/E.
- Consumer-side protobuf mapping now constructs owned oneof/repeated children before handing them to the locked SDK. The SDK source, schema, descriptors and locks are unchanged.

See `changed-files.json` and `ImplementationDesignPreflight-002.json` for hashes and owner/source bindings.

## Exact verification

All commands below ran from the real worktree. Complete argv, exits, SDK facts, compile output, selected test names, JUnit observations and executable hashes are under the named directories.

- `python tools/fc08/section_c.py preflight --repo . --phase qualified-preflight`: both local PackageBindingPreflight and EvidenceContractPreflight exit 0, `PASS_SLICE_C`.
- `python tools/fc08/section_c.py build --repo . --phase qualified-001 --targets ...`: the exact target list is in `qualified-001/invocations.json`. Release MSVC 19.51 `/MT /W4 /WX` build exit 0. CTest exit 8: **689 selected, 686 passed, 3 failed, 0 skipped**.
- Of these, the C target has **139 tests: 137 passed and 2 blocked H03 inputs**. The third failure is the independently preserved A/B baseline test `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds` (existing Shape RichText-target expectation mismatch). No new implementation test failed.
- `python tools/fc08/section_c_native.py --repo . --phase qualified-native --skia <verified local SDK>`: build and CTest exit 0, **13/13 existing Runtime regressions passed**, 0 skipped. This includes EditorHistory, Text, Image, Connector/Sticky, facade and Brush regression consumers. It is not physical Section UI verification.
- Python ran with emitted `AXIOM_PROTOC`, runtime and prefix environment: `python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract -v`: **10/10 passed, 0 skipped**. `python tools/check_runtime_boundaries.py --root .`: exit 0.
- `allocation-observations.json` records real single-allocation failure sweeps through forward, Undo and Redo, including compound deletion and redo retention. Both SDK directions initialize before the scoped sweep. Cold SDK initialization, malloc/arena failure and every possible interleaving are unqualified. Development failures and interrupted diagnostics remain under `development-001`.

The initial `red-001` is a capability-presence RED probe, not FC08-RED-01 product evidence. `baseline-history-red-002` is a source-bound observation of the old closed History API, performed after implementation; its failed expectations are preserved and it is explicitly not used as acceptance or same-entry regression closure. The new execution path is proved by real Engine/History tests and frozen independent expected values.

## Frozen verification input blocker

H03's independent recipe generates four-byte integer OrderKeys, including `00000100`, `00000200`, etc. The established `OrderKey::isValid` invariant prohibits a terminal `00`. Both frozen cases therefore fail while establishing the initial scene, before the intended F/U boundary is reached. The second case also introduces its wrapper with terminal-zero key `00010000`.

- `FC08-H03/root-children-65534`: frozen positive command-admission expectation cannot currently be evaluated.
- `FC08-H03/root-children-65535`: fixture rejection must not be misreported as the intended inverse-budget rejection.

`h03-input-proposal-001/proposal.json` (SHA-256 `d31824ed2e41c9f72d71b3566efe2cf340f92c7eb1d7cefe0ee5bd3800cc660c`) contains a **PROPOSED_NOT_ACCEPTED** versioned overlay: append `01` to every four-byte key. It preserves IDs, order, object counts, geometry and both expected files. New independent F/U hashes are included. The original recipes, inputs, expected values, oracles and historical results have not changed. The user decision is pending.

A separately named valid-key companion test proves admission at 65,534 roots and pre-write rejection at 65,535 roots. It is not counted as a frozen H03 PASS. Frozen H04 33,554,431/32/33-byte operation cases exercise actual bidirectional admission separately.

## Coverage and continuation

`coverage.json` preserves all 60 logical cases/302 IDs. It records **51 native owner-subset observations, 2 blocked H03 variants, 249 NOT_RUN**. This inventory does not establish a complete product runner or discharge any full E01–E10 family. Chrome/WASM, physical interaction and D/E are NOT_RUN.

T06's actual complete retained F is replayed by real History and compared with unchanged independent canonical expectations. Its old incomplete shared redo carriers remain immutable and unqualified.

Next: decide the concrete H03 overlay, then bind a separate versioned input amendment and rerun it. The next implementation slice after that is D (Section bounds/world transforms, selection scope and Scene publication), requiring its own bounded authorization. PH01–PH04 remain FC13 debt; INT-01 retains FC04/FC10 ownership. No global Aegis registry or main branch was changed.
