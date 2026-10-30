> Current consumer profile: Windows `axiom.dll` and the caller-side `axiom.hpp`,
> JavaScript `axiom.mjs`/`axiom.wasm`/`axiom.data`. See
> [consumer delivery](consumer-delivery-20261031.md),
> [preview packaging](sdk-preview-release.md) and the shipped SDK READMEs.
> Earlier drafts below retain their historical scope/status.

# Axiom SDK-00 review bundle — DRAFT

Source baseline: `50e53124ffe7ed2959d6bcf956c3d252fce6451a` of `Mostorm-Labs/axiom`. On 2026-10-29 `origin/main` was fetched and matched this revision; FC05 `e5e630c5662167763fc7ba66b94c35881b1e1470` is its ancestor. Existing worktrees were not changed. This is a planning and package-materialization result, not SDK implementation or accepted Authority.

- [V1 symbol inventory](api-inventory.md): every declaration, signature, mapped existing owner, candidate disposition and gap.
- [Boundary and compatibility decisions](contract-decisions.md): proposed typed interface, multi-selection migration, ownership, errors, lifecycle and unresolved decisions.
- [Verification and distribution](verification.md): proposed obligations, actual test discovery and platform support.
- [SDK-01 package candidate](../../../.aegis/packages/AXIOM-SDK-01/package.json): six-file content-addressed draft. `p32_authorized=false`; preflight success is not approval.

Historical D07 fault injection (`.aegis/results/FC04-D07-AB-P32-002/report.md`, last changed at `3615a38422a103024bc23924c570e3c585656670`) timed out during a real allocation-failure sweep with a Protobuf/Abseil lazy-initialization hang. The user requested merging this history despite that open defect, so the historical statement that it was not suitable for merge remains historical evidence; the merge does not prove remediation. Multi-selection progress 006 (`4eee69946dad61e0fc7678f9453423066c51750c`) retains MSE-20/23/26 gaps. Group scope evidence (`a8df6113c9399a68ff909e836333362b6d1a8247`) leaves physical retest pending. No later independent closure was identified in this repository inspection. These facts do not block producing SDK-00 but limit claims of atomicity, physical qualification and release readiness.

SDK-02 through SDK-07 remain the explicitly non-executable sequence in [verification.md](verification.md); no future accepted result or handoff reference is invented. The user-facing implementation plan remains outside the repository at `D:/axreview/sdk-20261029/Axiom API SDK Multiplatform Implementation Plan v0.2.md`; its bytes are identified in the SDK-00 result rather than treated as accepted Authority.
