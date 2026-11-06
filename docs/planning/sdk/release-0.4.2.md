# SDK 0.4.2 release

This completes the authorized merge of codex/uiux-selection-handles through
`d79bb7fdde43ca99598efe7583dccec0d5665f03`. See release-0.4.0.md for feature
scope and clipboard contract, and release-0.4.1.md for the bounded MSVC conversion
fix and hosted outcomes. Published RN 0.4.1 and both earlier tags are retained.

This patch aligns related verification fixtures with the already approved
interaction/selection changes; it changes no production Runtime or adapter code.
The authority is docs/planning/uiux-default-interaction-v1.json (Enter inserts a
paragraph, Ctrl/Cmd+Enter finishes and selects the object) plus the approved
selection affordance implementation 04562284 in the user-selected branch.

The Windows fixture now checks ordinary Enter keeps editing and inserts a real
paragraph, Backspace restores the content, synthetic native Ctrl+Enter finishes,
and Select is active. It reactivates the actual Text widget before testing a new
empty draft. Native focus, text contents, object counts and composition guards
remain asserted; no physical keyboard injection is used.

The logical-input fixture independently checks a white grip center and violet
edge at DPR2, including stroke-expanded selection bounds, then verifies a ten-unit
pointer drag translates the object ten world units. Successful creation already
selects the shape, so a stationary second click (which now enters text editing)
is omitted from this geometry/drag fixture. It has an isolated invocation for
reproduction. The original cyan assertion fails before this update; the updated
pixel/drag case passes. A diagnostic pixel row was retained in local evidence,
not production code. Full macOS composition subsequently rejects a provider
rebind at line116; that unrelated failure remains recorded and unmodified.

All platform versions are 0.4.2. API/ABI/transport remain1, canonical formats and
32/96MiB budgets/accepted dependency locks remain unchanged. Initial feature
matrix and the0.4.1 fresh checks are preserved under their exact sources; final
checks and new hosted runs must bind this release. No full-suite, physical-input,
universal performance or formal Gate/G5/P34 acceptance is claimed.


The release also repairs a private build dependency: sdk/common read version.json
without registering it as a CMake configure input. Reused local builds retained
AXIOM_SDK_VERSION=0.4.0 after the wrapper reached0.4.2. Native public readback now
asserts the exported SDK_VERSION; this fails before rebuilding and passes after
adding CMAKE_CONFIGURE_DEPENDS. The Web external consumer compares the independent
exported constant too, rather than comparing two engine-derived values. Fresh
hosted builds were never affected:0.4.1 RN publication binds0.4.1. Earlier local
behavior evidence remains valid but is not proof of local version parity; the
new0.4.2 guards and rebuild establish that parity explicitly.

Final local checks pass:40 targeted native groups, isolated logical-input pixel/
drag RED→GREEN, real native Session/Runtime controls/clipboard with exact version
readback,15 controller Node tests,37 package tests,15 workflow/server tests,119
Runtime boundaries and strict Web/shared/DOM-free consumer types. Rebuilt matching
JS/types/WASM/resources pass actual Chrome external consumers with explicit
wrapper/engine version equality and default-interaction Enter/finish checks.
Full macOS FC00XRuntimeComposition still fails the unrelated rebind assertion;
its failure is preserved. Windows keyboard/focus and complete hosted consumer
verification must pass the exact new source before publication is called done.
