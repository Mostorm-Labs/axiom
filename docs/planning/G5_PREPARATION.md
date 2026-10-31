# G5 前置核对与性能采样准备

执行范围：用户授权的第 1、2 项准备工作。此记录不授权 G5 优化 P32，不替代原 Gate、独立 DEV、FC-13 实机验收或 P34。原有 `.aegis`、descriptor 和依赖 lock 均保持原样。

## Authority 与准入

- [G5 Implementation Plan v0.3 CURRENT](https://app.notion.com/p/3f64c57a590c816e8e8ae42f396a3ff2)：执行路线与 P23 acceptance；先做 WP-G5-00 reconciliation/fixture/profile freeze。Performance 数字仍需按被接受的 profile 冻结。
- [FC-00 P23 acceptance](https://app.notion.com/p/3f64c57a590c81faa2c8ea0097aad6c5)：历史验收不能替代后续原 Gate/physical 记录。
- [FC execution v0.4](https://app.notion.com/p/4044c57a590c81a8b2aec982fad25107)：已取代 G5 页面链接的旧 FC policy。FC-08 的依赖排序例外不取消 FC-12/13 和 G5 前置。
- [Public Facade ownership supplement](https://app.notion.com/p/3f44c57a590c813e9546e6cfbc9f27ac)：保持输入 source-loss 隔离与 camera→frame→overlay 一致性。
- `docs/notion/manifest.yaml` 是仓库 authority 导航；不能用目录里存在旧设计来推断新能力授权。

本次审阅 HEAD 为 `a0b20fbb0d31b6da0a536eb5f63fc7594468f415`，同时有另一项 Ink v2 工作修改同一 worktree。实际采样以工具生成的 commit、dirty 标记、逐文件 SHA256、SDK artifact hashes 为准。

| 检查项 | 检查到的本地记录 | 可以得出的结论 |
|---|---|---|
| 原 G4/G4.4/G4.5/G4.6/G4.7 | `.aegis/gates.json`、各 G4.x result | registry 到 G4 completion；后续完整独立 acceptance 尚未绑定到本审计 |
| G4.5 final vector | `GT-G4.5-FINAL-VECTOR-CLOSURE/result.json` | readiness；记录明确不宣称 G4.5/P34 PASS |
| PX0-07 | `PX0-07/result.json` | `BLOCKED_MISSING_INPUT`，Windows/Web physical pending |
| G4.6 | B control review 与 C result | B 的有界能力结论不能替代原 G4.6；C 为 `BLOCKED_EVIDENCE` |
| G4.7 | `GT-G4.7/result.json` | hosted jobs success、待 combined physical；`g5_authorization=false` |
| FC-01..12 独立 DEV | 发现 FC-01/03/05/08 等记录 | 有实现和测试记录；不从目录名称推断 12 项均独立验收 |
| FC-13 original + integrated physical | 未在检查来源中绑定完整 receipts | 需要原始与集成构建的准确 revision、artifact、physical review |
| 集成 ancestry | 审计记录存在的 revision 是否在 HEAD ancestry | ancestry 只能证明历史包含关系，不能证明 acceptance 或当前输出正确 |

结论为 `BLOCKED_UNVERIFIED_PREREQUISITES`，含义是本审计未闭合这些绑定，不能推断全项目没有远程验收。后续需要控制面提供准确 acceptance refs 与 integrated revision，再做 WP-G5-00 正式准入。

## 已存在的 owner 与参考路径

当前生产链为 SDK root facade → private Session → RuntimeComposition → CanvasEditingRuntime → BrushSession/OperationEngine → Scene coordinator → FramePlan → SkiaRenderer → canonical/preview providers。旧 playground 不是当前生产入口。

| 层 | 当前参考/owner | 本轮处理 |
|---|---|---|
| Canonical mutation | OperationEngine、IndexedObjectStore | 通过现有 owner；不新增 store 或提交路径 |
| 独立存储 oracle | `runtime/semantic/src/reference_object_store.cpp` | 保留、记录 hash；源码存在不等于本次完成 differential 验证 |
| Scene oracle | `runtime/scene/src/full_scene_compiler.cpp` | 永久保留 |
| Spatial oracle | `runtime/scene/src/linear_spatial_index.cpp` | 永久保留 |
| Direct non-tiled | `runtime/render/src/direct_reference_source.cpp` | 永久保留 |
| 绘制 | `runtime/render/src/skia_scene_renderer.cpp` 与 headless backend | 使用现有共享 Skia helpers |
| Damage | 当前 `runtime/scene/src/damage_tracker.cpp` | 与 G5 要求的 Render owner 尚有差距；留到 WP-G5-08，准备阶段不迁移 |
| 编辑增量编译 | `EditingSceneCompiler::compileDelta()` | 当前显式 requires-full-rebuild；保留为性能排查项，不改 recovery semantics |
| GeometryChunk/RenderGroup/Tile/Scheduler | 未找到完整生产实现 | 不以现有 preview chunk 代替 G5 通用 GeometryChunk；不提前实现或宣布完成 |

Web normal path 已使用独立 WebGL2/Ganesh targets，并通过 GPU fence 完成 Runtime receipt；正常 present 没有 RGBA readback。`sdk/web/axiom.mjs` 仍逐 fact JSON serialize/copy/call/parse。它们是采样归因点，不是未经测量的瓶颈结论。

## 本轮闭包与 API 影响

仅新增 verification 工具、profiles、测试与说明；不修改 SDK exports、公共 DTO/ABI、Runtime owner、Brush 算法、provider 实现、依赖或生产热路径。

必需验证：审计不得提升 DEV/readiness 为原 Gate；缺失 receipts 明示；source drift 与 artifact mismatch 拒绝基线；固定输入保留全部 samples 和 terminal；percentiles 保留尾部；不把不可观测指标写成零；真实 root SDK consumer smoke。

补充一个仅限 `vector-solid-v1` 的原生 BrushSession component runner，以现有 owner 分别测量 load/resolve、append、seal、fresh replay，并记录历史复制计数；它与 Web consumer 结果分开，不能当作 SDK→Skia 全链路耗时。

终态：准备工具可执行、核对表可复查；测量成功为 `MEASURED`，不叫 Gate PASS。源码并发变化为 `SOURCE_CHANGED`；构建、浏览器、receipt 或 artifact 失败为 `BLOCKED_EVIDENCE`。失败必须保留日志与 SHA，不更改 production 来绕过。hosted/physical 非本轮必需，也不从 host/browser smoke 推导其他平台结论。

## 测量边界

`verification/corpus/performance/g5/profiles.json` 记录固定 seed、笔画长度与 repeat；同时列出未来的 1K/10K/50K/100K、1M engineering、text-heavy、dense-grid、font cold/warm、FC-12 mixed 义务。大画布 corpus 尚未实现/验收，这轮不能报告其完成。

浏览器 runner 复制同一次构建的 root JS/declarations/WASM/data/adapter files 到独立临时 consumer，通过 Chrome CDP 原生 mouse input 走真实 DOM collector。每个样本等待 CDP ack，每八个样本等待 RAF；适合比较同一 workload 的软件路径，不能冒充真实 240Hz 输入或压力设备。保留所有事件和 warmup 原始记录。

记录 pointerdown/move/up 同步 dispatch、RAF intervals、LongTasks、up 到 canonical callback、两份 canvas identity、WebGL renderer、browser version、source/build/artifact identity。up dispatch 包含 seal、Operation apply、Scene projection，不通过相减伪造内部 attribution。

精确 Brush append/seal、Scene projection、GPU execution/upload bytes、preview present counters、input-to-photon 没有 root SDK observable seam 时输出 `null + reason`。canonical `onFrame` 是 Runtime completion callback，不是 preview present 或物理显示。像素截图放在计时区间之外。

默认不强制 SwiftShader，实际 renderer 必须随结果保存；可显式选择 SwiftShader 做功能对照，其数字不能作为硬件 GPU 基线。source 在 configure/build/run 前后需一致，SDK files 必须和 browser 实际复制的 hashes 一致。

运行方式见 `verification/runners/g5/README.md`。生成物写入 `out/g5-preparation/`，不把机器路径和原始 timing 写入 workspace manifest 或旧 Gate evidence。
