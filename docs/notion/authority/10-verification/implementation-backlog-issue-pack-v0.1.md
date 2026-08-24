# Implementation Backlog / Issue Pack v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81a48bd8c605fc5fe88a
> Snapshot date: 2026-08-24
> Source status: Execution Pack — derived from G3 Freeze Candidate contracts

## Execution rules

One IH = one independent issue/primary responsibility. Target paths remain logical until IH-00 reconciles real repo/build/CI. Golden expected is read-only. Verification-only hook does not enter Product public ABI. Every issue delivers code/data, acceptance tests, evidence artifact and implementation summary. Out-of-scope is a hard boundary.

## Dependency chain

IH-00 → IH-01 → IH-02 → IH-03 → IH-04; IH-03→IH-05; IH-04+05→IH-06→IH-07; IH-00+07→IH-08; IH-01→IH-09; IH-08+09→IH-10/11/12/13→IH-14→IH-15. Protocol seed becomes trusted before platform seed.

## Issue index

- IH-00 Repo / Branch Reconciliation — real repository/build/CI/path mapping.
- IH-01 Schema + Workspace Skeleton — 12 schemas validate by one command; workspace builds.
- IH-02 Protocol Package + Envelope Codec — typed protocol + strict parse/version rejection.
- IH-03 Runner Core A — Handshake/Session/Action/Completion state machines.
- IH-04 Runner Core B — Event/Source/Fault/Fence/Finalization state machines.
- IH-05 Scripted Adapter + Transport — in-process + serialized loopback drives malformed behavior.
- IH-06 Materialize protocol-seed-v0.1 — 56 vectors; all blocking meta-tests pass.
- IH-07 Shared CLI + Protocol CI Gate — clean checkout runs protocol seed and emits evidence.
- IH-08 Verification Native Hooks / Common Host — test-only fault/source seams driven by adapter.
- IH-09 Materialize platform-seed-v0.1 — 28 stable scenarios schema + semantic valid.
- IH-10 Web Reference Adapter — applicable scenarios emit complete observation/result.
- IH-11 Windows Native Adapter — minimal native host runs applicable set.
- IH-12 Android Instrumentation Adapter — Activity/View/JNI host runs applicable set.
- IH-13 Apple XCTest-style Adapter — UIView/ObjC++ host runs applicable set.
- IH-14 PR CI DAG — schema→protocol→semantic→platform→aggregate dependency enforced.
- IH-15 Nightly / Release Wiring — repeatable platform-family evidence and G3 aggregation.

## Source child issue pages

The Notion execution pack contains individual child pages for IH-00 through IH-15. This repository snapshot preserves the authoritative issue index/dependency/completion signals used by G0 mapping. When an implementation worker needs per-IH file-level steps beyond this index, the dedicated G0 implementation package under `docs/notion/implementation/gates/G0/authority-map.yaml` is the execution mapping authority and must not invent semantics outside the migrated 10 contracts.
