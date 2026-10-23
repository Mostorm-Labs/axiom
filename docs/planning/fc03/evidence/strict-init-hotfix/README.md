# FC03 strict initialization repair

Baseline: `178173a03acd6f458991a69227ce3fdccaaffedd`.

Clang 22.1.8 with VS 2026/MSVC 14.51.36231, `/MT /W4 /WX`,
`-Wextra -Wpedantic -Werror`, syntax-only `/Zs`.

The aggregate probe using the baseline ImageCropUpdateRequest fails with
`-Wmissing-field-initializers` for contentScale. The same probe passes with its
default member initializer. Compiling the Windows image Debug UI test then exposed
another missing initializer for chooseImageFile; zero-initialize the context and
assign its RuntimeFacade pointer explicitly. No warning flag was weakened.

Fresh verification: the probe passes, and canvas_runtime_image.cpp, control.cpp,
fc03_image_facade_test.cpp and windows_image_debug_ui_test.cpp all pass strict
syntax compilation. Commands and output are recorded in strict-results.json.
A subsequent fresh Release /MT MSVC /WX configure and linked build also passed
for axiom_fc03_image_facade_tests and axiom_minimal_windows_image_debug_ui_tests.
CTest: FC03ImageFacade and FC03WindowsImageDebugUi both PASS (2/2, zero skipped).
Build, configure, resolver facts, offline resolution, commands and raw CTest output
are preserved alongside this record. CMake emitted object-path length warnings for
unbuilt unrelated targets; the selected targets completed successfully. This is not
a physical-platform observation or an all-CI-green claim.

Existing CI failures remain separate: G4.7 Windows configuration fails at the
FC02 test's unavailable GTest::gtest_main target; the platform Windows boundary job
uses GNU and fails configuration. No claim is made that these jobs are repaired.
