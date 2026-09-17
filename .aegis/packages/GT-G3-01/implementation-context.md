# GT-G3-01 implementation context

G2 already owns canonical shared `RuntimeScene` state. This task establishes only the first G3 per-view Render Core foundation so two views can bind the same scene generation while keeping camera, viewport, surface, metrics, and frame identity independent.

Create the minimal immutable types and `RenderViewRuntime` holder listed by the package. Do not implement visibility, draw lists, backends, Skia, presentation tracking, platform hosts, or any later G3 work package.

First action: add the G3-01 contract test and CMake target, run the frozen build command, and record the expected RED before adding the missing production headers/types. Continue only through the GT-G3-01 closure contract.
