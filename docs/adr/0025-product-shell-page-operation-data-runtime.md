# ADR-0025：产品 Shell、Page/Document、Operation 与数据运行时再基线

- Status: Accepted
- Date: 2026-08-23
- Related gates/stages: AR-0, G0～G9, R1～R5
- Supersedes: ADR-0002 与 ADR-0015 中的平台产品矩阵；ADR-0014、ADR-0019、ADR-0020、
  ADR-0022 中把 `Document transaction` 当作 canonical 外层的解释；此前单 Page / 多 Page
  未决项
- Clarifies: ADR-0001、ADR-0003、ADR-0023、ADR-0024

## Context

POC 阶段已经证明共享 C++ Runtime、跨端渲染、RN/Fabric Overlay 和 Arc 边界可行，但仓库
正式文档仍混有三个时期的假设：Windows React/Tauri、Apple 仅作可移植性验证；
`DocumentRoot → Page*` 仍待决定；以及 `Operation` 之外另有全局 canonical
`Transaction → operations[]`。这些假设会让产品 Shell、保存/同步、Undo、Schema 和公开 ABI
分别沿不同方向实现。

本轮重新核对产品设计后，需要在进入生产 Gate 前先给出一个明确、可执行的组合决定。它只
冻结职责、所有权方向和语义方向；Shared Data Runtime 的实现语言与物理模块 owner，以及
具体 wire 字段、Bridge/ABI layout、数据库、线程拓扑和协作冲突算法仍由对应 RFC/Contract
与 Gate 证据决定。

## Decision

### 1. 产品平台与 Shell

| 平台 | 产品 Shell | Canvas 数据面 |
| --- | --- | --- |
| Web | React + TypeScript | WASM + WebGL；Pointer 批量进入 WASM，不经过 React state |
| Windows | React Native for Windows | Native Canvas/Surface + C/C++ bridge；屏幕批注使用 Native Overlay Host 特例 |
| Android | React Native | Native CanvasView/JNI；高频输入、IME 核心和渲染不逐事件经过 RN JS |
| iOS / iPadOS | React Native | Native Canvas component/ObjC++；高频输入、IME 核心和渲染不逐事件经过 RN JS |
| macOS | 暂缓 native 产品 Shell | 当前用户从 Web 产品进入；保留共享 Core 的构建/回归能力，不设 native 发布门禁 |

ChromiumOS 继续复用 Web；Headless 继续作为 reference/test target。POC-01 的 macOS/Apple
证据仍是有效的可移植性证据，但不自动产生 macOS 产品承诺。POC-05 的 RNW、Android RN 与
Apple RN/Fabric 证据是当前 Shell 决定的输入；其 POC-only bridge 仍不得直接成为产品 ABI。

### 2. 一 Page 一 Document

- 一个 Product Page 恰好对应一个独立 Axiom `Document`。
- Axiom Document 内没有 Page ObjectKind，也没有 `DocumentRoot → Page*` synthetic root。
- 每个 Document 独立拥有 identity、revision、Operation history/frontier、ResourceManifest、
  digest、Snapshot 和恢复边界。
- Page 集合的引用、标题、顺序、创建/删除/复制、导航、thumbnail 和生命周期由产品层拥有；
  Shared Data Runtime 可以保管 `PageId → DocumentId` repository 数据，但不能把 Page 集合
  重新塞入单个 Axiom Document。
- 每个 Page/Document 是允许负坐标、连续 pan/zoom 的无限工作区；export/content bounds
  不限制编辑空间。

### 3. Operation-only canonical mutation

- `Operation` 是 Document 唯一的 canonical mutation 和持久/同步语义单位。
- 单个 Operation 可以携带领域内需要原子发布的多对象 payload；实现必须执行完整
  `decode → normalize → validate → idempotency → prepare apply plan → atomic apply → publish`，
  失败或重复输入都不得暴露部分 Document，也不得改变 revision 或 digest。
- 上述内部边界称为 **Atomic Operation Apply**，不再称为独立的 canonical
  `Document Transaction`。
- `OperationBatch` 只是一次传输、回放或 ABI 摊销的容器，不天然拥有跨 Operation 原子性。
- Command/Intent 与 Undo group 属于 EditorSession；它们可以产生一个或多个 Operations，
  但不是 wire、Snapshot 或协作中的第二层 canonical fact。
- 历史 POC 的 NDJSON transactional batch、`TextTransaction` 与现有代码中的 transaction
  计数可以保留为实验实现事实；产品迁移不得继承其语义，后续在各自 Gate 中替换。

### 4. Shared Data Runtime 与 Platform Host

- Shared Data Runtime 是中性的数据侧职责边界，负责 DocumentSession、repository、Snapshot /
  Operation continuation 的 custody、LocalStore、Outbox/Inbox、Revision/Cursor、Sync、Blob 和
  cloud client。TypeScript 是来自当前 Notion 设计的候选实现方向，不是本 ADR 冻结的语言。
- Axiom 继续拥有 Document、Operation、Snapshot 和 digest 的语义及 codec 规则；Data
  Runtime 通过后续 RFC 定义的窄 Data Bridge / Runtime ports 保管和传送 opaque handle/bytes，
  不依赖 Scene、Tile、Surface、Pointer 或 Arc 内部类型。本 ADR 不冻结 `AxiomDataBridge` 为
  最终包名、物理 ABI 或公共 SDK 表面。
- local apply event 与 remote/replay apply 分离；外部 apply 不回显为新的 local outbox event。
- `Platform Host` 是架构级 composition root 角色。`Host Runtime` 只表示可能承载该角色的候选
  物理模块；本 ADR 不冻结 `Host Runtime`、`Axiom Host Runtime` 或其他包名、target 名与发布
  单元。Platform Host 编排一个 Canvas 区域的 Axiom、Arc、surface、frame scheduler 和
  overlay，不拥有整个产品窗口，也不复制 Axiom/Data Runtime 的语义状态机。

### 5. 产品能力与 Arc

- Connector、Group、Sticky 进入当前产品 Schema；Frame、PDF 以及已确认的编辑能力进入实施
  路线，但尚未由现有 Schema release 完整覆盖的部分必须先完成兼容 Schema/行为评审，不能
  伪装成已实现或已冻结。
- Lasso、Align/Distribute、Smart Snap 是 G4 必做交互能力。
- 擦除固定三条语义路线：对象擦除删除完整 Stroke；细矢量笔部分擦除生成确定的 segment
  fragments；粗笔、Dab/texture 笔部分擦除生成 object-local Pixel/Dab erase mask。Brush 到
 策略的 resolver 必须版本化并有 golden/replay 证据。
- Arc 是产品硬需求，负责 transient low-latency preview；任一 backend 不可用或失败时，
  Platform Host 必须切换为 Canonical-only / inline preview，不得阻断输入、Operation commit 或
  Canonical Renderer。

## Consequences

- Windows 产品路径从 Tauri 改为 RNW；iOS/iPadOS 从 portability harness 提升为产品目标；
  macOS native 产品化暂缓。
- Product Schema 不再为 Page hierarchy 付出持久化、协作和迁移复杂度；跨 Page 产品行为在
  上层协调多个 Document。
- API、Snapshot、OpLog、Sync 和 Undo 文档中的 `Transaction` 必须按域迁移；数据库事务等
  storage-local 用语不受影响。
- Shared Data Runtime 的数据侧职责成为 G7/G8 的正式产品边界；具体实现语言（包括是否采用
  TypeScript）、物理 owner、包名、Bridge ABI、codec、数据库、AXTP wire 和 conflict algorithm
  仍不是本 ADR 的结论。
- Notion 中标为 Draft、Current Direction、Proposed Freeze 或 Open 的字段不会因本 ADR 被
  自动升级。G0/G1 必须把已声明的 Schema/golden 重新物化为仓库可复现证据。

## Validation

- 文档 lint 禁止在现行产品路径重新出现 Windows/Tauri、`DocumentRoot → Page*` 或全局
  canonical Transaction；历史/POC说明必须显式标明其范围。
- G1 验证单 Operation 原子拒绝、Reference/production store 等价、Snapshot + continuation
  replay 和 Page-free Schema。
- G3/G4/G6/G9 分别验证 RNW、Web、Android RN、iOS/iPadOS RN 的产品边界；macOS 不作为
  native product gate。
- G4 对三类擦除、Arc→Canonical-only fallback、Selection/Transform 和真实笔输入建立
  deterministic + physical evidence。
- G7/G8 先通过 RFC 冻结实现语言、物理 owner 和 Bridge/包边界，再验证 opaque bridge、
  no-echo、crash recovery、离线队列和同步故障；Open conflict policy 只能使相应 Gate
  `BLOCKED`，不能由实现者临时决定。
