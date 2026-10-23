# FC03 Windows Image Debug UI supplement

This supplement implements the user's explicit request to connect image import,
replacement and Crop to the current Windows Minimal Host Debug UI. It extends the
FC-03-P31 implementation scope to `debug_ui/**` and the small renderer MIME readback
needed by bytes-only import. The original package, Authority, SDK locks and historical
Gate/evidence records remain unchanged. This is implementation evidence, not an
independent review or Gate/PHYS verdict.

## Ownership and behavior

- `F12` opens Debug UI. `Control` starts with an `Image` section.
- `Import image...` opens the native Windows file picker for PNG/JPEG/WebP.
  The host reads file bytes and extension/MIME facts. RuntimeFacade validates the
  encoded image, registers its hash/length/dimensions through the existing Manifest
  operation, inserts the Image, and selects it.
- `Replace selected image...` requires one editable Image. It retains its identity,
  display size and Transform, switches resource/intrinsic dimensions, and resets Crop.
- `Crop selected image` enters a Runtime-owned temporary session. Drag the image
  interior or the visible full-source area to pan the image; drag edges/corners to
  adjust the source/display box. The preview renders the complete source image and
  dims only the area outside the crop frame, so a user can see and position the
  off-frame content. `Content zoom` accepts 0.25–4.0. Numeric inputs expose normalized
  source bounds and display dimensions. Source dimensions must exceed 1/4096; display
  dimensions must exceed 1 DU. Rejected values retain the previous valid preview.
- `Crop Done` creates one History step. `Crop Cancel` or Escape discards the preview.
  Crop previews change derived drawing and selection bounds, without writing canonical
  ImageContent. Surface loss, binding change, close and captured pointer cancellation
  discard the session.
- File selection runs outside ImGui drawing. Binding and replacement selection are
  checked again after the modal dialog returns. Debug UI holds only UI drafts and
  invokes RuntimeFacade; it does not construct canonical objects or operate D3D12.

Resource registration survives Undo so restoring an Image can resolve the same bytes.
Rejected MIME, corrupt bytes, invalid Transform and absent parent are tested before
Manifest changes. Registration and Image insertion are separate existing operations;
the tests do not prove rollback for every possible failure after registration.

## Validation

Local build: VS 2026 MSVC 19.51 x64, `/MT`, project `/WX`, locked release Skia and
Semantic SDK. Release optimization is `/O2 /Ob2`; standard test assertions remain
enabled. `SK_RELEASE` follows the selected Skia SDK variant independently of NDEBUG,
so asserted tests use the same inline Skia ABI as the published release archives.

Build directory: `out/fc03-p32-red`.

- 21 selected CTest cases passed, including FC03ImageFacade,
  FC03WindowsImageDebugUi, image Scene/resource, FC02 text and Windows touch/label
  entry, Minimal consumer, composition/facade, control router and Web Surface host.
- 17 Python descriptor/semantic/FC02 host/font checks passed, with the emitted
  Semantic environment imported; no skips.
- The ImGui regression activates real controls through Controller, Router and the
  Minimal RuntimeFacade. Only the file chooser is replaced by a byte fixture.
- Pixel readback verifies that temporary Crop changes rendering, Cancel restores
  original pixels, and Done/Undo preserve expected canonical state. The Windows
  Debug UI integration also verifies full-source Crop preview pixels, a dimmed
  outside region, panning from that region, and removal of the dimming after Done.
- RED was observed for strict Crop minimum rejection before enforcing the frozen
  limits. Prior bytes-only import, UI activation and negative-import REDs were also
  observed during implementation.

Selected CTest command:

```powershell
ctest --test-dir out/fc03-p32-red -R 'FC03|FC02|FC00XRuntimeComposition|FC00XRuntimeFacade|FC00XMinimalRuntimeHost|G47ImageResource|G3WebSurfaceHost|PX0ControlRouter|PX0Workbench' --output-on-failure --timeout 120
```

Python command (after importing `out/fc03-build-environment.json` environment):

```powershell
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract verification.tests.test_fc02_host_contract verification.tests.test_host_text_font_resource -v
```

New evidence is recorded separately under
`.aegis/results/FC-03-DEV/debug-ui-addendum/`. The current implementation snapshot
is identified by base HEAD, dirty state and changed-file SHA-256 values.

## Manual checks

The detailed matrix below remains **PHYSICAL_PENDING** except where an explicit
user observation is recorded in the closeout addendum; a general acceptance reply
does not independently prove every format/device/lifecycle combination:

1. Native picker: import PNG, JPEG and WebP (including a Unicode path); cancel picker.
2. Select an Image, replace it; verify preserved frame and Undo/Redo.
3. Crop: drag interior and each edge/corner, use zoom, then Done and Undo/Redo.
   Confirm the complete image remains visible outside the crop frame and is dimmed
   there while the selected frame stays at full brightness.
4. Repeat Crop with Cancel and Escape; verify original content returns.
5. Resize/maximize, lose capture and reopen the panel; verify no stale Crop input.
6. Check actual D3D12 presentation and touch interaction on the running application.

This supplement adds the Windows file-picker adapter. A browser file input and
browser physical verification are not included in these new evidence claims.

## Native picker follow-up (2026-10-23)

The user reported **FAIL** on the first build: the Desktop picker did not list
their JPG/JPEG. Read-only inspection confirmed that the Desktop path contained
`20261021-203735.jpeg`, a regular file with a JPEG header. The original semicolon
filter matches JPG/JPEG using Windows PathMatchSpecW, so the precise cause of
the shell enumeration failure has not been established by this automated test.

The picker now explicitly selects filter index 1 and browses all files by default,
with separate PNG/JPEG/WebP filters available. Validity is checked after selection:
the host reads immutable bytes and MIME signature facts, and Runtime still performs
the actual decode. Supported extensions inconsistent with bytes are rejected.
This also permits selecting an image with no recognized suffix without renaming it.

The added regression calls the actual picker configuration and file loader with
only the interactive GetOpenFileNameW call replaced. Windows wildcard matching
confirms that the default filter admits JPG/JPEG/uppercase/Chinese/extensionless
names. Real temporary files verify bytes and MIME, plus corrupt-content and
extension/MIME-mismatch rejection. The extensionless case failed against the
old configuration before restoration of the fix; it does not reproduce the
user's standard-JPEG enumeration failure.

Five focused CTest cases passed after the change, with assertions enabled.
The original user result remains **FAIL**; the new native file-list observation
is **PHYSICAL_PENDING**, pending the user's repeat in the new window.
Separate follow-up build/test/launch logs preserve the prior evidence.

## User acceptance and integration closeout (2026-10-23)

After testing the complete-image Crop preview in Windows build PID 261292,
SHA-256 `abcb4d55a75b2ccecbfa920f955230178ff7fc0ef8ce10f0b753fb7d1223719a`,
the user replied: “可以了 非常不错，可以收尾fc03了，把fc03的内容提交推送并合并回main吧，我们到后面统一验收”.

Record this as **USER_ACCEPTED_FOR_INTEGRATION** for the demonstrated Windows image
workflow/full-source Crop experience. Preserve the earlier picker **FAIL** and
its separate pending retest history. The user explicitly defers consolidated
acceptance, so this integration does not assign P34, DEV Gate or PHYS PASS, or
claim browser/device cases that were not observed.

Fresh closeout verification passed 24 selected CTest cases plus the separate
G46EditingOverlay case, and 17 Python tests with no skips. Results are stored in
`debug-ui-addendum/closeout-*` logs. FC03 is ready to integrate per the user's
request; remaining acceptance coverage is carried forward for consolidated review.
