# FC-08 Section C — accepted H03 input amendment

Status: **READY_FOR_CONTROL_REVIEW** for this bounded C verification amendment. The accepted H03 input blocker is resolved. No DEV/P34/PHYS/release or full302 product-runner PASS is asserted.

Result/source revision: `7570a816b058f7df2f67ef7fcdcbf22835e93ce8`  
Materialized ref: `refs/heads/codex/fc08-section-history`  
Unchanged production source: `4a4c0b419a57fdaae12e8d7454f5463361f1c501`  
Accepted package revision: `6bab7b7ffde876317c11a8e69201da9505d6a476`  
Package: `.aegis/packages/FC-08-SECTION-C-H03-v0.1/package.json`  
Package SHA-256: `afe81967b3032612920d2c4b9752ba2ced3c203a555f57149e2a131269ace4b0`

The user explicitly accepted proposal `d31824ed2e41c9f72d71b3566efe2cf340f92c7eb1d7cefe0ee5bd3800cc660c`. The new package records that decision and binds a separately versioned H03 input overlay. All four-byte ordinal OrderKeys acquire terminal `01`; object identities, sibling order, geometry, counts and both original expected files are unchanged. No production implementation or interface was changed.

## Real SUT results

- `FC08-H03/root-children-65534`, input version `H03-v0.1`: real F admits; object counts are 65,534 -> 65,535 -> 65,534 -> 65,535 across F/actual History Undo/Redo. Canonical generation and commit ordinal each advance three times; the three emitted operation IDs differ. History remains one entry and its cursor follows Undo/Redo.
- `FC08-H03/root-children-65535`, input version `H03-v0.1`: the forward operation is valid/prepared, but the required inverse root guard would contain 65,536 members. Actual C admission rejects before canonical/History publication. Objects, generation, ordinal and History/redo state remain unchanged; F is absent from the applied ledger.
- Reference U bytes are independently reconstructed only to check approved identities. The application consumes the actual inverse retained by EditorHistory, not the reference U carrier.

`H03-observations.json` contains parsed actual observations. `qualified-H03-raw` preserves full same-binary raw output, IDs, source identity and input hashes. CTest truncates passed-test stdout to 1,024 bytes, so the two cases were additionally executed directly to retain complete observations; neither tests nor global CTest policy were weakened.

## Verification run from the actual worktree

- `python tools/fc08/section_c_h03.py preflight --repo . --phase qualified-preflight`: **both PackageBindingPreflight and EvidenceContractPreflight PASS_H03_AMENDMENT**, exit 0.
- `python tools/fc08/section_c_h03.py build --repo . --phase qualified-001 --targets ...`: exact target list, CMake consumer arguments, commands and exits are in `qualified-001/invocations.json`. Locked SDK, MSVC 19.51, Release `/MT /W4 /WX`. Build exit 0.
- Selected native matrix: **689/690 passed, 0 skipped**, CTest exit 8. The only failure is the preserved existing `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds`, independently established before this amendment. It is not reclassified or fixed here.
- **C native tests: 140/140 passed**, including the two revised H03 cases, one original-input diagnostic, existing F/U/History, allocation and T06 regressions.
- Direct H03 cases: **2/2 passed**, exit 0.
- Existing Axiom Python descriptor/semantic contract tests: **10/10 passed, 0 skipped**; Runtime boundary checker exit 0.
- Verification-tool integrity tests: **6/6 passed**. These are separately classified and are not Axiom product tests.
- `python tools/fc08/section_c_h03.py audit --repo . --phase qualified-scope`: **PASS_H03_AMENDMENT**, exit 0.

`native-evidence-reuse.json` proves production paths are byte-for-byte identical to the earlier 13/13 Native Runtime regression subject. Those 13 results are reused; no new native-host or physical run is claimed. The Semantic SDK was resolved offline from the locked Store, with no dependency rebuild.

## Preservation and scope

The original H03 fixtures, accepted catalog, independent writer/oracles, expected values, original C packages/results and prior failures remain immutable. `red-original` preserves a fresh same-binary failure on the old malformed input version, and the diagnostic continues to prove the original setup rejects. These original failures did not become PASS; the accepted overlay is a separate input version. `blocker-resolution.json` records the scoped resolution without editing the historical blocker record.

`coverage.json` retains 60 cases/302 logical variant IDs. The local native-owner subset now records 53 variants, with the two H03 rows explicitly bound to `H03-v0.1`; 249 remain NOT_RUN. This is not a complete product runner or full E01–E10 family discharge.

Changed ownership is limited to scoped control documents, independent fixture construction, verification tooling and the C test input binding/observations. See `changed-files.json`. Runtime, schema, descriptors, SDK locks, qualification hooks/default-OFF policy, global registries and main are unchanged.

The next smallest implementation slice remains D (Section geometry/world transforms, selection scope and Scene publication). D/E and Windows/Web Section interaction were not started under this input-amendment authorization. Independent control/Gate review remains a separate step.
