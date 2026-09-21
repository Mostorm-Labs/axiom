# GT-G4-5-SKIA-PARITY implementation context

Current flow: BrushRuntime already emits renderer-neutral BrushPrimitive artifacts. Windows has a SkiaInkBackend, while Android currently paints in `InkPlaygroundView` with `android.graphics.Canvas` and the Web playground paints through HTML Canvas2D. The Web repository also contains a WebGL2 Skia surface backend and locked Web/Android Skia SDKs, but neither is the programmable-DAB playground path.

Target flow: `BrushDefinition -> BrushCompiler -> BrushRuntime -> BrushPrimitiveDAB -> common Skia programmable-DAB renderer -> platform surface (Android Skia/RGBA bridge, Web WebGL2 Skia)`. Platform adapters may provide surface/lifecycle glue only; they must not recreate brush semantics.

Owners/files to inspect: `runtime/ink/**`, `runtime/render/**`, `apps/ink_playground/platform/android/**`, `apps/ink_playground/platform/web/**`, existing Skia SDK/CMake integration, and focused tests under `apps/ink_playground/tests/**` and `runtime/**`.

Preserved starting work: the seven uncommitted Android/Web parameter-unification edits in the worktree are accepted as starting modifications and must remain behaviorally compatible; they are not evidence of Skia parity.

First incomplete action: resolve package-local bindings, record ImplementationDesignPreflight and RED oracle, then add parity acceptance tests before production renderer changes.
