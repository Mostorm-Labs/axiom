# UR0a 可观测性与执行就绪审查

状态：静态协议材料，等待控制面审查。执行授权仍是修订 2 的 `G5-UR0a` 包，起点 `a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e`。本次没有新增埋点、runner、浏览器采样或生产实现。本文的“现有”来自源码检查；新动态证据全部 MISSING，不能据此签发平台、G5 或 P34 PASS。

## 交付与所有权

| 冻结义务 | 交付 | 静态检查内容 |
| --- | --- | --- |
| UR0a-PROTOCOL | [protocol.md](protocol.md)、[metrics.schema.json](metrics.schema.json)、[schema-examples.json](schema-examples.json) | 单调时钟、分段、身份、拒绝/删失分母、nearest-rank、导航无 canonical receipt；正反例 |
| UR0a-ROUTES | [routes.json](routes.json) | 公开入口与私有分支、显式 factory 反向清单、owner、迁移分类、后续包、正反例、真实 target 与身份缺口 |
| UR0a-MATRIX | [matrix.json](matrix.json)、[fixtures](fixtures/) | 9/16 对象锚点、两个预算档、三种立即衔接、三次独立重复、固定输入、超时、清理与整批停止 |
| UR0a-READINESS | 本文、[validate.py](validate.py)、结果目录 | 当前观测与独立 oracle 能力、UR0b 补齐工作、开销资格和未启动边界 |

Runtime/Interaction 继续拥有输入、编辑、History、反馈交接和 commit 准入；OperationEngine 是唯一 canonical mutation owner；Scene 是派生投影；Render/Provider 拥有实际绘制、资源和完成观测。SDK 保持现有 root facade、C/POD 和生命周期契约。观测使用私有诊断记录，不把内部缓存、Skia 或可变 Runtime 对象暴露给消费者。

## 现有 seam 与 UR0b 缺口

源码身份、行号和文件 SHA 在 routes 的 `source_refs` 中，不以本文叙述替代精确绑定。

| 观测面 | 源码事实 | 缺失及后续工作 |
| --- | --- | --- |
| Web 输入 | root SDK 安装 PointerEvent collector；Move 可读取 coalesced events；collector 仍逐 fact 同步调用 Session；Session 构造 PlatformPointerBatch，Runtime 做归一化与 Interaction dispatch | 原计划、producer-send、handler enter/exit、Runtime admission、真实 pointer/source/generation 尚未统一关联。UR0b 冻结私有 event/batch/interaction ID 并保留 rejects；不能把合成事件称为物理触控板测量 |
| 实时负载 | 旧 drag 每 Move 等 RAF，旧 cumulative erase 每轮等 quiescence；7200 点的内容时间跨度是 28.796s | 新字面 30s corpus 与独立任务计划、catch-up/迟发计数尚未实现。立即序列必须预排，不根据上一 Up 实际返回时间滑动计划 |
| 下一 Down | `eraserBegin` 在 canonical dirty 时可能同步 `presentCanonicalFrame`，之后才开启 trace | UR0b 分开 admission、前置 present、Scene 可用性和首反馈；当前没有证据证明下一 Down 已解耦。UR2 才改变该行为，并先证明 Scene recovery 可独立推进 |
| Up→commit | `eraserFinish` 包含精确命中、publication 预留、OperationEngine apply、observeApplied、History、Scene 以及可能的 present | 每段跨度及 before/after generation、Operation/commit ID 未统一暴露；apply 后 History 仍可失败。UR0b 只观测，不能把已提交但派生失败重写成未提交 INPUT_REJECTED；UR2 使用现有 prepareRecord/commitPrepared 做独立事务修复 |
| commit/observer | `observeApplied` 选择更新并复制 commit record，再调用既有 observer | 需分别计 publication custody、commit-record 复制、History finalization、SDK 编码/observer 成本，含没有外部 DataPort 的路径；不以总 Up duration 推断全是 PathOps 或 coverage |
| Scene/Reference | incremental coordinator 与 recovery 已有；canonical 和 transient 各自构造 reference，transient 随后清空 canonical bytes/digest | Scene apply、source 物化/共享、reference build、编码/丢弃字节、hash/metadata/深比较/复制需各自计数。canonical 根 timing 不覆盖所有 transient；没有 span 的 0 不能解释成没有工作 |
| canonical/Direct | cooperative preparation、streamed material、pixel hit、Direct fallback 并存，调用默认兼容参数不同 | UR0b 对每个 attempt 记录 CURRENT_BRANCH 决定、实际 leaf、目标、fallback 原因和终态。只看到 GPU draw 不能证明经过未来 UNIFIED_POLICY；Direct 也必须计入覆盖分母 |
| preview/edit | v4 preview stable prefix/mutable tail 仍可能创建 CPU raster；transient creation 有二次 ink 绘制；普通拖动与带永久 mask 的分支不同 | 逐 route 归因 CPU coverage、geometry build、lookup 与合成，保留其他 family/legacy 分类；UR0b 不替换实现 |
| body/mask | 共享 prepared geometry、独立 body/mask 层与 mask prefix 更新已有部分实现 | 命中前扫描、不同 mask/transform/body identity 的实际复用及失败恢复缺少新链路证据。mask Undo/替换不等同 append-only；不能跳过验证强行命中 |
| navigation | retained plane、细/粗 tile、prepared/unprepared 与资源压力分支并存 | 需要 view/camera revision、Scene/source basis、valid region 的有效 submit 资格。导航没有 canonical completion receipt，不能为了诊断重新占用 canonical fence 槽 |
| provider/completion | WebRuntimeAdapter 区分 prepare/poll，并校验平台完成；资源 retirement 独立持有 GPU epoch/metadata | `onFrame` 粗粒度事件不能直接代替每个输入的反馈；需要精确 frame/basis 关联。旧 completion 只能退休旧资源，不能确认新 surface 或清除新反馈 |
| 实际 target | `makeSurface` 请求可能失败后改为 Raster；SkPictureRecorder 只录制命令，不等同 CPU 像素 raster | 显式 factory 要记录 actual returned type/context/format、allocation/lease。schema 区分 GPU、CPU、RECORDING、NONE、UNKNOWN/MISSING。factory 枚举不是 Skia/driver 内部的完整分配清单 |
| 资源/线程 | retirement ticket 可已完成但仍等待有序 epoch 退休；PreparedNavigationGeometry 有 owner-thread mutable scratch/计费 | 分开 live/inflight/retiring/staging、驱逐与真实释放、预算 owner/context 代；不得把 const shared_ptr 当线程安全。UR0b 不移交 worker、不修改预算 |
| 开销/丢样 | 既有日志可做归因，但没有本协议同二进制 OFF/ON 资格 | 私有有界 ring、critical/statistical loss、未知 route 和 job/attempt 终态对账待实现；开销实验没有结果，当前为 MISSING |

`ImmutableValue` 已有共享指针相等快速路径；指针不等时才可能深比较。后续计数应区分 pointer-fast-hit、metadata/hash/深比较/复制与真正几何重建，不能笼统称“每次命中都复制整条线”。

## 独立 oracle：已有能力与未覆盖组合

- 精确命中：`runtime/render/tests/erase_hit_test.cpp` 对 hit/miss/no-op、历史 mask、对象变换、无效输入及缓存结果进行检查，并对照独立 component oracle。UR0b 可复用其独立几何来源；不能用 GPU 覆盖图、navigation LOD 或 candidate pixels 决定 semantic hit/no-op。新 16 对象的 E0/E1 目标集合和 masked-drag 抓取点仍需离线精确资格，当前 MISSING。
- CPU 严格像素：`runtime/render/tests/render_cache_test.cpp` 保留 strict CPU Direct/cache 字节对照。GPU 已有几何 AA 边界容差分支；边界定义来自独立几何，不能根据 candidate diff 扩张。相同实现的两个缓存输出相等不够独立。
- 共享 body、擦除和导航：`exact_contour_replay_oracle.hpp`、`exact_contour_locality_oracle.hpp`、`navigation_alpha_mask_oracle.hpp` 提供既有独立检查组件。它们不自动证明所有消费者都使用共享 source，也不代表当前连续输入或 mask forks 已通过。
- 最小组合须在 UR0b 逐项绑定具体 fixture/测试：下层半透明对象拖动；永久+活动 mask；透明/彩色背景；warm→miss→pressure；DPR 切换；feedback→canonical。已有单项测试可引用精确执行身份，但缺失组合继续 MISSING，不能用一个不透明白底例子代替。
- History/snapshot：`runtime/interaction/tests/editor_history_test.cpp` 是既有能力入口；旧累计擦除 runner 已声明没有验证完整 History/snapshot。需独立检查 Operation 恰好一次、Undo/Redo、未提交失败、提交后 Scene 失败及 feedback 退休。性能 worker 内不运行昂贵像素 reference、截图/export 或 profile。
- public path：主 cohort 必须使用匹配的 root JS/types/WASM/resources，通过实际消费者输入/公开方法。内部 helper 单测不能代替公开 entry→decision→leaf；原生 providers 本包只静态登记，仍 DEFERRED，无跨平台 PASS。

## UR0b 的实施清单与完成前提

1. 先由控制面发布 UR0b 的准确 package、authority/verification/evidence 内容锁，审查本次协议；UR0a 不自动授权后续埋点、runner 或性能执行。
2. 复用 Runtime/Render/Provider owner 增加私有低成本诊断：事件、阶段、basis、job/attempt/decision/leaf、target、lease。CURRENT_BRANCH 与 UNIFIED_POLICY 分开，缺失 ID 写 MISSING，失败与取消具备终态，禁止新的公开调试 API。
3. 让 wrapper 与内部 spans 使用明确时钟，实测分辨率与校准误差；落实父/子 span inclusive/exclusive 和 exact query/Scene/Direct 漏项，不靠累加嵌套 span 声称总 CPU。
4. 实现 protocol 定义的同二进制低成本 toggle，6 对固定 AB/BA 开销资格。不同 artifact 的编译 ON/OFF 不能替代。超出开销界限或 NOISY 要保留原结果；最多一次有具体原因的诊断重测，不循环到通过。
5. 先验证纯数据 fixture 和公开入口生成的文档、实际 package/config/seed、对象/copy 数量、camera 及可抓取点。真实 Runtime session seed 为 `0x4500 + session`，不是新增可设置的公开 seed API。opaque snapshot 离线解码验证；未确认前不要称相同 brush seed。
6. 增加原计划不变的 producer、terminal 保留和 catch-up 记录；三条立即分支分别 reset，分支内部不等 settle。工具切换若未及时 admission，保留真实延误或失败，不偷偷等待。
7. 覆盖 schema 的拒绝、timeout、missing-submit、no-op、navigation 和 qualified-chain；parent 在页面卡死时也保存原计划/最后 heartbeat/右删失。完整 source tree、artifact 前后哈希与浏览器/GPU 环境进入归档。
8. 先完成独立正确性与观测资格，再跑矩阵预定行；失败行保留，不从尾分母移除。生成 BASELINE_READY / PERFORMANCE_UNMET 等结论须由 UR0c 审查。可靠 RED 可以指导修复，不能据此给错误像素签发性能达标。

同二进制 toggle、完整身份和日志对账、fixture oracle、开销资格目前都缺动态证据。它们是 UR0b 必须补齐的工作，不是本次 UR0a 未满足的执行前置。性能慢也不是静态协议的失败。

## 修订 3 补充：非阻塞建议，不改 R2 内容锁

控制面另存 R3 文件并明确本次 closure 不变；精确内容身份记录于结果目录的 `advisory-inputs.json`。不把 R3 反向增加为 UR0a 必须实现的要求。

| 建议 | 当前静态发现与后续包映射 |
| --- | --- |
| GPU 排队与完成 | UR0b 观测未完成 submitted group 数、最老年龄、submit→完成观测延迟、已完成但有序退休待办、停止输入后 work/staging 收敛。group/ticket 不是 GPU 指令数；完成观测延迟含轮询调度，不能称纯 GPU 执行时间。导航用私有非阻塞观测，不制造 canonical receipt；具体阈值 UR0c，准入规则 UR1 |
| target 与软件 coverage | Axiom 选择 GPU target 不能证明锁定 Skia 内部从未执行软件 coverage；后一结论 UNKNOWN。无权为此重建 SDK。后续资格只按可观测边界表述 |
| transient 编码与 lookup 成本 | transient 当前 build 后清 canonical bytes/digest；UR1 需完整 transient no-encoding 模式，保留真正 canonical 编码/身份校验，不能套用 navigation 的不完整 source list。普通未提交 Move 的丢弃编码字节目标为零；UR0b 先区分编码、验证、hash/深比较/metadata/复制和几何工作 |
| 替代 exact frame 后的反馈退休 | A/B 都已提交，旧 A draw 取消，只呈现包含 A/B 的新帧时，须由既有 Runtime 给出完整 basis/替代覆盖证明。不能仅比较 revision 大小或伪造 A receipt；UR2 前冻结，包含 Undo、删除、Scene recovery 和活动 B，当前 MISSING |
| 共享本体的分叉隔离 | UR1/UR2 必测 A/B 共享 body：仅擦 A 不改 B 像素/命中；A 变换 B 静止不错误复用设备 pixels；不同 mask 历史 Undo；删除/驱逐 A 保留 B lease；不同 document/lifetime 相同 ObjectId 不命中旧缓存。已有快路径不代替这组证据 |

## 静态校验范围与结果边界

`python3 docs/planning/g5-unified-render/ur0a/validate.py` 只检查本地 JSON/schema 样例、数学统计、fixture 字节、源码引用、显式 factory 清单、双向 route 关联、矩阵及冻结输入。它实现本 schema 使用的有限 JSON Schema 2020-12 关键字子集，未知关键字失败；不是通用 JSON Schema 元验证器，也不是 whole-program 静态分析器。未运行任何 Runtime/browser workload。

结果目录为 `.aegis/results/G5-UR0a/`，内容 hash 清单标识未提交文件。`result_revision` 仅记录当前 Git HEAD，不能宣称该 revision 包含这些未提交材料。生产 tracked diff 必须为空，R2、R3 与旧证据原样保存。五份材料可进入 **READY_FOR_CONTROL_REVIEW (UR0a)**；不提交/推送，不自动进入 UR0b。
