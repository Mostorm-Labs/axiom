# World navigation tiles: implementation and evidence

Execution base: `051ee02700a3770fe29796f5486c3a6b9cb5e583` in
GitHub Mostorm-Labs/axiom. This is an engineering realization under the user’s
ordered request and G5 v0.3 WP-G5-12..19, not a formal G5/P34 acceptance.

The production Runtime now queries ordered commands for complete world tile
regions, including filter gutter. Navigation consumes vector-solid-v1 sealed
outlines and Ink v2 compact coverage through existing shared Skia helpers. It no
longer derives its production drawing source from the last viewport screenshot.
The legacy three-argument screenshot method remains for compatibility/tests;
new Runtime navigation uses the world-source query overload.

Render retains world-addressed tiles at private power-of-two scale levels. It
uses ready target cells, then the nearest retained parent, then complete child
coverage up to three levels. These are navigation feedback representations,
never exact coverage receipts. Coarse missing coverage is prepared first. A cold
incomplete plane cannot clear the existing full presentation. Shared gutter
sampling and disjoint interior patches prevent double alpha at adjacent or
parent/child boundaries. Full stroke geometry is queried at expanded tile bounds;
strokes and material seed/phase do not restart at tile boundaries.

Jobs use the existing cooperative scheduler implementation on the owner thread.
Compact coverage advances in ranges of 32 knots, keeping the predecessor at every
range boundary and max-combining the same pinned equation. A provisional 3ms
check between units and a configurable unit cap bound admission; this is not a
hard deadline. Material composition and vector path draw remain atomic. Small
camera motion retains an applicable world job; out-of-view work, Scene changes,
surface/device/resource changes and budget denial cancel or fall back safely.
Immutable per-tile source snapshots and raster resources use shared RAII budget
leases. Current Scene source revalidation is conservative; unchanged tiles retain
pixels after source equality, while changed sources rebuild locally. This is not
an Axiom-wide budget/scheduler closure or a completed general Tile implementation.

Runtime stops navigation refinement after idle and complete coarse coverage,
then requests the existing exact current-camera canonical frame. Navigation
presents do not create canonical submissions, count as canonical coverage, or
retire ARC. Unsupported/mixed content, transformed objects, erase masks,
nondefault backgrounds or admission failure use the existing exact path.

Observed Chrome/ANGLE Metal Apple M2 diagnostics, 64 strokes × 16 samples:

| Logical viewport | Mode | Longest RAF CPU callback | Exact receipt after input |
| --- | --- | ---: | ---: |
| 800×500 DPR1 | zoom | 20.2ms | 1182.6ms |
| 800×500 DPR1 | touch | 9.3ms | 1182.8ms |
| 400×250 DPR1 | zoom | 12.2ms | 1116.5ms |
| 400×250 DPR1 | touch | 8.2ms | 1183.3ms |

These are single diagnostic observations, not controlled speedup ratios, physical
input-to-photon, GPU times or SLOs. The exact settled CPU path is still about 1.2s.
A cold initial shader/tile may cost more; an earlier candidate retained in local
logs reached 90ms and 3.3s exact completion before idle ordering/work-policy
changes. No zero-gap guarantee exists for previously unseen content with no
cached ancestor/child: existing complete presentation is held until coverage is
ready. Prefetch/idle prewarming and general mixed-content effects remain open.

The original crop scenario was reproduced through the real root SDK: 16 strokes,
exact zoom 2× then zoom 0.5×. Navigation now obtains outside-old-viewport content;
complete world coverage is visible while canonical receipt remains 18. Exact
completion advances receipt to 19, semantic generation stays 16 and no errors
occur. Screenshots and raw observations are in the evidence bundle.

Verification: final native G5 suite 25/25 before the final duplicate-source guard;
final focused cache/navigation/tile/sampling guards 4/4 and actual WebGL probe pass
including duplicate identity rejection. Public SDK browser, frame-handoff
loss/restore, Shell/Axis and example pass with matching wrapper/WASM artifacts.
Full repository CTest is 214/236 with the same 22 failure names as its predecessor,
including FC02Text 240s timeout. That full run precedes final navigation
work-policy/cancellation/identity guards, which received focused/GPU/browser
verification; it is not represented as a complete final-source full-suite run.
Node workspaces retain the historical C8 exact-source evidence generator failure
with `git ENOBUFS`; semantic CLI 137/138. Dependency locks and historical evidence
were not modified. Physical Android/Windows/iOS results remain BLOCKED_EVIDENCE.

Evidence: `verification/evidence/performance/g5-world-navigation/`.
Interactive matching SDK: http://127.0.0.1:4187/example.html . The existing user page
was not reloaded. Classification: PASS_WITH_FINDINGS engineering checkpoint.
