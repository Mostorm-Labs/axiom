# Evidence Contract — Functional D07 P31 v0.3

## Blocking functional evidence

- Exact package preflight output and source/cursor identity.
- Exact discovered test-ID sets for the 14 frozen D07 IDs and 160 Selection IDs; zero matches, missing IDs, extra IDs or skipped tests fail.
- Raw outputs, exit codes, CTest/JUnit files, exact build/toolchain facts and source revision for every required test.
- Individual real-owner evidence for SCENE-PREP-01..06; do not infer six passes from one aggregate test without distinct assertions/results.
- Real explicit Intent positive test for all eligible Operation kinds, exactly one Undo unit, Undo after selection change and Redo with fresh IDs.
- Fault-injection evidence at actual operation, reservation, History, Scene/index, ledger and result/publication boundaries. Preserve actual phase; no moved failpoint.
- Before/after Document, Scene/index publication/generation, History cursor/size, Redo and observer snapshots.
- Windows native and actual Chrome/WASM qualification where supported by the locked environment.
- Public ABI guard and source-scope diff.

## Separate overall D07 blocker

The descriptor-init OOM sweep and captured follow-on hang are known failing evidence. They remain blocking for overall D07/release, but they are not the functional P32 closure target in this narrowed package. Preserve the existing report and raw stacks. Do not relabel the sweep as corroborative, delete it, move the fault or claim overall D07 PASS.

## Corroborative

Prior successful tests from other revisions, historical smoke runs and source narrative are context only; they do not substitute for current-revision required evidence.

## Materialization

Return exact result revision, reviewer-resolvable materialized ref, source diff and all raw test/evidence refs. No official P34 PASS is claimed by implementation.
