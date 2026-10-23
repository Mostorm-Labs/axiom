# FC05 Connector 与现有 Shape 兼容合同 v0.1

状态：**产品范围已确认 / P12–P13 合同已物化 / 待独立设计审查**。
本合同不声明代码已经实现、P20 已冻结、P31/P32 或 DEV/PHYS 通过。
实现基线：`178173a03acd6f458991a69227ce3fdccaaffedd`。

## 1. 决策与生效范围

用户明确选择“支持全部现有 Shape，并更新合同”。本合同把 FC06 原先延期的
Triangle attachment 和既有 Shape 版本连接能力纳入 FC05 首版；同时承接此前已确认的
直线＋自动避障正交折线，以及 Sticky 底色＋RichText＋移动缩放＋History＋连接目标。

角色：P12/P13 版本与操作合同。Authority：上述用户决策、既有 Connector V1
端点/anchor 合同与 FC01/FC02 已发布 Shape 几何/标签模型。
目标：连线能连接用户当前可创建的 Shape，添加标签或 Undo 不破坏引用。
非目标：增加 ObjectKind/Operation/Proto 字段、曲线、手工折点、Connector label、
跨对象通用事务、新 Registry、历史 Golden/Gate 改写。
所需分析/输出：精确 tuple、解析数学、版本转换、reader 准入与原子拒绝。
质量门槛：旧数学与历史字节保持；新合法性由版本化后继验证证明。
交接：先独立审查本合同，再收敛 P14–P18/P20；本文件本身不是实现包。

## 2. 精确 Connectable 集合

所有记录先通过现有完整结构/资源/层级校验，再按以下集合决定 connectability：

| ObjectKind | kind_version | 内容限制 | 几何 |
| --- | --- | --- | --- |
| Shape | 1 | shape_kind=1 Rectangle 或 2 Ellipse，label absent | 已有矩形/椭圆解析 |
| Shape | 2 | shape_kind=3 Triangle，label absent | 本节三角形解析 |
| Shape | 3 | shape_kind=1/2/3，合法 present RichText label | 基础 Shape 几何；忽略 label bounds |
| Image | 1 | 合法 ImageContent；display width/height > 0 | display box；crop/bytes/decode 状态不改变资格 |
| Sticky | 1 | 合法 StickyContent；width/height > 0 | Sticky box；RichText child 不承担 attachment |

未知版本/shape_kind、Shape/1 Triangle、RichText、Stroke、VectorPath、Group、Connector
保持拒绝。不能用“同 ObjectKind”或“已有 render bounds”自动支持未来版本。
Shape/3 的 label 内容有效性仍归 FC02；空 label 不改变其基础几何和连接资格。

## 3. 保持的端点、坐标与锚点语义

- FreePoint 是 **Connector object-local f64**，通过 Connector world transform 解析。
- AttachedEndpoint 保存 target ObjectId 和 AnchorRef；目标变化只更新 derived endpoint。
- Auto hint present 使用目标 local box normalized [0,1]^2，并排除精确中心 (0.5,0.5)；
  absent 使用另一 free point 的 world 坐标或另一 attached target 的 world box center。
- 先逆目标 world transform 得到 local 参考点，再减 C=(W/2,H/2)；精确零方向取 +X。
  不递归求另一端 perimeter，不把 hint 误作 world point，不用 label/pixel/stroke 边界。
- 旧矩形/椭圆的解析公式、hint presence inequality 和端口 namespace 保持。
- 每个 Shape 新版本显式发布其 port namespace；0/5..max 仍未分配，拒绝且不退回 Auto。

## 4. Triangle analytical perimeter

沿用既有正立三角形 canonical vertices：A=(W/2,0)、B=(W,H)、D=(0,H)。
W,H 为有限正数。方向原点沿用 V1 **box center C=(W/2,H/2)**；不切换到质心。

将 local ray 写成 normalized coordinates：

```text
u(t) = 1/2 + t * dx/W
v(t) = 1/2 + t * dy/H
q = (dx/W, dy/H),  t >= 0
triangle interior:  2u+v >= 1,  -2u+v >= -1,  v <= 1
```

C 严格处于三角形内部。求每个将离开半平面的正边界时间并取最小值：

```text
left:   if 2*q.x+q.y < 0:  tL = -0.5 / (2*q.x+q.y)
right:  if -2*q.x+q.y < 0: tR = -0.5 / (-2*q.x+q.y)
bottom: if q.y > 0:       tB =  0.5 / q.y
others: +infinity
P_local = (W*u(min(tL,tR,tB)), H*v(min(tL,tR,tB)))
```

方向为零时先用 +X；角点并列使用 left/right/bottom 固定次序作为 edge identity，
几何点不变。浮点实现需防溢出/非有限值，不得把异常结果发布成成功路由；可以用
与该 ray 相同的正数缩放方向以稳定求交，不用 epsilon 改写 canonical intent。
world 映射在 local 解析之后发生；非均匀 scale/rotation/skew 不改变 anchor。

Triangle/2 和 Triangle/3 的 ports 明确为 **从 box center 沿四个 cardinal rays 的交点**：

| port_id | 含义 | Triangle local point |
| --- | --- | --- |
| 1 | Top | (W/2,0) |
| 2 | Right | (3W/4,H/2) |
| 3 | Bottom | (W/2,H) |
| 4 | Left | (W/4,H/2) |

不能把 Triangle Right/Left 放在 box 外边中点 (W,H/2)/(0,H/2)。
Shape/3 Rectangle/Ellipse ports 则保持原 box midpoint/cardinal ellipse 位置。
本节定义 attachment point；正交出线方向、端口 escape、障碍 clearance 仍归路由合同。

数学例：W=200,H=120 时，ports 为 (100,0),(150,60),(100,120),(50,60)。
hint=(1,0.5) → (150,60)；hint=(1,1) → (200,120)；coincident absent → (150,60)。

## 5. 操作、标签版本变化与 History

InsertObjects/RestoreObjects：按 **最终 staged state** 校验全部 target tuple 和 ports；
允许 target+Connector 同批，非法任一引用整项拒绝，不能部分创建或丢弃关系。
SetConnectorContent：仍为 whole-content replacement，start 合法/end 非法时整项拒绝。

对已附着 Shape 添加首段确认文字，会按 FC02 既有 guarded EditRichText 把 /1 或 /2
升级成 /3。ObjectId、基础 shape_kind、transform、placement、ConnectorContent 保持。
Undo 可以恢复 /1 或 /2 的 absent-label 前像，Redo 恢复同一 /3 与标签。每个结果版本均
在上述集合内；依赖图使几何变化刷新路由，但不得另造 SetConnectorContent/History。
若 FC02 文字增长策略同步改变基础几何，按目标的最终几何重新解析，不追踪 label bounds。

任何未来操作会把已引用目标改成不支持的 tuple，必须在 **相同 staged validation /
prepared apply 边界** 检查 incoming Connector references，失败时保留全量前像。
不能在发布之后靠 renderer 修补，不 silently detach、不删除关系来让操作成功。
当前不授权 ShapeKind 转换；本条不新增转换入口。

移动/resize、字体/格式变化、资源到达、route cache refresh 不写 Connector canonical。
Cascade delete 和 Restore 的既有完整闭包、Undo 捕获规则保持。
所有公共入口、远端 apply、Undo/Redo、typed bootstrap 使用同一语义规则，Lost/Closed
写准入和失败无副作用义务不变。重复 OperationId 继续走既有去重。

## 6. 序列化与旧 reader/writer

Connector/1、AnchorRef variants、port IDs、routing=1/2、Operation envelope、Proto tags
均不变。不因本次能力扩展自动增加 schema number，也不假装旧 reader 已具备新能力。

新增组合能力使用现有 CapabilityRequirementsV1.required_ids 的后继条目
`connector-shape-attachment-v1`：实际文档中存在 AttachedEndpoint 指向 Shape/2 或
Shape/3 时必需，其他情况不额外要求。schema3 使用现有 ASCII 排序唯一的编码、
digest/checkpoint framing；与 triangle、shape label、font/manifest 的现有需求取并集。
这是现有能力集合的窄扩展，不新增 registry 模块或 proto field。

旧 schema1/2 的原字节/原记录继续按旧规则；含上述新组合不得伪装成 legacy snapshot。
最小 schema3 容器也是为了可表达组合能力，而不是 Connector 字段升级。新 reader 同时
具备目标自身能力和组合能力才可原子恢复；缺任何能力则整个文档拒绝，不丢 Connector
或降级 Shape。旧 reader 如只支持 Triangle/label 仍不能写这个组合。

恢复 continuation/远端 writable admission 同样检查该能力，不能仅靠 SnapshotCapability
旧值或 operation envelope=1 推断。Operation log 中即使当前 snapshot 已删除新组合，
只要待回放区间可能创建该组合，就需先具备新能力再发布 Ready。无协作协商 owner 的
宿主必须 fail closed，不能声称客户端兼容或自建第二套 Sync 协议。

新增能力条目/typed reader 支持必须在 P20/P31 锁定；此处没有为生产 reader 设置默认值。

## 7. 版本化验证与保存历史

增加 FC05 独立 fixtures：所有上述 tuple/端口、hint absent/present、变换三角形，
非法 tuple/port，target+Connector staged insert/restore，connected Shape label
upgrade→Undo→Redo，以及缺组合能力的 restore/continuation 原子拒绝。

冻结 V1 bytes 和 SH-G06 保持。旧 Shape/2 attachment-negative 仍是旧 Authority 的历史
结果；版本化 successor 应有独立 fixture/expected，不抹除旧 Gate/Evidence，不把旧 FAIL
改为 PASS。实现阶段只更新明确引用后继合同的当前测试期望，并保留 baseline RED。

独立数学 oracle 从 vertices/half-plane 得出边界，不调用生产 resolver。图形/文本 pixels
不是 attachment oracle。P20 需区分 descriptor unchanged、语义接受矩阵和真实平台接线。
