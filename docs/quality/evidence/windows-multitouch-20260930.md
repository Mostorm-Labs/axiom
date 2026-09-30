# Windows concurrent touch investigation, 2026-09-30

Repository: `Mostorm-Labs/axiom`, branch
`codex/g4-5-web-eraser-brush-packages`. The checkout and remote were initially
verified at `2c5dbb8cefe2b71ac92e47764bed897764e5d2c3`. Diagnostic instrumentation
was subsequently committed as `bff32030327d1527f7b100626f00477811cce66d`.

## Observed failure

The user reports that the second finger cannot draw in Axiom. A standalone
native Windows touch probe on the same device recorded four overlapping pairs
(162/163, 164/165, 166/167, 169/170), with both contacts updating. For 169/170,
the first contact moved approximately 754 pixels before the second DOWN,
1920 ms later. The user confirmed two differently colored tracks. This is a
hardware/control observation, not an Axiom functional checkpoint.

In the subsequent Axiom diagnostic build, `pointer-diagnostic-routing.log`
recorded native touch IDs 172, 175 and 176 sequentially, with maximum `active=1`,
frame contacts=1 and preview contour count=1. Each entered Ink and submitted
one canonical operation. The total canonical count was four because the first
stroke used the mouse fallback. Its DOWN had `promoted=1`, `source_type=4`
(touch), `origin=1` (hardware), and signature `extra=4283520937`; subsequent
messages reported mouse-global capture on owner HWND 527800. There was no
overlap, viewport claim, WM_TOUCH or WM_POINTERCAPTURECHANGED in that run.
No pointer/touch/mouse messages were logged at the overlay, but gesture
messages were not instrumented. The missing second contact's HWND is unknown.

That run is `BLOCKED_INPUT_ROUTING`. Shared Runtime is not established as the
root cause, so Android, Web and shared Runtime behavior remain outside this fix.

## Window contract defect and candidate correction

The preview popup previously used `WS_EX_TRANSPARENT` and returned
`HTTRANSPARENT` without being a layered window. Microsoft's
[WM_TOUCH documentation](https://learn.microsoft.com/en-us/windows/win32/wintouch/wm-touchdown)
states that touch does not honor HTTRANSPARENT. The
[extended styles documentation](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
defines WS_EX_TRANSPARENT alone as a painting-order flag. These mechanisms
therefore did not establish the popup's intended input-transparent contract.

The candidate adds `WS_EX_LAYERED` and initializes constant alpha to 255 using
`SetLayeredWindowAttributes`, retaining WS_EX_TRANSPARENT. Microsoft's
[window features documentation](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features)
describes system hit-test transparency for layered transparent windows;
[DirectComposition permits a layered target window](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondesktopdevice-createtargetforhwnd).
The owner retains its existing touch registration. The popup does not register
touch or pointer targets, and this change adds no input capture.

The confirmed defect is the incomplete popup transparency contract. Whether it
fully explains this machine's lost second touch requires a real Axiom trial;
the source and control probe alone do not prove that causal link.

## Regression and verification

`G4InkPlaygroundWindowsPreviewPopup` runs the actual D3D12/Skia provider and
queries the created popup through native Windows APIs. It failed before the
change because layered transparency was not initialized. After the change it
passed, including surface acquisition and presentation before and after resize
and overlay visibility changes. It neither injects touch nor proves two-finger
drawing. Local evidence: `windows-preview-popup-native-red.log` and
`windows-preview-popup-native-green.log`.

The Windows executable and requested test targets built using the locked
prebuilt Skia SDK; Skia was not rebuilt. The relevant CTest run, including the
new test and three existing interaction tests, passed 22/23. The existing
`G45BrushLab` manifest comparison failed. The requested three Python contract
files plus diagnostic contracts passed 42/47. Five existing source-text
expectations in `windows_skia_preview_contract_test.py` still fail:

- `test_windows_preview_uses_runtime_skia_provider`
- `test_windows_hides_preview_presentation_before_canonical_redraw`
- `test_windows_final_pointer_up_completes_canonical_handoff_immediately`
- `test_windows_owner_surface_has_presentation_only_amber_fallback`
- `test_windows_amber_fallback_applies_runtime_viewport_transform`

Logs: `windows-build.log`, `windows-layered-ctest.log`,
`windows-layered-python-contract.log`. Physical trial log:
`pointer-diagnostic-layered.log`. Physical trial is pending; no functional
checkpoint or G4/G4.5 Gate PASS is asserted.

A full Python run initially stopped during collection because Windows cp1252
could not decode an Android source file. Repeating with `python -X utf8 -m
pytest -q` completed: 458 passed, 31 failed, four skipped, 209 subtests passed.
All failures, including semantic/compiler subtests, are retained by name in
`windows-pytest-full-utf8.log` and `windows-verification-summary.json`; they
were not repaired or reclassified as passing in this Windows input change.

The initial full CTest invocation had 49 unbuilt tests and the BrushLab failure.
An attempt to build all configured targets stopped at deprecated
`SkiaInkBackend` uses in `skia_ink_viewport_test.cpp` and
`skia_brush_primitive_test.cpp`, treated as errors by the existing /WX policy.
See `windows-build-all.log`. No deprecated-API warning was disabled and no
Runtime source was changed to obtain a green full suite.
