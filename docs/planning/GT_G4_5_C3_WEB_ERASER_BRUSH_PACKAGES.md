# GT-G4-5-C3 Web Eraser and BrushPackage Amendment

This amendment is scoped to the Common Runtime and Web/WASM surface. It does
not change the C2 `vector-solid-v1` authority, descriptor locks, G3 snapshot
authority, or the Windows Skia preview branch.

It freezes two additional package selectors (`marker-flat-v1` and
`chalk-grain-v1`) and three Runtime tool modes (`Brush`, `Object Eraser`, and
`Partial Eraser`). Package material is resolved in Runtime; platform code only
submits `PlatformPointerBatch` and presents Skia output.

Object erasing dispatches `DeleteObjects`. Partial erasing dispatches
deterministic `SplitStrokes` fragments for `vector-solid-v1`, and
`AddEraseMasks` for marker/chalk. Fragment generation is Runtime-owned and
never implemented in a platform adapter. Mixed profile hits remain
fail-closed until independent operation application is wired.

The implementation target for this amendment is
`READY_FOR_WEB_PHYSICAL_VERIFICATION`. Host tests and WASM build evidence do
not constitute a physical browser PASS.
