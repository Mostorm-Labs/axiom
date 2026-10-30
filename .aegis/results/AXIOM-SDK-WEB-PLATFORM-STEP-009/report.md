# Web platform SDK adapter and external consumer — scoped result

Base: e89d47686f5bc7077fb612b55a4f0af1634625f1. Tested source bytes and raw results are bound in manifest.json. The installation profile is Emscripten Release with the locked Semantic and Skia SDKs. The external runner copied a verified SDK and consumer outside the checkout, installed the package, configured, linked and ran in Chrome. See raw/external-final.json for exact commands, environment paths, exit codes, pixel values and negative configuration controls.

The shared adapter owns frame and surface coordination around caller-owned contexts, including GPU fence feedback and recovery. Demo uses the same adapter. External Chrome exercised synthetic pointer input at DPR 2, preview and canonical GPU pixels, resize, real WEBGL_lose_context events for both contexts, rebind, font catalog, synthetic Chinese text and shutdown. No real touch, native IME, physical presentation or visual acceptance is implied. Host applications still own DOM event and IME wiring.

Web selection: 19/19 and 28/28 on the shared adapter, and 45/45 on final code. Three additional host suites passed; actual context recovery passed. Native focused CTest: 12/12 passed. Two older Debug UI tests, web_fit_tool_test and web_sticky_fractional_test, failed on both the modified build and an independently built git-archive of exact base e89d; they remain FAIL and were not diagnosed or suppressed. Their raw observations are included. Baseline build logs identify its source and toolchain.

Full SDK-01, physical acceptance and release are not claimed. Existing independent D07 Protobuf/Abseil OOM blocker is unchanged.

Large raw browser JSON files are gzip-compressed without modifying their contents; decompress them to inspect the original observations.
