# FC-03 Authority 对齐记录

状态：**REPO_LOCAL_SUCCESSOR_MATERIALIZED / READY_FOR_INDEPENDENT_CONTROL_REVIEW**。

## 对齐结果

- 仓库 `execution:FC/feature-completion` 已从 `fc-feature-completion-execution-v0.2` 迁移到 `fc-feature-completion-execution-v0.3`。
- v0.2 保留为 `Superseded`，没有改写其内容或历史 Gate/Evidence/Integration 记录。
- v0.3 精确引用 Notion P23 Current 页面 `3f74c57a590c812a8260dcf23b659ed0`，依赖 `g4-capability-authority-v0.3` 和 `fc01-shape-triangle-semantic-v0.1`。
- v0.3 只承接 Notion P23 已接受的 progressive selection/provider 条件和 FC-04/FC-08 design-only permission；不授权跳过 FC03 自己的 P10–P20/P31/P34。

## ImageContent 状态确认

仓库镜像 `docs/notion/authority/04-semantic-schema/01-object-schema/image-content-v1-release-v0.1.md` 声明 `frozen-by-v1-final-gate`；对应 Notion 页面 `3c44c57a590c81948870c4f266a39861` 的正文状态仍为 `Proposed Freeze`。这是来源状态差异，不是允许实现者重新解释字段的授权。

FC03 采用以下保守处理：

1. 继续消费仓库已经存在的 ImageContent V1 字段与现有 Operation/codec/validator；
2. 不新增 ObjectKind、Operation kind、Proto tag 或改变 `source_rect`/`content_mode` 语义；
3. 将 Notion/仓库状态差异保留为 P21/P22 governance finding；
4. 在该 finding 未被独立审查改判前，FC03 只能以当前仓库字段实现，不能发布新的 Image semantic Authority。

## 证据与非声明

验证命令使用 pinned Aegis v0.6 CLI `Mostorm-Labs/aegis@00d3690ca2cdde15b3f919c54ccc8ef6290f55ba`：baseline `VALID`，successor `TRANSITION_OK`，successor `VALID`，生成 state 后 `STATE_OK`。

本记录不声明 FC03 P31、P32、P34、DEV 或 PHYS 通过；它只完成 repo-local Authority materialization，并把剩余 control review 明确化。
