# GT-G0-08 Verification Native Hooks / Common Host Evidence

> 任务：`GT-G0-08`（Notion locator：`WP-G0-08 / IH-08`）
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation
> 执行时间：2026-08-24（Asia/Shanghai）
> 状态：`Validating`；verification seam 的本地实现和验证已完成，尚未绑定提交与托管 CI

## 1. 目标、边界与术语

- **Verification native hook**：只在 `AXIOM_BUILD_VERIFICATION=ON` 时编译的故障注入和事实
  采集入口，不进入产品公共 ABI。
- **Normalized platform port**：verification hook 调用的最小平台事实入口。本任务只冻结调用
  形状；仓库尚无正式产品 Platform Runtime，因此测试由 deterministic recording port 承接。
- **SourceLease / SourceAttempt / EventTap**：分别记录异步来源生命周期、每次发布尝试和真正
  进入 runner 的平台事件；stale attempt 必须可见，但不能成为 EventTap。
- **Common host**：平台 adapter 可共享的 session/action/completion/profile protocol bridge。
  它只报告事实，不读取 scenario expected，也不做 comparator。

本任务不实现 Web、Windows、Android、iOS/iPadOS UI host，不选择真实平台 surface winner，
不实现产品恢复协调器，也不把 `FaultHandle`、`SourceLease` 或 `LateEventFence` 加到 Runtime facade、
公共 C ABI 或产品 SDK。

## 2. 交付物

- 顶层 `AXIOM_BUILD_VERIFICATION`，默认 `OFF`；仅在开启时加载
  `verification/native/platform/`。
- `axiom_verification_platform_hooks`：normalized surface/device loss、present completion
  hold/release、generation publish probe、SourceLease、SourceAttempt 和 EventTap。
- `axiom_verification_platform_host_common`：verification build manifest、session/epoch、action、
  exactly-once completion、profile/capability reporting 和 protocol facts。
- 两个 C++ 行为测试、ON/OFF 构建隔离检查器和结构化 trace validator。
- 三份可重放 Evidence：
  - [`build-boundary.json`](../../../../verification/evidence/g0/gt-g0-08/build-boundary.json)
  - [`native-hooks-trace.json`](../../../../verification/evidence/g0/gt-g0-08/native-hooks-trace.json)
  - [`platform-host-common-trace.json`](../../../../verification/evidence/g0/gt-g0-08/platform-host-common-trace.json)

## 3. 测试先行记录

第一轮 RED 在 `AXIOM_BUILD_VERIFICATION=ON` 时因 `hooks/`、`common/` 目录不存在而配置失败，
证明构建边界会约束缺失实现。最小 target 与状态机加入后，两项 C++ 测试通过。

第二轮先加入 CTest build-boundary 用例。测试最初因没有显式发现 Python 解释器而 `BAD_COMMAND`，
补充 `find_package(Python3 REQUIRED COMPONENTS Interpreter)` 后，通过真实独立构建验证 ON/OFF
矩阵。第三轮 trace validator 先因测试程序不接受 `--trace`、没有生成 JSON 而失败；随后只在
测试 executable 中增加输出，产品类没有增加 trace-only 方法。

进一步的 mutation review 发现普通 completion 未经过 generation guard、common host 只有
`ProfileCapabilities` 结构而无 report 行为；对应测试先失败，再加入最小实现。最终还覆盖了
scope 关闭后的 late attempt，确保得到 `DROPPED_STALE_SCOPE` Evidence 而不是静默消失。

## 4. 本地验证结果

环境：macOS 26.6.2 arm64；AppleClang 17.0.0；CMake 4.0.1；Ninja 1.11.1；Python 3.9.6。
没有在该 macOS 主机查找或假设 MSVC runtime。

| 检查 | 结果 |
| --- | --- |
| verification ON clean configure/build | Pass；两个静态库和两个测试 executable 构建成功 |
| native CTest | Pass，4/4；hooks、common host、build boundary、trace validation |
| verification OFF clean product build | Pass；`canvas_runtime_scene` 构建成功 |
| OFF target isolation | Pass；无 `axiom_verification_*` target |
| ON product dependency isolation | Pass；`canvas_runtime_scene` query 不含 verification target |
| public/product source isolation | Pass；`runtime/` 与 `docs/api/` 无 verification include/flag 引用 |
| workspace Python tests | Pass，9/9 |
| workspace manifest | Pass；SHA-256 `4988903650a231b7e67a53646602018fed8327fa308d721be7f5cee51fe1efc2` |
| RF-01 module boundary | Pass，19 contract files |
| Runtime C ABI manifest | Pass，54 functions、49 structs、163 enum constants；无新增 ABI |
| Markdown / diff | Pass，115 Markdown files；`git diff --check` 无错误 |

结构化 fault trace 证明：

1. `SURFACE_LOST@7`、`DEVICE_LOST@8` 进入同一 normalized port；
2. 两个 held completion 在 CLEAR 后按捕获顺序 `1,2` 释放；
3. generation 3 的 held/stale attempts 均记录为 `DROPPED_STALE_GENERATION`，没有 EventTap；
4. generation 4 的 current/immediate attempts 为 `FORWARDED`，各产生 EventTap；
5. scope 关闭后的 late attempt 为 `DROPPED_STALE_SCOPE`，仍不产生 EventTap；
6. close scope 后拒绝新 lease，已有 lease 显式关闭后 `all_leases_closed=true`。

Common host trace 证明 session/action/completion ID 映射、epoch 递增、exactly-once completion、
profile/capability report 与四类非法 transition 拒绝。输出 fact 类型中没有 expected 或 comparison
能力；common host 不能替代 Shared Runner 的 oracle authority。

## 5. Evidence 身份与等级

| 对象 | SHA-256 / Git identity |
| --- | --- |
| 当前基线 commit | `ed64da74331bfcc92bef4f252ca98de708f6f046` |
| 当前基线 tree | `079d4eaec6679fe59a0f677de96352989c025734` |
| build boundary | `99fccd790a7eae3e9046f5290e152d481faa01d5885f9610847fb38aebae5f52` |
| native hooks trace | `097df2236f311fb82a0155f553ad2be28551fbcaa40ef1dbdd8cf113edd826ab` |
| common host trace | `49b25b73253932b2e5c69c64ae4b30b36e97ada9160523fc437578e9f15bdb25` |

当前工作树尚未提交，因此上表基线 commit 仅说明起点，不是实现 commit。提交后必须重新执行全部
验证并追加 implementation commit/tree/archive 与重新生成的 Evidence hash，才可将任务改为 `Pass`。

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | 本地 Pass |
| E2 Reference / Mock | applicable | deterministic recording port 与 common host Pass |
| E3 Integration / Golden | pending | 后续平台 adapters / scenarios 绑定真实 normalized ingress |
| E4 Physical / Demo | not applicable to IH-08 | 真实设备由 G0 后续平台任务负责 |

## 6. 未完成条件与状态

仓库当前没有正式产品 Platform Runtime 或 normalized surface/device loss coordinator。现有结果只证明：

```text
verification hook → normalized port contract → deterministic recording port
```

不能证明：

```text
verification hook → product normalized loss ingress → production recovery/redraw
```

这不是 IH-08 verification-only seam 的实现失败，但是真实产品恢复路径的明确缺口；不得用 test
double 把它改写成产品 recovery `Pass`。该缺口将由后续平台 adapter / canonical host 任务接入并
留下 E3/E4 Evidence。

因此 `GT-G0-08` 当前为 `Validating`：设计与本地实现/验证已完成，等待提交后的 commit-bound
复验和适用的 hosted CI。`GT-G0-09` 保持 `Not Started`；G0 和 R1 均不晋级。
