# R1/R2 follow-up — PHYSICAL_PENDING

Exact product/build revision: `1f00471f2ffeefd42e20d88e3796ce04f4e58023`.
All 9 executable/page/JS/WASM file paths, lengths and SHA-256 values are in `artifacts.json`. Native executables and WASM were rebuilt from a clean checkout of this revision; they are local artifacts, not committed binaries.

Windows Minimal with Debug UI:
`C:/Users/Chado/.codex/worktrees/fc00x-scene-resource-composition-p32-f08-resume/out/p32-f08-msvc1951/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe`.
Press F12 for Debug UI. The no-Debug-UI executable uses the same filename under `out/p32-f08-msvc1951-off/apps/minimal_runtime_host/`.

Manual preparation: draw a brush stroke, draw/select/move a shape, Undo/Redo, resize/maximize/restore, minimize/restore, and change monitor DPI where available. Check preview-to-canonical handoff and control usability after rebind. Do not launch a historical Playground/currentXX binary. No manual observations are claimed by this repair.

Web debug-on:
```powershell
python -m http.server 8787 --bind 127.0.0.1 --directory out/p32-web-debug-on/apps/axiom_canvas_demo
```
Open `http://127.0.0.1:8787/index.html?r1r2=1f00471f`, disable browser cache and compare Network response bytes to `artifacts.json`. Repeat on 8788 with `out/p32-web-debug-off/apps/axiom_canvas_demo`.
Await `axiomG310Ready`, draw, resize with fractional/DPR2 metrics, lose and restore the surface. Current page keeps the existing narrow brush/lifecycle interface; S4 product controls are excluded, so debug-on/off files may match.

R1 public contract: preserve `opportunity.canonicalWork` from the actual frame opportunity and echo it with actual outcome/evidence/time to `receivePlatformPresented`. Do not reconstruct identity from current ViewId or counters. WebSurfaceHost mapping callers must retain `submittedCanonicalWork(platformFrame)` at submission and echo it with feedback. The old bare Composition feedback overload was removed; stable C exports are unchanged.

R2: a terminal callback during preparation aborts the candidate and prevents subsequent-role preparation/activation. Closed/Quiescent does not turn into Lost on late resize/lost events. Automatic hidden-window D3D12 resource tests are distinct from visible/manual observation.

Windows/Web manual status remains PHYSICAL_PENDING. Android/APK remains DEFERRED.
