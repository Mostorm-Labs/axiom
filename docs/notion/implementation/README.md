# Axiom Implementation Packages

This directory contains implementation-facing work packages derived from published Axiom architecture authority snapshots.

## Purpose

An implementation package translates frozen architecture into executable engineering scope without redefining the architecture itself.

Each package should identify:

- the implementation goal
- authority dependencies
- input dependencies
- in-scope and out-of-scope work
- required contracts and hard invariants
- implementation work packages
- mocks and oracles
- automated verification
- runnable demos
- performance thresholds
- required evidence
- exit criteria
- architecture-blocker handling

## Gate Layout

Gate packages are expected under:

`docs/notion/implementation/gates/G0/` through `G9/`

Directories are created only when a real package is published; empty placeholder directories are intentionally not committed.

## Authority Rule

Implementation packages may interpret frozen authority into concrete work, but must not silently change frozen architectural decisions.

If implementation exposes a contradiction, use the Architecture Blocker Protocol described in `docs/notion/README.md`.

## Evidence

Executable evidence belongs under the repository's existing `docs/evidence/` hierarchy, not under `docs/notion/`.
