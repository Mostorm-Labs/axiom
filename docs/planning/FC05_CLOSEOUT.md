# FC05 产品功能收尾

日期：2026-10-25（Asia/Shanghai）。仓库：Mostorm-Labs/axiom。
状态：**USER_ACCEPTED_FOR_MAIN_INTEGRATION**。这是当前产品基线的收尾入口，不是 P34、DEV、PHYS 或 G5 PASS。

用户原文：“现在这个connector功能不错，基本已经完成了，将fc05进行整理后收尾吧，然后合并回main”。
这授权整理并合入 main，确认当前 Connector 的总体使用结果；没有把未逐项观察的触摸、IME、DPI 或
跨平台专项改为 PASS。Sticky 较早已由用户确认并标记 `fc05-sticky-complete`。原 FAIL 和 RED 不改写，
本次反馈单独保存在 [human-feedback.json](fc05/evidence/closeout-20261025/human-feedback.json)。

## 本轮交付的产品基线

- Sticky：方形便签与既有 RichText 子对象；选中空便签的 Type text 提示、双击编辑、左上开始输入。
  字号在移动/手动缩放时保持，容器内换行与省略号；仅选中且溢出时显示扩展按钮。
  输入扩展与点击扩展按方形向右、向下增长。移动/缩放中的文字、光标、按钮使用同一布局和变换。
- Connector：直线和自动避障正交线。选中 Shape/Sticky 的外部连接点支持悬停、点击和拖出；
  点击或拖至空白可创建同类型对象与连线，绑定相对侧端口，一次提交。
  支持 Rectangle/Ellipse/Triangle/Image/Sticky 的既有目标元组；端点使用真实边界。
- 绘制与拖动：中线折线、无遮挡时优先轴向直连；三角形上下端口跨逃逸候选选优。
  连续 Move 真实渲染帧保留折线，与 Up 后的路径一致；对象移动时连接点隐藏，结束后恢复。
- 路由基础版：多障碍物、可取消和累计预算；目标及第三障碍物变化后更新派生结果。
  Ready 路径共享给绘制、bounds、spatial、hit 和箭头。Pending 是正交示意而非避障已通过；
  无路由/超预算有独立回执和诊断，失败创建不留下部分对象。
- 公共消费者：RuntimeFacade 提供 create/read Sticky，create/read/switch-routing Connector，
  包括按完整对象 ID 读回路径/状态；MinimalRuntimeFacade 与 Debug UI 走同一 Runtime。
  文本、格式、外观、选择与 History 复用既有入口。宿主不另建文本测量或路由算法。
- Windows D3D12 与 Web Minimal Host 均已构建并用于测试。继续消费锁定发布 Semantic/Skia SDK，
  没有重建 SDK 或修改 SDK locks。

## 入口

F12 → Control → Connector / Sticky。New connector routing 选择 Straight 或 Orthogonal；
选择已有连线后，可在 Selected connector routing 切换并 Undo/Redo。
Select 选中对象，点击/拖动黄色外部连接点；上下与左右使用相对侧端口。
Sticky 使用同一文本编辑与格式入口。旋转手柄位于右上方。

当前产品源码基线是 `301b8138e7deae8ac5b12fc84cc6ec0b8340f922`，证据随执行分支
`codex/fc05-p32` 提供。精确收尾文件摘要、本轮测试和集成 main 的基线在
[closeout 证据目录](fc05/evidence/closeout-20261025/) 记录；最终合并事实以对应 PR 和 origin/main 祖先关系为准。

## 验证入口与已知缺口

- [Sticky/Connector 起始执行快照](../../.aegis/results/FC-05-DEV/result.json) 是旧 revision 的历史记录，
  其中早期实现/工具链阻塞不代表当前状态；原文件保持不变。
- [正交基础版](FC05_ORTHOGONAL_BASE.md)、[交互跟进](FC05_ROUTING_UX_FOLLOWUP.md) 与各自版本的
  RED、自动结果及产物摘要均保留；最新三角形与预览证据位于 `fc05/evidence/triangle-ports/` 和 `live-polyline/`。
- 本次重新执行选定的语义、History、Sticky 文本、路由、普通消费者、Debug UI、FC02/FC03 和 Scene 回归，
  同时运行 Python、native/WASM 语料与实际 Chrome 合成回归；结果由本轮 JSON 和原始日志给出。
- `canvas_g2_a3_full_incremental_equivalence` 的旧 Image 摘要期望仍不包含 FC03 已增加的字段，
  保留该失败；没有为了收尾而更新历史期望或把整体测试写成全绿。
- 本轮补充的 FC03WindowsImageDebugUi 曾在连接点像素上误判裁剪背景。现在采样同一背景区域内、
  连接点覆盖之外的像素，并同时断言 Done 前有背景、Done 后背景消失；保留前后证据。
- main 已有 FC03 显式初始化修复，本分支对齐其 optional/context 初始化写法后验证，避免整合覆盖。

## 后续功能与统一验收

1. Sticky 更多颜色类型/预设；Connector 曲线、手工折点、线路整理等增强另行安排。
2. 套索、Frame、Group、复合 Selection 等后续工作沿已具备的基础对象推进，本轮不启动。
3. G5：局部候选查询、缓存失效粒度和大型场景调度/内存/帧耗时。当前路由使用保守 world AABB，
   上限 256 个相关障碍物、8192 个搜索状态/记录检查、65536 个碰撞谓词；密集场景可明确超预算。
   当前仍有完整 Scene 投影和不可变场景快照，不宣称 G5 或任意规模可用。
4. 真实 Windows/Web 触摸、笔、原生 IME、DPI/多显示器及完整组合操作逐项验收，未逐项观察的保持 PHYSICAL_PENDING。

## 冻结合同与当前实现的边界

本次用户产品确认不自动撤销 P20/P31 的证明义务，也不把候选 API 当作已经交付。
旧候选的 Sticky 向上增长已由后续用户确认的方形右下增长取代；原候选及冻结包保留。
当前 Sticky 子对象创建、文字提交、显式扩展可以是多个操作/History 步骤；现有回归明确覆盖这些步骤，
不能等同于冻结候选中的专用 EditSticky 聚合单事务。
当前公开 Connector API 已交付创建、读回和切换路由；冻结候选中的专用端点重连/完整请求重试回执矩阵
没有在本次收尾中被宣称全部完成。上述合同/验证收敛、独立产品 Gate Review 另行保留，不伪造 P34/DEV PASS。

历史 Gate、Authority、Golden、PHYS 和 `.aegis` 状态/结果不改写；本次是用户接受的产品基线整合。

## 本轮最终复核

Windows/Web 产物重新构建通过；44 组 Native/WASM 路由输出完全一致。Chrome 合成回归中，
普通画板 13 组、文本 7 组均通过。Native CTest 156/157 通过、0 skipped，保留上文已知 Image 摘要失败；
Python 17 项通过、0 skipped，Runtime 边界检查覆盖 96 个文件。
精确构建 revision、命令、exe/JS/WASM 摘要及原始输出见
[artifacts.json](fc05/evidence/closeout-20261025/artifacts.json)。最终证据提交只增加文档，不重新标记物理验收。
