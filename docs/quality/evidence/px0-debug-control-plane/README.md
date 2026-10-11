# PX0-07 Debug Control Plane Evidence Index

- Package: `PX0-07-P31-v0.1`
- Task anchor: `7e3772d747cf3f7e158fb0917e5169a56065a08b`
- Qualification subject revision: `6fc1508f1a971abc5a90e068d0cc5f6ae82c064b`
- Evidence head: `refs/heads/codex/px0-07-closure-evidence` (evidence-only descendant)
- Repository: `github:Mostorm-Labs/axiom`

## Automated qualification

- Native profile: Ninja / Release / clang-cl, RF01=ON, POC01=OFF, Debug UI full, BUILD_TESTING=ON.
- Windows executable: `out/px0-07-native-vs2/apps/ink_playground/axiom_ink_playground_windows.exe`
- Web output: `out/g4-5-web-debug-ui-physical/apps/ink_playground/index.html`, `axiom_ink_playground_web.js`, `axiom_ink_playground_web.wasm`
- Native DUI/PX0 CTest: 14/14 PASS. Log: `.aegis/results/PX0-07/native-ctest.log`
- Structural contracts: 4/4 PASS. Log: `.aegis/results/PX0-07/structural-contracts.log`
- Generic command source audit: no forbidden generic debug command symbols; typed runtime owner queues remain allowed. Log: `.aegis/results/PX0-07/source-audit.log`
- Diff check: PASS. Log: `.aegis/results/PX0-07/diff-check.log`

## Windows physical matrix

Status remains pending until observed in a real Windows GUI session.

| Check | Status | Observation basis |
|---|---|---|
| show/hide | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| workspace navigation | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Vector / Marker / Chalk / Membrane | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Object / Partial Eraser | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| selection mode | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Undo / Redo | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Pan | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Zoom At | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Fit Content | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Fit Selection | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Fit Primary Object | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Surface Platform / CPU / GPU controls where supported | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| ARC diagnostics visibility | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Surface diagnostics visibility | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| telemetry visibility | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Activity request/receipt updates | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| hidden Debug UI does not consume Canvas input | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |
| Debug-owned pointer/keyboard sequence does not leak to Canvas input | PENDING_PHYSICAL_OBSERVATION | Requires real Windows GUI |

## Web physical matrix

Status remains pending until observed in a served browser session with the exact Web output above.

| Check | Status | Observation basis |
|---|---|---|
| show/hide | PASS | Browser observation: Debug UI toggled visible and hidden |
| workspace navigation | PASS | Browser observation: Dashboard/Control workspace switch rendered distinct panels |
| Vector / Marker / Chalk / Membrane | PASS | Browser observation: each control updated selected brush and status JSON |
| Object / Partial Eraser | PASS | Browser observation: each control updated selected tool and status JSON |
| selection mode | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Undo / Redo | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Pan | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Zoom At | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Fit Content | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Fit Selection | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Fit Primary Object | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Surface controls as actual owner state | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| ARC diagnostics visibility | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Surface diagnostics visibility | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| telemetry visibility | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Activity request/receipt updates | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| hidden Debug UI does not consume Canvas input | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |
| Debug-owned pointer/keyboard sequence does not leak to Canvas input | PENDING_PHYSICAL_OBSERVATION | Requires served browser/WebGL session |

## Commands for physical observation

Windows: launch `out/px0-07-native-vs2/apps/ink_playground/axiom_ink_playground_windows.exe` from the qualification subject build.

Web: `python -m http.server 8000 --directory out/g4-5-web-debug-ui-physical/apps/ink_playground`, then open `http://127.0.0.1:8000/index.html` in a browser.

## Scope and claim boundary

This evidence index contains no production changes and does not issue PX0, P34, DUI-M1, physical, Gate, or G5 PASS. Android and iOS are outside the PX0 blocking physical scope.
