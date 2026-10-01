# Windows multitouch recovery evidence — 2026-10-01

This bundle snapshots the tested `9085b973186a0ab69b5b95a182dcafb4a04661ca`
Windows executable. [windows-result.json](windows-result.json) records the
verified initial revision separately from the tested fix revision.

The user confirmed recovery on a physical touchscreen. The owner received
distinct native contacts and overlapping Ink previews. The first controlled
183/184 slice submitted two independent strokes. A fresh default Vector window
then reached eight concurrent contacts/outlines and 27 successful scene applies
from an empty scene. No drawing input was injected by the agent.

These mixed trials do not close three identical protocol repetitions and are
not a G4/G4.5 Gate PASS. Simultaneous Pending contacts also formed viewport
gestures. Metrics and their scope are explicit in the result and summaries.

| Artifact | Purpose |
| --- | --- |
| `pointer-diagnostic.log.gz`, `vector-canonical.log.gz`, `vector-summary.json` | Fresh default Vector recheck, snapshot at archive time |
| `pointer-diagnostic-layered.log.gz`, `layered-canonical.log.gz`, `layered-summary.json` | First recovered run; includes later eraser trials |
| `pointer-diagnostic-before.log.gz`, `before-summary.json` | Negative Axiom input-routing observation before layered fix |
| `native-touch-control.log.gz`, `native-touch-control-summary.json` | Independent native hardware control, not Axiom acceptance |
| `popup-regression-red.log.gz`, `popup-regression-green.log.gz` | Actual provider regression before/after fix; no touch injection |
| `windows-build.log.gz`, `windows-ctest.log.gz`, `windows-python-contract.log.gz` | Requested build and related checks, including failures |
| `verification-summary.json`, full pytest/CTest/build logs | Remaining failures and incomplete full C++ build |
| `screen-capture.png` | Earlier Object Eraser final display; not the Vector recheck or peak concurrency |
| `sha256-manifest.json` | SHA256 and sizes of every artifact except the manifest itself |

Gzip files are lossless byte snapshots, use mtime=0, and expand to the original
text logs. `canonical_strokes` in the original diagnostic means submitted
operations, including eraser operations. `preview_sessions` counts outline
entries, not BrushSession objects. The Vector canonical object count is also
cross-checked against the independent per-pointer scene-apply log.

The [retrospective](../../../engineering/multiplatform-multitouch-brush-retrospective-20261001.md)
documents implementation pitfalls, evidence limits and remaining risks.

Local verification used clang-cl 22.1.8 with the Release preset and explicitly
enabled render tests/Arc. The preset itself defaults render tests OFF. The
local `windows-test-assertions.cmake` hook kept assertion-based test bodies
active using /UNDEBUG on executable targets ending in `_tests`; production
flags were unchanged. The configured common C++ flags included
`/clang:-Wno-missing-designated-field-initializers`. Deprecated SkiaInkBackend
warnings were not suppressed: they still blocked the attempted full build.
The exact local configuration is recorded in verification-summary.json.
