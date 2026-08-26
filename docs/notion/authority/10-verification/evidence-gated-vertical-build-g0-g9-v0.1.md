# Implementation Verification Design — Evidence-Gated Vertical Build + G0–G9 v0.1

> Source page: https://app.notion.com/p/3c44c57a590c814c9364d030a35752a7
> Snapshot date: 2026-08-24
> Source status: Design Approved / Freeze Candidate — Implementation Verification Architecture

## Model

Formal implementation uses Evidence-Gated Vertical Build, not module-first integration and not throwaway-POC-first. Maintain an increasingly complete but always runnable/provable vertical system.

Sequence: G0 Verification Foundation → G1 Semantic Kernel → G2 RuntimeScene Foundation → G3 Basic Canonical Canvas → G4 Interaction + Ink → G5 Large Canvas Optimization → G6 Rich Editing + Platform Lifecycle → G7 Local Data Runtime → G8 Sync + Recovery → G9 Integrated Product Gate → hardening.

## Evidence levels

E1 Contract/Unit; E2 Reference/Mock; E3 Integration/Golden; E4 Physical/Demo. Every Gate delivers Production Code, Reference/Mock, Automated Evidence, Demo/Inspector/CLI, Benchmark/Diagnostics and Gate Report. Every Gate has at least one runnable/viewable artifact.

Gate Report is machine-readable and ties gate/commit/status to correctness, performance measurements and artifact references. Unfrozen benchmark numbers remain observations, not Product SLO.

## Gate overview

G0 proves trustworthy measurement substrate: Conformance CLI + Golden Corpus. G1 proves canonical semantic write/replay. G2 proves derived RuntimeScene/bounds/spatial incremental correctness. G3 proves semantic→scene→Skia→surface vertical rendering. G4 proves input/session/preview/canonical/selection/eraser. G5 proves large-scene optimization with reference parity. G6 proves rich editing and platform lifecycle. G7 proves local durability/crash restore/Local Ready. G8 proves offline/reconnect/gap/duplicate/lost-ACK recovery. G9 proves integrated product behavior.

## G0

Inputs are Reference IDL/Common Wire, Operation validation, Golden Corpus Seed, Projection/Result schema and governance. Implement stable corpus layout, ConformanceCase/Runner/Result/Divergence, CLI and deterministic test services. Oracle is reviewed golden expected semantic/error/digest/reference codec; current implementation cannot bless expected. Exit requires seed/invalid/first-divergence/deterministic repeat/CI integration evidence.

## G1

Wire→typed Operation→validation→prepare plan→atomic apply→ObjectStore/Index→Semantic Document→ChangeSet. Keep simple ReferenceObjectStore parity oracle. Reject tests prove unchanged state. Runnable Semantic Replay Inspector. Structural performance requirement: no whole-document scan for ObjectId lookup or single-object mutation.

## G2

Semantic Document + ChangeSet→SceneCompiler→RuntimeScene→Bounds/Spatial/HitTest/ViewQuery. Permanent FullSceneCompiler/simple reference modes prove incremental/full equivalence. RuntimeScene remains derived/rebuildable.

## G3–G6

G3 creates basic canonical Canvas demo with real surface. G4 adds interaction/Arc/ink with preview→canonical handoff evidence. G5 adds spatial/tile/cache/scheduler optimization only behind reference comparison and performance playground. G6 adds RichText/complex editing plus lifecycle recovery and platform harness evidence.

## G7–G8

G7 proves local commit→durable storage, restore, crash windows and LocalRecoveryClosure without coupling Local Ready to Cloud Ready. G8 proves remote persist-first, order readiness, duplicate/out-of-order/gap/reconnect/no-echo/frontier recovery and explicit unresolved collaboration policies.

## G9

Integrated Product Gate combines canonical correctness, platform/lifecycle, persistence/sync, performance/resource and user-visible demos in one product scenario. No layer may claim completion solely because code compiles.

## 07 intake

Later Gate packages must directly verify 07 invariants: whole-op atomicity, ChangeSet projection, incremental/full scene equivalence, persist-first remote apply, LocalRecoveryClosure, resource materialization separation, session conflict fallback, lifecycle no-auto-commit and Presented/CanonicalVisible handoff.
