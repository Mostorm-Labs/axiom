# Codex：开始实现 Selection Batch 1

用户已确认 Lasso／矩形框选／统一选择设计，并要求按小步实现推进。请读取同包 01～03 文档后，在独立分支开始第一批实现，不再只返回另一份规划，不要求完整 FC-08 302 变体 runner 或未来对象全部完成才开始编码。

## 基线

Repository：Mostorm-Labs/axiom。
本包已核对的 main：`2fee75f66a7f96f6aac834228ede372a9abda59a`，包含 Section PR #155。
建议新分支：`codex/selection-unified-batch1`（若重名先检查，不覆盖他人分支）。

先检查 status、当前 HEAD 与 origin/main。以该提交或包含它的相关已集成后继开始；main 前进则只核对受影响选择代码。不得回到旧 Section bootstrap 基线；不丢弃用户未提交工作、不自动合并 main。

本包是实施计划，不伪装成已通过机器预检的 Aegis package。若仓库需要记录执行方向，按本次用户确认做最小必要增量，不伪造审批或测试，也不要将仍未实现的最终验收能力重新放成第一行产品代码的前置条件。真正的语义冲突只停止受影响部分并指出精确冲突。

## 真实缺口，不要重新猜测

- SelectionSession::marquee 只过滤传入 candidates，不是实际框选工具。
- HitTestService::selectable 不做几何计算。
- toggle 不是 Shift Add；cancel 当前是清空选区，旧测试依赖它。
- setSelectionMode(false) 清选区，切 Hand 经过该路径。
- selectionPointer 空白 Down 立即清选区，缺少点击/拖动区分。
- PlatformPointerSample／PointerSample 没有 modifiers；需补 Host 到 Runtime 的传递。
- 控制层无矩形／自由套索子模式。
- 多个属性/变换和 overlay 只读 primary；多选后不能误装成批量编辑。

## 执行顺序

1. 扩展正式选区 Replace/Add，保持同一 SelectionSession；追加不 toggle。加入修饰键归一化测试。
2. 实现小型 RegionSelectionSession 与 editor 层区域查询。Move 只预览；Up 一次提交；取消不改变仍有效原选区；陈旧结果不覆盖新选区。
3. 接通现有 acceptPlatformBatch→dispatch 路径。Select 空白拖动矩形；显式矩形/自由套索从对象上也能开始；保留单对象变换、Section boundary、连接点及 Text/Crop 独占。
4. 接通 Facade、Debug UI、Windows/Web 真实修饰键和 transient overlay。选择模式切换与 Hand 往返保留选区。
5. 使用真实 Shape fixture 验证 ellipse/triangle 的 AABB 空角、旋转、凹/自交套索、相交与包含、Shift、取消、迟到 Up、删除、缩放/DPR 与 source lost。
6. 验证纯选择不修改 canonical 状态、Operation count、History 或 redo。多选下未支持的编辑在 Runtime/UI 明确不可用，不能只改 primary。

优先复用已存在且正确的旧 Lasso 代码；如果只存在未合入分支，核对后局部移植，不整分支盲合。没有现成代码就实现最小版本，不重复建设输入、Scene 或选区框架。

## 范围

具体核心文件、新增文件和测试名称见 03。首批在基础 Shape 上完成真实输入到选区的闭环，并复用可证明正确的已有 provider。其他对象缺口保留为显式 coverage/Partial/Unsupported，不默默跳过，也不冒充全对象完成。

本批不做完整多选属性编辑、Group/Ungroup、全量统计面板、Snap/Alignment 新特性，不更改 Section 类型／Operation 17/18 或历史 descriptor lock，不改旧 oracle 使测试变绿。

## 完成标准

按任务写失败测试、实现、运行、提交。必须在已配置构建中实际运行，不把 CTest 零匹配当成功。Windows 与实际 Chrome/WASM 分开记录；缺环境是该平台 NOT_RUN，不假称跨平台验收完成。

提交回执应包含精确 base/head、文件、真实命令/退出码、可复现交互、对象覆盖、失败与未执行项。原生/浏览器测试不是仅调用 marquee 的 checker。不要签发 Gate PASS，不自动 merge main。

直接进入第一批代码实现；非阻塞的命名、局部拆分和工程参数按仓库惯例处理并记录，不为每个小决定返回再开设计阶段。
