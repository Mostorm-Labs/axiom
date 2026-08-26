# 08 Platform Contract — Authority Index

> Source: Notion `08 Platform Contract`
> Source page: https://app.notion.com/p/3c44c57a590c80938181ed05f2320a0c
> Snapshot date: 2026-08-24
> Repository status: current-direction / decision-closure

## Working authority set

- Cross-platform Contract Matrix v0.1
- ABI / Bridge Matrix v0.1
- Surface / GPU Matrix v0.1
- Thread Matrix v0.1
- Lifecycle Matrix v0.1
- Text / IME Matrix v0.1
- ExternalSurface Matrix v0.1
- OPEN Platform Decisions v0.1

08 no longer expands new platform architecture themes. It owns physical realization, Experiment Profiles, benchmark/POC decisions and platform qualification without redefining 07 logical semantics.

## Mandatory inherited semantics

Platform implementation must preserve canonical vs derived ownership, Operation/no-echo behavior, persist-first remote flow, and Presented/CanonicalVisible semantics from 07.

Separate-plane Arc Preview is permitted only when the platform profile can produce PlatformQualified presentation evidence. If a platform/profile cannot prove qualified canonical visibility, it must use inline preview or disable separate-plane handoff rather than clearing preview from approximate evidence.

## Platform work phases

- G0: freeze POC Experiment Profiles.
- G1: comparative benchmark / fault injection.
- G2: close V1 shipping blockers in ABI, lifecycle, Text/IME, ExternalSurface and related platform decisions.
- 10 Verification: trace platform Decision IDs into executable evidence.

## Status discipline

Source 08 uses Accepted / Current Direction / Proposal / Superseded / OPEN. Codex must not silently upgrade an OPEN physical decision to frozen architecture merely because one implementation is convenient.
