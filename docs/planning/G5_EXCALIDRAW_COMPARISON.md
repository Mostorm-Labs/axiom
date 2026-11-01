# Excalidraw 累积笔画性能：源码核对与 Axiom 对照

核对日期：2026-11-01。固定上游 revision [`8bcdca4526867433f99fdfcb55a063668db0c9dc`](https://github.com/excalidraw/excalidraw/commit/8bcdca4526867433f99fdfcb55a063668db0c9dc)。以下是代码事实与对 Axiom 的推论，未跑 Excalidraw 与 Axiom 的同机竞速 benchmark，也不声称 Excalidraw 任意对象数都流畅。

## Excalidraw 实际做法

1. **静态内容与新元素分层。** `StaticCanvas` 通过 `React.memo` 比较元素、可见集和相关 AppState，避免输入/界面变化无条件触发静态重绘。新建元素有单独的 `renderNewElementScene`，只绘制 `newElement`；静态、新元素绘制都有 RAF throttle。[StaticCanvas](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/excalidraw/components/canvases/StaticCanvas.tsx#L103)、[new-element renderer](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/excalidraw/renderer/renderNewElementScene.ts#L20)、[static throttle](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/excalidraw/renderer/staticScene.ts#L608)。
2. **可见性筛选与 memoization。** Scene Renderer 用 `isElementInViewport` 过滤后交给 static renderer；部分派生列表按输入 identity memoize。当前 `getVisibleCanvasElements()` 本身仍遍历 `elementsMap.values()`，不能从“只画可见对象”推导出“查询与全局对象数完全无关”。[Renderer](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/excalidraw/scene/Renderer.ts#L198)。
3. **不变元素复用 shape 和 raster。** `ShapeCache` 的 WeakMap 保留生成的 shape；`elementWithCanvasCache` 的 WeakMap 保留每个元素的 canvas。普通显示走缓存的 `drawImage` 路径；export 保留直接绘制分支。缩放、theme、device scale、render environment、image crop、containing-frame opacity 等变化会触发对应缓存重建，shape invalidate 也清 canvas cache。[ShapeCache](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/shape.ts#L87)、[canvas cache validity](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/renderElement.ts#L724)、[normal render cached branch](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/renderElement.ts#L1251)。
4. **缓存尺寸有上限。** 每元素缓存按 element bounds、zoom 和 DPR 建立，最大面积/边长超限时降低缓存 scale。源码同时明确这些阈值取决于浏览器/设备；这不是全应用统一内存预算，也不能直接复制为 Axiom 的 authority。[cappedElementCanvasSize](https://github.com/excalidraw/excalidraw/blob/8bcdca4526867433f99fdfcb55a063668db0c9dc/packages/element/src/renderElement.ts#L223)。

## Axiom 当前为何越来越卡

Ink v2 已降低**单笔 authoring**的历史重算/数据体积，但 canonical 层仍把多笔当作直接重绘输入。这是两个维度：当前笔画 knot 数 K，与文档已有笔画数 N。

- 每次新笔画提交后，生产 `EditingSceneCompiler::compileDelta()` 仍要求 full rebuild；`RuntimeScene::prepareIncremental()` 也重新投影全 source。数据紧凑并不会自动消除这些全局访问。
- `DirectReferenceSource::build()` 对每个可见 ID 调用 RuntimeScene lookup。旧实现线性扫描，V 个可见对象、N 个总对象时这一步约为 V×N，随后还复制内容和生成 canonical bytes。
- Ink v2 canonical `drawCompactStrokeToSkCanvas()` 每笔都遍历完整 device clip 的 256px coverage tiles。coverage 是 CPU raster，material 再合成到目标；最终目标是 WebGL/GLES 并不表示中间 coverage 都在 GPU 执行。
- 旧路径连笔画完全不覆盖的 tile 都创建 surface、清除并执行 material。新增一个对象会使旧可见对象再次执行这套流程。

本轮复现（Chrome/ANGLE Metal Apple M2，800×500、round-handdrawn@1、每笔 8 samples，CDP 原生 mouse）：32 个对象时 pointer move 约 .5ms、up 同步提交约 2.6ms，up 后 canonical callback 约 779ms。它是 Runtime/浏览器 receipt 延迟，不是 input-to-photon。全部原始事件、真实 object counts、WASM hash 与 renderer 均保留。

## 对 Axiom 的采用边界

已经具备 canonical/preview 独立 provider，继续保持。不能让 Web JS/Android/Windows 单独实现 geometry/cache semantics，也不把 Excalidraw Canvas2D 移为第二个 renderer。

固定同源三轮 Web 复测并未证实稳定 callback 改善（全笔画 p50 中位数 197.6ms → 200.0ms）；先前 779ms/333ms 不同产物诊断不能作为收益证据。详见 `G5_INK_V2_LOCALITY_RESULTS.md`。

首个切片消除 RuntimeScene 逐 ID 全表扫描，并保留原 clip-anchored grid 地跳过空 coverage tile/空白 shader 区域；同一材质算法对照独立旧绘制 oracle。未增加常驻缓存或更改 brush 输出。

后续按 G5 authority 顺序推进：

1. Foundation：消除单对象编辑的无关对象遍历/复制；Scene dependency closure 与 full oracle parity，区分真实 changed 与全局 fallback。
2. Spatial/GeometryChunk：查询可见/候选片段，长笔画不中途重置 seed、pressure 或 prefix；Chunk 不等于 raster Tile。
3. Render-owned Damage：changed region 与本视图 invalidation；不要因 preview dirty 重画 canonical。
4. RenderGroup/RasterCache：缓存未变的绘制产物，按 content/transform/clip/order/resource/scale/device validity 检查；有界、可清除，miss 回退 Direct，不用 global document revision 作 cache key。
5. Tile、Scheduler、ResourceBudget：在 correctness、coverage/handoff 和局部化计数成立之后再做，统一预算/调度。

这些对应 Excalidraw“只重算变化、缓存不变结果”的原则，但 Axiom 还必须保持跨平台 Skia、Operation/Scene identity 与 CanonicalVisible 精确 receipt，不能把旧缓存/amber preview 当作新 canonical coverage。


## Zoom cache policy follow-up

The same pinned `renderElement.ts` lines 739–746 explicitly suppress zoom-driven
bitmap regeneration while `appState.shouldCacheIgnoreZoom` is true. The cached
bitmap is resampled during that gesture; the following `canSnapElement` comment
also documents that behavior. When that flag clears, a changed zoom can rebuild
the per-element bitmap. This does not prove zero settle work or a fixed maximum
frame time in Excalidraw. Its capped element-sized canvas and reuse of shape data
keep a different cost profile from Axiom's CPU compact coverage/material work.
The Axiom follow-up in `G5_SETTLE_PREPARATION.md` preserves exact Direct output
and spreads independent group rebuilds across owner frame opportunities; it does
not claim Excalidraw pixel quality or performance equivalence.
