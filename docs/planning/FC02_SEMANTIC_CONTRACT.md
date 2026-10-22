# FC-02 RichText：宽度、迁移、Undo 与 Snapshot 合同

2026-10-19；工作分支 `codex/fc02-runtime-boundary-design`。
审计源码：`d15acacceda1cf12f8d89846b15a4ec44dbd3cc0`；main 基准：
`ea135122f8e3266cafa433d69903f7950a5e6300`。

状态：**P12/P13 设计稿，供后续接口和实现采用的明确选择**。这里收敛上一轮的候选，
尚未发布新的 ObjectKind 版本、proto、registry、稳定 ABI 或验收结论。
用户交互以[产品设计](FC02_PRODUCT_AND_SEMANTIC_DESIGN.md)第一节为准。

## 1. 取舍与边界

| 问题 | 本稿决定 |
| --- | --- |
| 独立宽度存在哪里 | `RichTextContent.layout`，后继 `RichText/2`；不用 PropertyBag 或宿主字典 |
| 改宽度/迁移 | 现有 `EditRichText` 内的 delta2 布局步骤；同一 ObjectId、单次原子提交 |
| 精确 Undo | delta2 的完整、带校验的 RichText 状态恢复步骤；不依赖旧文字步骤可逆 |
| Snapshot | 复用 schema3 资源清单格式，组合能力准入；不新造 Snapshot4 |
| 旧数据 | 原编码和 V1 规则不变；普通文字编辑不自动迁移宽度 |
| 生产实现 | 先补严格准入、原子 apply/History 和兼容负例，再接会话/宿主 |

不增加 ObjectKind、OperationKind、全局 Transaction、Registry 子系统或并发文本合并。
现有 RichText 的 ObjectKind 为 4，EditRichText OperationKind 为 14；后继版本数字 2
及下述新增 tags 是本稿提出的分配，不能当作已接受 registry 使用。

## 2. 内容、宽度与旧格式

定义可持久化文本状态 `S = {kind_version, document, layout?}`。
`document` 复用现有 ParagraphId、段落顺序、run 边界和完整样式；位置单位仍是 Unicode
scalar。输入法、caret、selection、typing style、临时合成、自动高度和排版结果不进入 S。

| kind_version | document | layout | 合法性 |
| --- | --- | --- | --- |
| 1 | 合法 V1 文档，包括既有合法空状态 | 必须缺省 | 原格式，沿用 legacy 布局上下文 |
| 2 | 合法文档，沿用 V1 段落/run/style 规则 | 必须为 FixedWidthV2 | 显式本地换行约束 |
| 其它或上述字段不匹配 | 任意 | 任意 | 拒绝，不能猜测/回填 |

`FixedWidthV2.wrap_width` 是 binary64、对象本地坐标中的完整换行宽度；没有隐藏 padding、
canonical height 或宿主 viewport 宽度。高度由现有布局派生；超长不可分割内容使用既有
布局断行/溢出规则，不为此改字体或把 glyph 裁剪写入文档。

宽度采用明确的可表示性边界：`2^-126 <= W <= (2 - 2^-23) * 2^127`，上下界包含。
它们是 binary32 的最小正常正数与最大有限数，按 binary64 比较；NaN、Inf、零、负数、
越界一律拒绝，不 clamp。布局输入采用 IEEE-754 round-to-nearest, ties-to-even 转为
binary32；canonical 仍保存原 binary64，不能把排版取整结果反写。该范围是数值合同，
不是 G5 性能或内存保证；资源/排版失败仍须报告派生不可用。

本轮工具创建预设选 **320 本地单位**，只用于新草稿；首次有效插入必须把 320 或用户修改后
的实际值显式写入。改变未来预设不影响既有对象。旧对象当前布局上下文里的 128 不能
自动变成持久化宽度。拖拽创建不在本稿新增；初版宽度编辑固定本地左上锚点，数值输入或
右侧把手只改宽度，拖动 preview/取消均不提交，确认一个最终值提交一次。

对象缩放仍使用 Transform；zoom、DPI、surface resize 和窗口状态不写宽度 Operation。
布局、cache identity 和失效传播必须按每对象宽度解析，不能改共享全局宽度污染其它文本。

新草稿“非空”指至少一个 run 有一个 Unicode scalar（空格计入，单纯空段落不计入）；
未达到时换段仍留在草稿。样式来自 Runtime 工具预设并显式写入，没有平台默认字体。
进入可写会话前必须有合法的字体 manifest binding；缺失时先报告资源准备/准入失败，
不能因一次空草稿暗中提交资源 Operation。字体字节物化/缺 glyph 与绑定合法性分别报告。

## 3. Wire 增量与严格校验

下表是拟议 wire 分配，**没有修改生产 proto**。Edition-2024 singular presence 保留，
不能把缺省当零；现有 tags、V1 语义与 Brush payload2 保留。

| message | 拟议新增字段 |
| --- | --- |
| RichTextContent | `RichTextLayoutV2 layout = 2`；document 仍为 1 |
| RichTextLayoutV2 | `double wrap_width = 1`，必须 present 且有效 |
| RichTextStateV2 | `uint32 kind_version = 1; RichTextDocument document = 2; RichTextLayoutV2 layout = 3` |
| SetRichTextLayoutStep | `uint32 target_kind_version = 1; RichTextLayoutV2 layout = 2` |
| RestoreRichTextStateStep | `RichTextStateV2 state = 1` |
| RichTextStep.oneof | `SetRichTextLayoutStep set_layout = 7; RestoreRichTextStateStep restore_state = 8` |
| RichTextDelta | `bytes expected_state_digest = 3`；delta_version=1、steps=2 的 tags 不变 |

布局步骤目标 version1 必须没有 layout，目标 version2 必须有有效 layout。
Restore 的 state 按第二节验证。不能塞 ObjectRecord、任意 JSON 或可执行/native 数据。
新消息未知字段、重复 singular、缺失必需 presence、不合法 oneof 和未知枚举/版本按严格
codec 拒绝；typed 入口执行同样的值、版本与边界校验，不能绕过 wire 限制。

### Delta 准入

| 目标 | delta_version | 步骤与结果 |
| --- | --- | --- |
| RichText/1 | 1 | 现有六步骤，保留历史行为；不带新 guard |
| RichText/2 | 1 | 拒绝；不能让旧 writer 猜测新内容语义 |
| RichText/1 或 /2 | 2 | 六种文字步骤，或唯一布局步骤，或唯一 Restore 步骤；必须有 guard |
| 其它组合 | 任意 | 拒绝 |

delta2 不能为空。文字步骤可有多步，顺序作用于 staged 文档，沿用 scalar 规则；文字步骤
不能隐式改变 layout/kind_version。布局步骤、Restore 步骤各自独占 delta，不与文字或
Transform 拼成假原子编辑。新 FC-02 editor 对旧对象也写 **delta2 + guard**，但保持
kind_version1；历史 delta1 继续可读回放。旧数据没有被“编辑即升级”。

`expected_state_digest` 固定 32 raw bytes，不是十六进制字符串。定义：

```text
C = 按上述 RichTextStateV2 tags 严格规范编码的当前 S
D = ASCII("axiom.richtext.state.v2")
guard = SHA256(u32LE(len(D)) || D || raw16(DocumentId) || raw16(ObjectId)
               || u64LE(len(C)) || C)
```

C 对 V1 同样显式编码 kind_version/document，layout 缺省；必需 scalar presence、有限数值
规范化、最短 varint、tag 顺序等消费既有 canonical 规则。段落/run 顺序和边界保留；
**不合并相邻同样式 run、不删除合法空 run**。guard 不含 Transform、Placement、PropertyBag、
SemanticGeneration、资源字节或渲染状态。相同文本的另一对象不能复用 guard。
这是该 mutation 的内容前置校验，不替换或重命名 Document digest 算法。

## 4. 原子 mutation、迁移和重放

Operation envelope 保持 `schema_version=1, payload_version=1`。先执行现有完整 OperationId
去重/碰撞检查，重复已接受的相同 Operation 返回原接受结果，不因 guard 已变化而二次失败。
新 Operation 在既有写准入下检查对象存活、Kind/版本、guard、步骤与完整 staged 结果，
包括资源引用；任意失败不改文档、ledger、generation、History cursor 或发布 ChangeSet。
不能由宿主跳过锁定/只读准入，也不能因为称为 Undo 就免校验。

| 动作 | 一次 canonical 结果 | 可逆状态 |
| --- | --- | --- |
| 首次非空 confirmed 输入 | InsertObjects 插入含位置、段落/样式与宽度的 RichText/2 | Undo 删除；Redo 以既有 RestoreObjects 恢复身份/记录 |
| 修改文字/段落/style | EditRichText delta2，按当前内容校验并原子应用文字步骤 | Restore 完整前像 S0，guard 为接受后的 S1 |
| V2 宽度 W0→W1 | 唯一 SetLayout(version2,W1)，document 不变 | 唯一 SetLayout(version2,W0)，校验当前 S1 |
| V1 显式取得宽度 W | 同一 ID，唯一 SetLayout(version2,W)，document 不变 | 唯一 SetLayout(version1,layout absent)，精确恢复 V1 缺省 |
| Restore S0 | 只替换 document/layout/kind_version，保持其余 ObjectRecord 字段 | 可再 Restore 被替换的 S1；没有 Delete/Reinsert |

迁移是用户明确改旧对象宽度的结果；打开/保存/普通文字编辑/codec 解码不做此迁移。
回到 V1 不能把宽度置零冒充缺省。Restore 可以恢复合法的 V1 或 V2 前像，包括旧 ParagraphId、
段落样式、run 边界和空状态；这不是分配新的 paragraph 身份。目标对象已删除、类型改变、
guard 不匹配或前像不合法均拒绝；不恢复其它 Kind，不绕过 manifest 引用约束。

恢复与布局/迁移必须在同一个 prepared apply 中改变内容和 kind_version，并发布一个
相应文本内容/布局失效变更；中间状态不可见。ObjectId、Transform、Placement、PropertyBag
保持当前值，不把历史整对象覆盖到仍存活的目标上。

## 5. 文本 Undo/Redo 与证据

本轮实际运行的[最小诊断](fc02/diagnostics/rich_text_inverse_probe.cpp)调用当前 Semantic
normalizer、validator 与 `prepareRichTextDeltaState`。合法前像 `[A,S][B,S]` 经插入 X 再
删除 X 后成为 `[AB,S]`；文字相同、canonical run 边界不同。因此此前“反向文字步骤就能
精确恢复”的建议撤回。结果见[证据记录](fc02/SEMANTIC_CONTRACT_EVIDENCE.md)。

一次 confirmed edit 或最终宽度确认是一个 Undo 项；空草稿、composition update 和取消
没有项。初版不合并连续键入/多次合成的历史项。一次 replacement 的内部多步仍为一个
Operation。Undo 恢复 S0，Redo 恢复 S1，分别校验当前相反侧状态并使用**新完整 OperationId**；
也可在满足同一精确前后像条件时复用 guarded 原始 forward，不用旧 OperationId 重提交。
相同 Operation 重试沿用原 ID 走去重，不能误变成第二个用户编辑。

EditorHistory 写入前必须确认 forward/inverse 支持、取得规范前后像并准备必要的历史记录
存储/前置条件，再允许 Semantic apply；不能先 apply 再因 recordConditional 不支持而
terminate。提交失败不新增历史、不推进 cursor、不清空有效 Redo。原有 History owner 承担
此职责，native/DOM undo 不得成为另一份文档历史。

Undo 的 guard 检查目标文本整体，初版保守拒绝同对象的文字/宽度冲突，不自动合并；无关
对象、Transform 或属性变化不因全局 generation 改变而被错误拒绝，且不会被 Restore 覆盖。
远端 canonical apply 不等待编辑会话；冲突使投影 stale，旧合成不提交，重新同步后继续。

## 6. Snapshot3、混合对象和 reader/writer

继承已绑定 ResourceManifest P12/P13 的 `CapabilityRequirementsV1.required_ids`：
规范 ASCII、排序唯一、严格编码；schema3 必须声明 `resource-manifest-v1`；Triangle 要求
`shape2-triangle-v1`。来源是精确
[capability registry](https://github.com/Mostorm-Labs/axiom/blob/057464f9fab2db4b67c032134cce3f5f417a64e0/.aegis/packages/FC-00X-RESOURCE-MANIFEST-P12-P13/capability_registry.v1.yaml)
与同包 `snapshot_binding.v1.md` / `digest_projection.v1.md`，不是根据 enum 名字猜测。

本稿拟新增 **`richtext-fixed-width-v2`**，对象集存在 RichText/2 时必须且仅此时声明。
Snapshot 必需能力根据 canonical 对象/manifest 需求派生：不能漏报、静默删能力、接受未知
required id，或把工具预设、Undo 栈、会话状态加入能力/digest。旧 V1 文字对象继续合法。

| 文档输入 | 新 reader 的条件/结果 | 旧 reader 的条件/结果 |
| --- | --- | --- |
| 原 schema1/2，无扩展 | 现有显式 legacy import 与 Triangle 准入不变 | 原规则/原编码 |
| schema1/2 带任何 schema3 扩展或 RichText/2 | 拒绝混合格式 | 拒绝 |
| schema3：原对象 + 合法 manifest/caps | 严格现有资源、digest、checkpoint/frontier 绑定 | 仅具对应能力时可读 |
| schema3：Ink/Shape + Triangle + RT1/RT2 + 字体 | 同时具有 resource-manifest、triangle、fixed-width 能力；各对象版本与引用均有效 | 缺任何所需能力整体拒绝；不得丢宽度再保存 |
| 新宽度对象伪报成 V1，或缺能力、坏 guard/manifest/digest | 拒绝，不作修复性默认填充 | 拒绝 |

能力声明必须解析并与 reader 支持集合比较，还要检查实际对象需求。不能给互斥
SnapshotCapability enum 增加一个 Text 值冒充集合。保留旧调用数值/legacy 行为；后继窄实现
在现有 codec/bootstrap 边界内支持组合，不另立 Registry 或第二个恢复 owner。

Snapshot3 的 canonical font 引用由 manifest 绑定；字体字节缺失只影响布局可用性，不能删除
资源/对象来使恢复成功。codec、typed bootstrap、Reference/Indexed store 都先验证完整内容、
能力、引用、digest 与 checkpoint binding，再按现有 Loading/restore 边界发布；typed 入口
不能只看 digest 长度而跳过验证，也不能把恢复历史 source counter 当成本地 live generation。

Snapshot 不保存编辑历史。对 delta2 的写入/有序日志读取能力另行检查版本：新 editor 在
创建会话前确认当前 Runtime/目标写入链支持 delta2 和精确 Restore；否则拒绝启动写会话。
客户端即使只编辑旧 RT1，也可能产生 delta2，不能靠对象 kind_version 推断它仍兼容旧
writer。恢复 continuation 必须先对所选日志区间的 Operation/delta 版本完成准入，再发布
恢复完成；旧客户端不能忽略不认识的 delta 或绕过工具限制继续写。

### 已定位的实现缺口，不伪装成新合同已实现

1. `snapshot.cpp` 与 `snapshot_bootstrap.cpp` 将 Triangle 限制在 schema2；schema3 混合文档
   仍需修复。capability bytes 目前只判非空，没有按所声明的 required_ids 作准入。
2. typed bootstrap 目前检查 digest 长度；完整值校验和 expected checkpoint binding 不能
   依靠调用者自行保证。后继实现需要统一已验证输入，保留原有 Loading/原子恢复边界。
3. 当前 `snapshot.cpp::snapshotDigest` 的 semantic 分支对裁剪后的 protobuf 求 SHA256；绑定的
   `digest_projection.v1.md` 则定义带 domain、对象/manifest 长度等 framing 的投影。两者不等同。
   **不得在 FC-02 中静默改 digest_algorithm=1 的含义或把源码当新 Authority。**
   对需要新持久化写入的切片先 fail closed，单独核对该差异的既有决策/向量并在原边界窄修复；
   未解决前不宣称 Snapshot3 混合保存/恢复兼容。这个限制不阻止会话/FakeIME 接口设计。

## 7. 交给接口设计的约束与下一切片

语义选择已收敛为以上一个方案。后续 Runtime 接口设计应消费这些结果，而不是重新问用户
Enter 或宽度存储规则：

- Interaction 拥有会话、投影、scalar 映射与退出状态；Semantic 拥有 S 和原子 mutation；
  TextLayout 仍是唯一布局 owner，Host 只报告平台输入、执行 IME 完成与物理焦点请求。
- Facade 传递完整对象/段落/Operation 身份、会话与投影 token、顺序输入和明确接受/拒绝回执。
  Debug UI 与宿主均不能构造 canonical delta 或直接改 editor/D3D12。
- Enter 结束、Ctrl+Enter 换段；IME 候选消费优先。切工具/受控离页经过
  `finish composition -> confirmed input -> accepted canonical receipt -> exit` 屏障。
  失败保留可恢复会话并返回原因，重复/迟到 callback 不得落到新会话。
- 首次非空提交单次 InsertObjects；新空草稿零 Object/Operation/History。既有文本删空保留对象。
- 首个实现切片为严格 delta/content + History 与 FakeIME 会话，持久化路径先解决上节缺口，
  再接 Windows/Web。旧 Web/Playground 测试接线与 Android 编译修复继续延期。

后继最小证明必须覆盖：两框独立宽度；V1→V2→Undo 回到精确 V1；相邻同样式/空 run 的
精确 Undo；split/merge/style/跨段 replacement；失败多步原子回滚；新 ID 重放与旧 ID 重试；
无关 Transform 保留、同内容冲突拒绝；Triangle+Ink+两种文本+字体的混合 Snapshot；缺能力、
坏 presence/guard/digest 与 continuation 版本拒绝。这里是实现约束，未执行项不标为 PASS。
