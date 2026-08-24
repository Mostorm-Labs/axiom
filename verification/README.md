# Axiom Semantic Conformance

This directory contains verification-only artifacts for Axiom V1 semantic conformance.

Normative inputs:

- `docs/notion/authority/04-semantic-schema/**`
- `schema/axiom/v1/**`

The verification corpus, projections, observations, results, opstreams, and reports are **not** product storage formats, sync protocols, or public runtime ABI.

Core rule: adapters report observed facts; only the coordinator compares observations against golden authority and decides PASS / FAIL / BLOCKED_OPEN.

Bootstrap suite: `golden/v1/suites/seed-v0.1.json` (exactly 60 stable case IDs).
