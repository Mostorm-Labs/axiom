# SDK 0.4.1 release

This completes the user-authorized integration of codex/uiux-selection-handles
through `d79bb7fdde43ca99598efe7583dccec0d5665f03`. See release-0.4.0.md for
feature scope, owner/API boundaries, initial local checks and candidate failure.
The failed v0.4.0 tag/source remains preserved. A fresh numeric 0.4.1 tag is used
because local npm readback could not independently confirm registry absence for
retagging. No published tag/assets are replaced.

The only additional production fix explicitly converts the two Table axis counts
from size_t to uint32_t. They are already clamped to 2..64 (and 1024 total cells)
before this assignment. This removes MSVC C4267 under /WX without changing Table
geometry, validation or commit semantics. Local Clang narrowing-as-error checks
reproduce both implicit conversions before the fix and pass afterward.

All platform/package versions are 0.4.1; ABI/transport remain 1 and canonical
Operation/Snapshot formats remain unchanged. Clipboard v1 is a separate format.
The shared asynchronous controller clipboard gap is fixed in d632cc30. Ordinary
packages keep private owners hidden; locked Semantic/Skia dependencies and
32/96MiB defaults remain unchanged. Feature branches/worktrees remain available.

Final fresh verification follows the bounded conversion fix; the larger first
candidate matrix remains source-bound to 6638c646. Hosted release verification
must fully rerun on this new release source. Historical full-suite failures and
BLOCKED performance evidence remain open; no physical-input/IME, universal
performance, full-suite or formal G5/P34/Gate acceptance is claimed.

Fresh final checks pass: 40 selected native CTest groups plus 17 focused
PlacementDefaults/CreationHover/ObjectClipboard/PartialEraseLive cases; real
native Session/Runtime controller calls; 39 G5 Node and 15 controller Node tests;
37 package and 15 workflow/server tests; strict three-profile consumer TypeScript;
119 Runtime boundaries; offline Semantic resolution; all 25 affected production
C++ TUs under both shadow and 64-to-32 narrowing warnings-as-errors. Rebuilt
matching JS/WASM/resources pass actual Chrome external-consumer, placement,
creation-hover, clipboard and Table checks. The complete FC02Text group and
larger 14-check browser/DPR2/DPR4 matrix passed the first candidate before this
bounded conversion fix; their original logs/source binding are preserved.

Hosted Web/Windows/Demo and all six RN native target verification and six npm
publications must freshly pass this release source before completion is reported.
