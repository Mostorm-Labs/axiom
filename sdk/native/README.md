# @mostorm/axiom native binding boundary

The native binding is intentionally split into two layers:

- The application-facing TypeScript `Controller` uses the shared async contract
  in `sdk/api/axiom.d.mts`.
- A platform transport implements `request(command,args,bytes)`, subscriptions,
  and close against one `sdk::Session` owner. The reference transport uses the
  existing JSON vocabulary and never exposes `RuntimeComposition`, Scene, Skia,
  or a native pointer to JavaScript.

`AxiomCanvasView` is a separate Fabric host. It owns the platform surface,
input batches, resize/lost/rebind and frame receipt. It attaches to the same
opaque session as Controller; unmount releases the surface and cancels contacts
without closing the document. The production platform implementations are
tracked separately for RNW/D3D12, RN macOS/AppKit+Metal, iOS/Fabric+Metal and
Android/Fabric+EGL/GLES3. The POC05 files are reference evidence only.
