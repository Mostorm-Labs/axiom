# ADR-0001: Canvas v2 是 Visual Document Runtime

- Status: Accepted
- Date: 2026-08-16
- Related stages: POC-01～06, R1～R5
- Superseded in part by: ADR-0025（Persistence/Sync/Collaboration 的数据侧 owner 与产品平台矩阵）

## Context

目标能力包含结构化文档、RichText、多类 Ink、搜索、Comment、外部 surface、多视口、协作和持久化。把项目定义为“Skia Renderer”或“白板绘图库”会让 Document、Editor、Text、Ink 与 Cache 继续以 renderer 特例增长。

## Decision

Canvas v2 定义为 C++20 Visual Document Runtime。Runtime 正式拥有 Document、Operations、EditorSession、RichText、InkEngine、SceneCompiler、RuntimeScene、Layout、Geometry、HitTest、FrameGraph、Compositor、TileCache 和 Resources。

本 ADR 初版将 Persistence 列入 Runtime 的职责；该历史归属已由 ADR-0025 在产品路径中部分替代。当前 Runtime 通过 Persistence/Sync/Resource ports 提供语义事件和恢复输入，Shared Data Runtime/外部 data layer 负责具体的 DocumentSession、durability、offline/sync、blob 与存储编排；这不改变 Visual Document Runtime 的语义 ownership。

Skia 是可封装的 GFX backend。Product Shell 负责业务 UI、账户、分享、导航和平台服务，不拥有核心文档语义。

## Consequences

- 需要比单一 renderer 更严格的模块和状态边界。
- V1 功能面必须受控，但类型和 extension boundary 需支撑长期能力。
- Runtime 可服务 Web、Windows、Android、iOS、iPadOS、ChromiumOS 和 headless target，并保持
  macOS core/Web-reuse conformance。
- 功能不能通过在 Shell 中复制 Document 模型快速实现。

## Validation

六个 POC 分别验证共享 Runtime、Ink、100K Scene、RichText、Hybrid Surface 和 FastInk。若多个目标平台无法共享 Document/Operations/Scene 语义，必须新增 ADR 重新评估边界。
