# S1/S2 manual preparation — PHYSICAL_PENDING

Windows Debug UI on:
`C:/Users/Chado/.codex/worktrees/fc00x-scene-resource-composition-p32-f08-resume/out/p32-f08-msvc1951/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe`

Windows Debug UI off: the same executable name under `out/p32-f08-msvc1951-off/apps/minimal_runtime_host/`.
Compare exact paths and hashes with `artifacts.json`. These current binaries embed clean build ref `5ce43ead7883f8bdadb0f8540f78af4692a2aaa9`, an evidence-only descendant of product revision `7453884345a175f6aebad52225cfafaf337f2f3e`. Product files are identical at those two refs. Do not launch an older Playground/currentXX exe.

Press F12 for the existing common Debug UI. Exercise brush/shape creation, selection/move, Undo/Redo, repeated resize, maximize/restore, minimize/restore, and Windows DPI 100/125/200 where available. Confirm preview/canonical alignment and handoff continuity, retained content and usable controls after rebind. Try resizing while a brush handoff is pending. Missing device observations remain pending.

Web debug-on:
```powershell
python -m http.server 8787 --bind 127.0.0.1 --directory out/p32-web-debug-on/apps/axiom_canvas_demo
```
Open `http://127.0.0.1:8787/index.html?s2=5ce43ead`, disable browser cache and compare Network JS/WASM response bytes with `artifacts.json`. Repeat on port 8788 using `out/p32-web-debug-off/apps/axiom_canvas_demo`.

After `await axiomG310Ready`, use `axiomWebHost.resize(320,180,1.25)` and DPR 2, then `axiomWebHost.loseSurface()` followed by resize to restore. Observe drawing, preview and canonical positions, accepted surface/metrics identity and console errors. The existing page remains a narrow brush/lifecycle smoke interface; S4 Web product-control/debug-panel expansion is excluded. Debug-on/off may therefore emit identical page/JS/WASM; hashes record this explicitly.

Coordinate contract: per-monitor-aware Windows mouse client pixels convert once through accepted logical/physical dimensions; Web CSS samples already use logical coordinates. Runtime Scene, transient objects, brush preview, guides and selection chrome scale by physical/logical facts. DPR and displayScale are not blindly multiplied. Fractional physical rounding is a platform fact.

Actual binding changes preserve the existing selection-transform cancellation policy. Retained brush geometry stays in its accepted logical/world space, and old frame tokens cannot qualify new metrics. Zero-sized WM_SIZE is ignored as minimization, not bound as a legal 0x0 surface.

Windows/browser manual observations remain PHYSICAL_PENDING. The native D3D12 automated resource test is separate evidence. Android/APK is DEFERRED.
