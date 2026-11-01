# Table/1 本地实施与验证记录

日期：2026-10-31。范围：用户确认的 `table-v1-design.md` 与
`table-v1-contract.json`。本地 SDK 版本 `0.1.0`；开发分支
`codex/sdk-shell-axis-bridge`，基线 `cbd62f80a46f8d30c850e98334a0e20e3019519a`。
本记录保存提交前的本地验证结果；提交身份以 Git 历史为准。本轮未执行 SDK 版本发布。
不是正式 Gate、Windows ABI 或物理 IME 验收。

## 已实现

- 原生 Table/1 叶对象：稳定行列身份、内嵌 RichText、固定行高与按列换行裁切。
- 创建、选格、编辑文字、Tab/Enter、增删行列、尺寸、填充与文字样式、网格样式。
- 复用 TextEditSession/原生输入事实、OperationEngine、发布、History、Snapshot。
- EditTable 的完整前像检查与分动作校验；无条件覆盖和跨动作夹带结构变化拒绝。
- 组合内创建遵循现有作用域；完整身份 token 防止将陈旧输入写入其他单元格。
- 共用 Scene/Text/Skia 路径进行展示、命中和 PNG/JPEG 导出。
- Web 根导出 `runtime.table`、TypeScript 声明，以及 Windows 共享 Session/C++ 包装。
- Demo 第一行表格入口、第二行单元格与行列参数；底部保持既有画板工具。
- 行列边界几何与 GridStyle/1 分离。后续手绘样式应显式扩展样式版本与能力要求，
  不能让旧 SDK 把未知风格当规则线条，也不需把网格拆成额外 Shape 对象。

## 本地证据

完整机器观察保存在 `out/table-v1/summary.json`，以下为检查入口：

- `web-final.json`：实际 Chrome/WASM 根模块消费者与 Demo；包含文字输入、
  双击、键盘、拖动尺寸/取消、样式、身份、Snapshot/字体资源恢复、远端不回声、
  冲突撤销拒绝、PNG/JPEG 及非法参数的原子拒绝。观察文件记录实际构建版本和文件摘要。
- `focused.log`：11 个 SDK/Table CTest 项全部通过；TableSemantic 内有 7 个实际
  Semantic/Runtime 测试。`action-red.log` 和 `group-red.log` 保存新增回归的失败阶段，
  对应修复后通过记录为 `action-green.log` / `group-green.log`。
- `browser-regression.json`、`background-regression.json`、`export-regression.json`：
  原有外部消费者、背景、图片导出的浏览器回归通过。
- `api.log`、`types.log`、`boundary.log`：JS 公开 API、严格 TS、Runtime 模块边界通过。
- `package.log`：17 项发布打包测试通过；`npm-files.json` 为本地 npm dry-run，
  包中没有内部 C/C++ 头文件、独立静态库或 DLL。没有执行 npm publish。
- `schema.log`、`descriptor-tests.log`：当前 descriptor 可复现，历史字段/枚举保留；
  descriptor 测试 6 项通过，1 项因测试环境未指定 protoc 而跳过；
  单独的 schema 验证已实际使用锁定的 host protoc 通过可复现检查。
- `fixture-regressions.log`：Projection/DirectReference/FrameOrchestration/Connector
  旧测试夹具修正后复跑；两个 G46C Qualification 项仍失败，归入下述历史债务。

## 全量回归的限制

已运行配置目录的完整 CTest（211 项）。该轮有 46 项失败或未运行；
所有失败名称均能在此前 `out/object-identity/image-export-full-ctest.log` 找到。
其中包括旧渲染/排版断言、缺失的 FC04 可执行文件与 Qualification 环境/观察失败；
这不是全仓测试全绿。对应完整日志是 `out/table-v1/ctest.log`。
本轮修过的旧编译夹具还单独复跑，未修改历史 golden 或已接受的独立 descriptor。

该次全量运行未通过的项目如下（保留原名，避免将未运行算作通过）：

- `FC00XRuntimeComposition`
- `FC01Creation-width-style`
- `FC04.MSE-01`
- `FC04.MSE-02`
- `FC04.MSE-03`
- `FC04.MSE-04`
- `FC04.MSE-05`
- `FC04.MSE-06`
- `FC04.MSE-07`
- `FC04.MSE-08`
- `FC04.MSE-09`
- `FC04.MSE-10`
- `FC04.MSE-11`
- `FC04.MSE-12`
- `FC04.MSE-13`
- `FC04.MSE-14`
- `FC04.MSE-15`
- `FC04.MSE-16`
- `FC04.MSE-17`
- `FC04.MSE-18`
- `FC04.MSE-19`
- `FC04.MSE-20`
- `FC04.MSE-21`
- `FC04.MSE-22`
- `FC04.MSE-23`
- `FC04.MSE-24`
- `FC04.MSE-25`
- `FC04.MSE-26`
- `FC04.MSE-27`
- `G3HeadlessCanvasDemo`
- `G45BrushRuntimePipeline`
- `G46BQueryStructure`
- `G46CQualificationBehavior`
- `G46CQualificationBundle`
- `G46PlaygroundTransform`
- `G47TextHost`
- `G4InkPlaygroundComposition`
- `G4InkPlaygroundRepeatedStrokes`
- `G4InkPlaygroundSurface`
- `G4MultiPointerPlayground`
- `G4ViewportGestureState`
- `SDKCppPublicMultiPermissionStale`
- `SDKCppPublicMultiStrokeWidth`
- `canvas_fc02_shape_label_scene`
- `canvas_g2_a1_runtime_record_projection`
- `canvas_g2_a3_full_incremental_equivalence`

## 使用与边界

测试地址：`http://127.0.0.1:4189/example.html`。点击“表格”，选行列后在画板点击放置；
双击格子编辑，参数行可调整行高、列宽、颜色、字号与网格。
普通表格编辑支持撤销重做；远端改变其前像后明确拒绝有冲突的补偿，不自动合并离线修改。
Snapshot 保存字体引用而非字体字节，恢复时通过资源接口补齐字体后即可正确渲染/导出。
首次只提供规则网格；手绘线条、合并单元格、公式与表格粘贴不在本轮范围。
Windows 真机、Windows DLL 外部消费者和物理中文 IME 未在本机执行，不据 Web 结果代为确认。
