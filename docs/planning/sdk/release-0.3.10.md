# SDK 0.3.10 release

This patch release advances main by fast-forward to the user-selected integration
`3783d457e8d5b45840009cd9e618323d88df8f7a`, preserving the SDK 0.3.9 release
and Android initialization fix. It improves existing navigation and rendering
behavior; it introduces no public SDK capability or semantic format change.

Runtime retains camera, canonical document and receipt ownership. Scene supplies
derived bounds and authoritative object queries. Render owns disposable retained
coverage, prepared geometry, cooperative work and Skia submission.

- Prioritize bounded coarse viewport coverage while navigating, then release
  incomplete coverage after the quiet interval so exact preparation can converge.
- Share bounded prepared stroke geometry and use the existing time allowance for
  useful cooperative exact progress, retaining explicit budgets and cancellation.
- Reuse retained coverage across newly exposed borders only when the Scene query
  proves they contain plain background. Unknown/nonempty borders keep fallback.
- Correct transformed compact-ink preparation and local chunk selection while
  retaining strict canonical pixels, resource limits and existing fallback for
  unsupported forms.

All Web/API/RN/platform package versions and dependencies are 0.3.10. API/ABI and
transport remain version 1. Dependency locks, published Semantic/Skia artifacts,
canonical Operations and Snapshots are unchanged.

Fresh local release checks: 18 focused native cases including G5PreparedGeometry,
G5RenderCache and G5Navigation; 37 packaging tests; 15 workflow/server tests;
9 Node navigation/oracle tests; strict consumer TypeScript; 117 Runtime boundary
contracts; offline Semantic Store resolution; rebuilt matching Web/WASM/resources
with the external Chrome consumer, selected navigation and feedback checks.

Initial local script invocations used Xcode Python 3.9 and incomplete checker/test
arguments. Those invocation failures are retained under out/release-0.3.10-*.initial.log;
the corrected checks use installed Python 3.12 and all pass. No product workaround
or dependency rebuild was introduced.

The integrated source evidence under verification/evidence/performance records
the broader baseline findings: 67 existing native failed/not-built cases,
the denied-geometry full GPU phase failure, missing tsc in legacy workspaces,
and remaining exact-settle tails. This release does not claim those fixed or a
full-suite pass. Physical trackpad/display, device qualification and formal Gate
acceptance remain separate from the local synthetic Chrome checks.

Tag-triggered hosted gates must verify the exact release revision, Web/Windows
SDK and Demo archives, all six RN native targets and all six npm packages before
publication is reported complete. Existing published tags and assets are immutable.
