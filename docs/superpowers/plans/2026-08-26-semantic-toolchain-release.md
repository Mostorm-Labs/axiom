# Semantic Toolchain Release Asset Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with verification checkpoints.

**Goal:** Replace repeated hosted Abseil/Protobuf source builds with one immutable Linux x86_64 Release asset while preserving lock, manifest, and differential validation.

**Architecture:** A producer builds the pinned semantic toolchain from source and creates a deterministic self-contained archive. A consumer fetches the exact asset recorded in `semantic-toolchain.lock.json`, verifies the archive and manifest, then exposes the same `.deps/protobuf` layout to CMake. No implicit source fallback is allowed in consumer CI.

**Tech Stack:** Python 3.12, CMake/Ninja, GitHub Actions, GitHub Release assets, SHA-256.

**Spec:** Current GT-G1-02 hosted semantic-codec workflow and `deps.lock.json`.

## Global Constraints

- Target only `ubuntu-24.04` / `x86_64` / C++20 / Ninja.
- Preserve current Protobuf 36.0, Edition 2024, Abseil 20250512.1 lock identities.
- Do not change semantic golden expectations or production codec behavior.
- Asset extraction must be path-safe and atomic.
- Consumer CI must fail on missing or mismatched lock/manifest; it must not compile from source.
- Existing GT-G1-01/GT-G1-02 evidence remains immutable historical evidence.

### Task 1: Package contract and verification tests

Add tests for canonical SDK identity, deterministic archive bytes, manifest file hashes, path traversal rejection, and lock mismatch rejection.

### Task 2: Producer and consumer tools

Implement deterministic package creation, strict verification, and atomic extraction using the existing bootstrap output layout.

### Task 3: Release producer workflow

Build once on hosted Linux, package twice, compare bytes, run source-free smoke validation, and publish an immutable prerelease only on manual dispatch.

### Task 4: Consumer workflow integration

Add a lock file and switch the semantic codec workflow to fetch and verify the Release asset before the existing descriptor, runtime, and differential checks.

### Task 5: Local validation and evidence

Run package/consumer tests, workflow static checks, and a local clean source-free smoke build. Record that hosted validation must be rerun against the consumer workflow after publication.
