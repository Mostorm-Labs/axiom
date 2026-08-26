# Gx — Implementation Package Title

## Goal

Define the concrete implementation outcome for this package.

## Authority Dependencies

List the exact frozen authority snapshots this package consumes.

## Input Dependencies

List code, schemas, generated artifacts, previous gates, platform capabilities, and other prerequisites.

## Scope

Define work that must be implemented in this package.

## Non-goals

Define work explicitly excluded from this package.

## Required Contracts

List APIs, schemas, ownership boundaries, lifecycle rules, and compatibility requirements that must be implemented without architectural reinterpretation.

## Hard Invariants

List correctness properties that must hold for every implementation and platform.

## Implementation Work Packages

Break the work into implementation units suitable for Codex or human engineering execution.

## Mock / Oracle

Define reference implementations, deterministic oracles, mocks, golden data, or comparison models used to validate behavior.

## Automated Verification

Define unit, integration, conformance, replay, fuzz, or cross-language tests required by this package.

## Runnable Demo

Define the smallest executable demonstration that proves the implemented vertical slice works end to end.

## Performance Thresholds

Define measurable latency, frame-time, memory, throughput, or scale gates where applicable.

## Required Evidence

List the concrete evidence artifacts that must be produced under `docs/evidence/` or by CI.

## Exit Criteria

List objective conditions required to pass the gate.

## Architecture Blocker Protocol

If implementation contradicts a frozen authority, do not silently redesign the implementation contract. Record the blocker, stop the affected path, resolve it in the architecture workflow, publish a new authority snapshot, and then resume implementation.
