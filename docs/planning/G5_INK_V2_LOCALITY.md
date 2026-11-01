# Ink v2 后性能基线与首个局部化切片

用户于 2026-10-31（本机日志可能跨入 11-01 UTC） 授权执行此前第 3/4 项，并要求定位“越画越多越卡”和分析 Excalidraw。起点为 `6c46f540`，仓库 `Mostorm-Labs/axiom`。本次消费 Ink v2 已提交语义、材质和算法，不重新设计数据格式或压力模型。

既有 G5 v0.3 路线继续约束顺序：Foundation/Spatial/GeometryChunk → Render-owned Damage → RenderGroup/RasterCache → Tile → Scheduler/ResourceBudget。用户授权本次实施工作不等于补发旧 Gate/FC-13/P34 证据。完整 G5 准入缺口继续独立列出，当前切片不宣称整个 G5 完成。

## Owner 与范围

canonical document/OperationEngine 不变。RuntimeScene 拥有派生发布记录；Render 私有 Skia helper 拥有 coverage/materialization；SDK root facade 继续调用同一 Runtime，不新增公开 API、并行 document、renderer 或平台手势逻辑。

第一切片冻结：

1. 固定已交付 Ink v2 revision、package/resource hashes 和已有工作量/缓存/材质证据；重跑其关键回归。
2. 测量持久化笔画数增长与视口内/外分布，分别记录 input、semantic/scene commit、FramePlan/Skia present 成本；保留原始样本、source/artifact identities。
3. 消除已定位的 RuntimeScene 每个可见 ID 线性扫描，保留准确 ID、原遍历顺序和原 publication gate/rollback。
4. 消除 compact material 在笔画覆盖范围之外分配/合成空白 tile 的无效工作；保持原 coverage shader、seed、paint、采样、tile grid、erase mask 和直接 reference。暂不引入持久化 raster cache，不选择新 spatial backend。
5. 保存 Excalidraw 当前固定 revision 的源码引用，对比 static/new-element split、viewport filtering、element canvas/shape cache；说明其仍有线性可见筛选与重绘成本。

不在本切片里静默增加 TileManager、RenderGroup promotion、thread pool、semantic/schema/SDK lock 修改。单对象 commit 仍有 full projection 时如实报告，后续单独交付有 dependency closure 和独立 full oracle 的增量化。

## 执行闭包

必需：失败先行的 ID lookup/空区域工作量 guard；旧/新 material exact pixel parity（多 family、scale/DPR/translate/clip、边缘/空/负坐标/巨长路径）；publication rollback/generation/lookup 测试；现有 InkV2 material/preview/lifecycle 回归；实际 matching Web SDK consumer；累积笔画 before/after 记录。

数据不能用生成的输入数冒充实际访问/分配次数；新增 counters 仅用于内部诊断，不进 consumer API。墙钟仅佐证，不冻结新产品 SLO。host raster 不替代 Web GPU 或真实设备。

成功：限定变更通过其必需 correctness/workload guards，固定源码重采成功，并明确剩余全局路径。阻塞：authority 冲突、必要依赖不可解析、独立 oracle mismatch、构建环境失败、source drift。保留日志，不伪造 Gate PASS。没有授权发布 SDK/tag/npm 或改历史 Gate。

## 当前实现与后续边界

`verification/runners/g5/native/locality_guards.cpp` 还观察实际 ObjectStore
`allObjects()` 调用和复制对象数；它的 `projection` 模式是诊断，绝不当作
no-full-scan guard 已通过。当前 1K/10K 文档单对象变更分别复制 1K/10K 对象。
层级、Group、Sticky child、connector routing、text resources 与原子 publication
依赖仍须完整闭合后才能把 `prepareIncremental` 换为 dependency-closed projection。
生产 `EditingSceneCompiler::compileDelta` 继续返回 full-rebuild；本切片未把这条
fallback 改成误报 incremental。

Material parity corpus 扩为 450 个 family/scale/path/transform 组合，包含
rotation、skew、reflection 与 perspective conservative fallback。它逐像素比较
独立旧 full-clip raster，固定原 tile grid、noise coordinates 和 shader。

最终实测、完整回归及剩余缺口在 `G5_INK_V2_LOCALITY_RESULTS.md`，旧 G5
preparation results 保留原时间和 scope，不重新解释成当前优化结果。
