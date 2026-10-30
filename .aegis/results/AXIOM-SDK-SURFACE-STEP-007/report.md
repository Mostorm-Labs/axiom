# Public surface metrics and Windows frame path — intermediate result

The owner-thread public Composition exposes the actual view/surface and metrics generations and physical/logical dimensions without exposing render internals; other-thread reads return no value. Windows mouse, touch and text coordinates use this readback, and Web surface identity synchronization consumes it. The initial RED compile showed this public method was unavailable.

Windows production host no longer compiles the old direct frame/resize/paint fallback; existing white-box test targets retain it under their qualification define. Frame status exposes whether a pending handoff count is available; logging says unavailable rather than inventing a zero. This does not remove remaining private diagnostics and hover calls or complete SDK isolation.

UI-on Windows build and 12 tests including G3WebSurfaceHost passed; UI-off Windows adapter build and test passed. Installed outside-repository Windows HWND consumer passed using actual public metrics, resize and loss/recovery. The actual Chrome/WASM demo suite passed 45 assertions. Exact commands and observations are in raw. These use synthetic input and provider presentation, not physical touch, IME or scanout.

Web still has no independent installable platform adapter or outside-repository browser consumer. Windows host logging/hover still uses private Core and the full Web/Windows vertical closure remains incomplete. D07 OOM risk remains independent. No SDK, Gate, release or PHYS PASS is claimed. Dirty tested source is pinned by manifest hashes and the containing commit.
