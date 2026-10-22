# Windows touch double-tap to edit Shape labels

## Scope and current status

This follow-up addresses the Windows physical-device report that a touch
double-tap on a selected Shape did not enter its text editor. The Windows Host
already forwarded native `WM_POINTER*` contacts with full
source/pointer/generation identity, but only mouse `WM_LBUTTONDBLCLK` requested
text editing. Touch-promoted mouse messages are filtered, so the mouse-only
path could not see touch double-taps.

Double-tap recognition now lives in the existing Runtime selection path. It
qualifies two completed finger contacts on the same selected Shape, then starts
the existing TextEditSession and places the caret with the existing shared text
hit test. It handles empty and populated labels for rectangle, ellipse and
triangle presets. The host continues to collect and forward native pointer
facts; it does not identify Shapes, calculate caret positions, or create text
state.

The recognizer uses a 350 ms maximum contact duration, a 500 ms interval between
completed taps, 24 logical pixels of per-contact movement slop, and 36 logical
pixels between tap locations. A drag, timeout, multiple concurrent contacts,
pointer capture loss, source/lifecycle cancellation or tool-mode change clears
the candidate. The added behavior is for finger input under Select; pen and Ink
double-tap are not interpreted as text-edit requests.

Automated tests exercise real Win32 WindowProc -> Runtime -> TextFacade flows.
Physical touchscreen verification remains `PHYSICAL_PENDING`; test fixtures do
not synthesize physical-acceptance claims. The earlier report that the
transferred exe failed to launch due to missing resources is addressed for this
build by packaging the Runtime font/resource files beside the exe.

## Verification

- Windows Release build: VS 2026, MSVC 19.51 x64, `/MT`, `/W4 /WX`; target
  `axiom_minimal_runtime_host_windows` and Windows text-entry fixture compiled.
- `FC02WindowsTouchShapeLabelEntry`, `FC02WindowsShapeLabelEntry`, and
  `FC00XWindowsTouchIngress`: PASS.
- Full selected CTest list plus the new touch test: 235/235 PASS, 0 failed,
  0 skipped.
- Python Semantic/Host/font contract tests: 17/17 PASS.
- Strict Clang `-W4 -WX -Wextra -Wpedantic -Werror` syntax checks: 9/9 PASS,
  including the WindowProc fixture and changed Runtime translation units.
- Web Minimal consumer `axiom_canvas_demo_web`: Emscripten/locked Web Skia SDK
  link succeeded. No Web-only behavior claim is inferred from this Windows
  touch test.
- `git diff --check`: PASS.

The cases cover all three Shape presets, the first tap selecting an unselected
Shape, the second tap entering the empty-label editor, populated-label reentry,
caret geometry availability, unchanged object/operation revision on editor
entry, and rejection of long intervals, movement, simultaneous contacts and
capture-loss sequences. Detailed results are in `validation.json`.

## Windows build bundle

The portable bundle is `out/fc02-resume/touch-shape-windows-package-20261022.zip`.
Extract the archive and run `axiom_minimal_runtime_host_windows.exe` from the
extracted directory, keeping `axiom-font.otf`, `icudtl.dat`, and `fonts/` beside
it. The archive also includes the font license and notice. Artifact hashes and
the exact product source identity are recorded in `validation.json`. The exe
was compiled from the recorded Git HEAD plus the listed, uncommitted Runtime
and test changes; do not treat the embedded Git revision alone as the complete
source identity.

For a physical check, select a Shape, tap once if it is not already selected,
then tap the same Shape again promptly. The editor should open with the caret
near the second tap; an internal tap should move the caret. A drag, slow second
tap, two-finger gesture or cancelled contact should not open it.
