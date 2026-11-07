# UR0b 埋点、runner 与基线复现实施包候选 r1

状态：内容锁定的待审候选，尚不授权生产写入或采样。用户本轮“继续下一步”推动 UR0a 审查及 UR0b 包准备；实现面已复核 UR0a 字节和静态检查，但尚无独立控制面审查结果。本包不把实现面的自检当作独立批准。

## 1. Role

P30/P31 implementation owner：把观测缺口变成具体可执行的纵向切片。控制面负责独立审查 UR0a、接纳本包并冻结执行窗口；本会话不自发签发 P34/G5 PASS。待审记录见 [review-request.json](../../../../.aegis/results/G5-UR0b-candidate-r1/review-request.json)。

## 2. Authority

受控仓库 `Mostorm-Labs/axiom`，工作树 `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`，分支 `codex/geometry-navigation-integration`，任务 anchor `a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e`，关系 ancestor，不要求后续 HEAD 恒等。

继续沿用 AGENTS、manifest 的 Architecture v0.4、Semantic/Operation 与 SDK 契约、已锁定的 Semantic/Skia SDK。R2、R3 设计和 UR0a 协议/矩阵作为本包精确绑定的设计输入；在独立控制面接纳前保持 proposed 状态。R3-2/3 在此明确落入 UR0b 观测；R3-1/4 的反馈替代和共享本体分叉实现留给 UR1/UR2。

UR0a 所有文件及旧证据原样保存。本包不把 UR0a 交付 revision 误作未提交文件的 Git 归档身份，实际内容由 SHA256 绑定。已存在的三份修订 2、三份修订 3 及 UR0a 包/结果均不改写。

## 3. Objective

形成可信的 CURRENT_BRANCH 基线，回答“局部擦除后再擦、导航和带 mask 拖动分别在哪个阶段等待”。分开测量计划投递、真实 handler、Runtime admission、精确命中、publication/History、Operation commit、Scene/reference、coverage/material、有效 submit、qualified completion 和 GPU 退休。先固定观察事实，再由 UR0c 决定 UR1/UR2 优化优先级与阈值。

## 4. Non-goals

- 不更换 Brush/Scene/coverage 算法，不接入新的 GPU 策略，不优化 temporary encoding，不调整缓存/预算/调度。
- 不移动 canonical commit、History、Scene 恢复或反馈退休的现有顺序，不修正 INPUT_REJECTED 的产品行为；只如实区分未提交拒绝和提交后派生错误。
- 不建立 UNIFIED_POLICY 假入口，不增加导航 canonical receipt、不调用同步 GPU finish/readback 来伪造完成。
- 不增加消费者 SDK capability、可见调试命令或内部 Runtime/Skia 访问器，不发布资格构建；不改版本、schema/descriptor/依赖锁、CI 或旧 P34。
- 不把 native/headless/probe 冒充真实 Web 主路径；不声称 Skia 内部没有软件 coverage。UR5 规模矩阵及跨平台证据不在本包。

## 5. Required Analysis：owner、切片与具体接线

公开 root SDK 输入与命令不变：DOM collector/Session → RuntimeComposition → Interaction/Ink → OperationEngine → Scene → Reference/FramePlan → SkiaRenderer → Provider。采集层是旁路观察者，不分配 canonical 身份、不做 mutation 或恢复决策。

| 切片 | 现有 owner / 需要观测的 seam | 交付与核验 |
| --- | --- | --- |
| B1：私有采集器 | Foundation internal 固定记录、owner 上下文；跨线程完成回调只用安全采集状态 | 有界 ring、OFF/ON、关键/普通丢样计数、lossless ID、drain 生命周期、单测；热路径不做 JSON/string/profile/readback |
| B2：真实输入和提交 | 验证 host 包装实际 DOM listener；Session、Composition、Interaction、History、Scene 的现有执行点 | planned/producer/enter/exit/admission；Up 每阶段；真实 commit/before-after/Scene 身份；不修改分支和 return |
| B3：Render 决策和实际 target | 当前 renderer/cache/material/navigation/body-mask/provider 分支 | 按 UR0a route_id 记录 CURRENT_BRANCH decision→leaf 或带 basis 的 pixel-hit；factory 请求与实际 GPU/CPU/recording 分开；Direct 漏项也记录 |
| B4：重复工作与 GPU 队列 | reference build/encode/digest、lookup、prepared geometry、resource budget/retirement | 编码/丢弃字节与时间、快速命中/深比较/复制计数；未完成 group、最老年龄、完成观测延迟、已完成待有序退休和输入停止后的收敛 |
| B5：fixture 与独立 oracle | 公开 SDK setup/snapshot，离线 native exact / CPU strict / GPU geometry-AA oracle | 实际 profile/config/seed、16 总对象=1 原件+15 副本、E0/E1 命中与固定 drag 点资格；像素/语义校验在计时外 |
| B6：driver 和基线 | 新验证 runner；旧脚本/结果保留 | 预排输入、catch-up、parent watchdog；6 对开销资格；10 行×3 独立重复或按原规则有界停止；逐行记录失败/删失/MISSING |

这些切片是同一 UR0b closure 内的顺序步骤，不是六个可在中间返回的任务。具体允许文件、obligation/oracle 与终态见包内 JSON。

### 私有构建及数据通道

新增默认 OFF 的 `AXIOM_G5_UR0_DIAGNOSTICS` 资格选项，限定显式 Web/WASM qualification 构建。私有 native 单测可 target-local 启用 collector，其对象不链接或安装到普通 SDK。normal SDK 的 root exports/types、私有 JS call 命令集和 DLL ABI 不变；normal artifacts 不含新增资格导出，资格 variant 不进入普通 install/release 产物。

资格 WASM 内部仅提供控制、字节地址、字节长度三个验证专用导出。具体名字在 implementation-context 冻结。verification loader 在隔离 staging 包装 generated internal factory，root SDK/declarations 保持匹配且不向消费者返回 module。主工作负载仍从真实 root SDK 和 DOM 输入进入；控制通道只做采集配置和读取。

控制请求只接受固定 opcode/uint32 参数；诊断 event 序号与 run 关联，不替代 source/pointer generation。clock/drain 的 u64/128-bit 身份用十进制/hex 文本编码在计时外输出。返回 buffer 是 module-owned borrow，下一次控制调用、reset 或 close 失效；runner 立即复制 HEAPU8 范围，不持有跨调用借用，不 transfer/free module 内存。长度不超过 UINT32_MAX，使用现有 owner thread，禁止重入。

主采集最多 16MiB，其中 1MiB 为关键事件保留；总量跨仍持有诊断状态的旧 run 计费，不能借新 run 绕过。有界计数器可保留总量，不覆盖丢失 terminal 来假装完整。关键身份/decision/leaf/terminal 丢失则该行不可判定；普通丢样报告覆盖率。采集满不改变输入/commit/render 的结果。

OFF/ON 是同一资格 binary 的运行时开关。旧 `AXIOM_ENABLE_FRAME_TIMING` console 输出在主 cohort 关闭；现有 spans 可复用其取时/嵌套机制，但 transient/Direct/Up 阶段必须有独立根，不能只扩大 canonical 根统计。normal 无诊断构建只做正确性/包装边界对照，不能替代同二进制开销配对。

### 计数真实性

counter 带 `EXACT`、`UPPER_BOUND`、`UNSUPPORTED` 或 `MISSING` 能力说明。已有 size/循环游标可直接计数；不得为了精确计数再遍历所有点。深比较调用次数与实际比较元素分开，上界不能标为实际访问量；复制值语义字节与 allocator 实际分配字节分开。无法可靠取得的量明确缺失，阻断依赖该量的归因，不伪造零。

GPU 沿现有完成回调采集，不新增 canonical fence 槽。group/ticket 不是 GPU 指令数；submit→完成观测时间含队列与 poll 等待，不是纯 GPU 执行时间。abandon/physical loss/unsupported 不算成功 GPU completion；已完成但待有序资源退休单列。完成回调不得捕获 Runtime/provider 或不安全 TLS，采集状态生命周期独立、有界且不可重入。

### 输入、矩阵与执行限额

原 UR0a 协议、字面 fixtures、10 行矩阵和开销资格数值不变。两个预算档是 navigation/shared 的 32/96 与 64/192MiB；不变更默认值。诊断内存单列，不能当作额外 coverage/staging 额度。

旧 9 对象拖动仍保留 per-Move RAF；旧 16 对象擦除保留逐轮 quiescence。新三分支使用固定 30s 内容 corpus 做 accelerated setup，E0→立即 E1→立即再擦/导航/masked-drag，原计划不得因实际 Up 阻塞而顺延。真实 60/120/240Hz 投递资格另列；只对执行过的档位作实时延迟声明。

开销先跑 6 对 `[AB,BA,BA,AB,AB,BA]`，每个 run 90s。主矩阵每行 3 次独立重复，每 run 45s setup +120s measurement +15s cleanup、3GiB process-family RSS，30 run 上限 5400s，主批含开销 6480s。可选一次有具体噪声原因的开销诊断复测最多再 1080s，保留原结果。实时校准 3 档各 90s，像素/故障诊断最多 6 worker×90s；独立附加总额度 810s，整轮全部上限 8370s。编译/单测另列，不与性能采样并发。

一次 hard timeout/RSS guard 停止该预算档，保存计划、最后 heartbeat、失败和右删失，未运行的行 MISSING。parent watchdog 独立于可能卡死的 page event loop，清理必须结束后才启动下一 worker。不能不断重跑直到三次成功，也不能改 fixture、预算或阈值补齐绿结果。

## 6. Required Output

在 `verification/evidence/performance/g5-unified-render-ur0b/<source-tree-sha>/` 存匹配 source/tree/runner/fixture/JS/types/WASM/resources 的 manifest、原始外层时序、内部 ring、解析后的 metrics、route/target/lease 对账、stage/counter/GPU queue 汇总、独立 oracle、同二进制开销结果和逐行 summary。`.aegis/results/G5-UR0b/` 放执行结果、精确物化身份、完整失败/跳过清单。

结果分开报告 correctness、evidence completeness、baseline performance。慢基线/可靠 RED 不被改写为采集器失败；缺身份、错误分母或未通过开销资格使相应延迟结论不可采用。无数据字段为 null/MISSING，不能用 0 占位。没有新产品性能 PASS，33ms 方向不变，具体门槛留给 UR0c。

## 7. Quality / Evidence Gate

完整 obligations 在 verification.lock。先验证采集器、时钟、身份与失败分类，再做 fixture/oracle 和同 binary 开销，最后才主矩阵。所列 C++/Node/Python 和 SDK 边界回归必须执行；已有全量原生 66 项失败与 4 项编译问题按旧证据保留精确身份，不称全量通过，不扩进本包修复。full CTest/hosted CI 为 corroborative；本轮不需要发布平台 SDK 或实机 PASS。

只要原计划与每个 run/跳过原因均忠实保存，性能不足或触及已冻结停止条件可以形成可审查的部分基线；缺失行不能声称完成覆盖。采集机制本身不能通过资格时，按冻结 blocker 返回，不能把它当作可靠性能结论。

## 8. Handoff 与执行入场

本候选和独立审查请求先交控制面；现在不启动 B1。控制面需提供独立 UR0a 接纳、本候选精确 hash 接纳及独立设计物化 commit。执行前核对同仓库身份、ancestor delta、内容锁、允许路径和 activation 的真实审查来源。不能仅写一个 `approved: true` 当作独立审查。

本轮静态校验器默认只做候选检查，明确返回 `can_execute=false`。收到独立审查结论后，控制面另行物化 `.aegis/packages/G5-UR0b/package.json` 可执行包并绑定本候选及 activation；本候选校验器继续只校验静态材料，不能用它自行放行。P32 内部 ImplementationDesignPreflight 若无矛盾即连续执行 closure，不再逐小步请求授权。结束返回 READY_FOR_CONTROL_REVIEW / 明确 blocker，停止在 UR0b，不自动进入 UR0c/UR1，不签发 Gate PASS。
