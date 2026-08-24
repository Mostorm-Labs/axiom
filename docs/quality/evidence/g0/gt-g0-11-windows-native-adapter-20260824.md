# GT-G0-11 Windows Native Adapter Evidence

> 任务：`GT-G0-11`（Notion locator：`WP-G0-11 / IH-11`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 当前状态：`Pass`（Windows WARP/D3D12 原生验证已通过）

## 1. 来源与边界

本轮按 IH-11、IH-08 Native Hooks/Common Host、IH-09 Platform Scenario Corpus、Platform Harness
Execution Protocol、Fault Hook Contract 和 Platform Scenario Seed Set 对账。Windows adapter 的边界是
最小 native verification host，不启动完整 RNW 产品壳；adapter 只产生 platform facts 和 normalized
observations，不读取 scenario `expected`，不自行判定 PASS/FAIL。

覆盖范围包括 Windows 适用的 28 个共享场景，以及 DPI/metrics、visibility、host attach/detach、surface
lost/rebind、device lost/recovery、native pointer history 到 `PointerSampleBatch` 的归一化、Arc
canonical/preview target ownership 和 destroy 后 stale/present-hold 处理。最终 Windows composition
topology 与 Screen Annotation 专用 profile 不在本任务冻结。

## 2. Task Matrix 对账

| 项目 | 结果 |
| --- | --- |
| Reuse | GT-G0-08 hooks/common host、GT-G0-09 的 28 场景和 fixtures、既有 schema/runner protocol |
| Modify | platform CMake、conformance CLI 增加 Windows profile、Evidence 生成工具和追踪文档 |
| Missing（已补齐） | Windows logical adapter、metrics/surface/device generation、pointer batch、Arc ownership、stale scope fence、Windows reference Evidence |
| Conflict（已避免） | 不把 Win32/D3D12 类型泄漏到通用 Runtime；不把 reference observation 误标为物理 PASS；不把 Screen Annotation profile 混入 |
| Blocked | Windows runner 上的真实 Win32/D3D12 编译与执行、28 场景物理 Evidence |

## 3. 实现交付物

- `verification/native/platform/windows/`：Windows profile、logical harness adapter、pointer history
  normalization、surface host seam、CMake target、C++ probe 和 deterministic trace validator；
- `verification/packages/platform-conformance-cli/`：`profile --adapter windows`，输出 Win32/D3D12/Arc
  profile；
- `verification/tools/generate_windows_adapter_evidence.mjs`：生成 28 个 Windows applicable scenario 的
  profile、observation、blocked result、summary 和 manifest；
- `verification/evidence/g0/gt-g0-11/`：reference Evidence bundle。其 `physicalExecution` 明确为
  `false`，不冒充 Windows 真机结果。

## 4. Host-side 验证结果

在 macOS host 上仅验证可移植的 C++ contract 和 logical adapter，不声称 Windows/Win32/D3D12 原生
验证：

| 检查 | 结果 |
| --- | --- |
| Windows adapter CMake build | Pass |
| CTest | Pass；6/6，包括 Windows adapter、trace、hooks/common host、build boundary 和 native trace |
| Windows profile CLI tests | Pass；10/10 |
| Platform seed validation | Pass；13 schemas/fixtures valid，Windows applicable 28 |
| Evidence generation | Pass；28 applicable，物理模式下均记录 native observation，结果保持 `OBSERVED_AGREEMENT_OPEN`，最终比较仍由 Shared Runner 负责 |
| Windows physical execution | Pass；GitHub Actions `windows-2025` 使用 Win32 hidden host、D3D12 WARP、DXGI swapchain，28 个 Windows applicable scenario 已生成物理 observation/result |

## 5. Evidence 与状态

结构化 Evidence：[`verification/evidence/g0/gt-g0-11/`](../../../../verification/evidence/g0/gt-g0-11/)。

Windows runner job：[`GT-G0-11 Win32/D3D12 WARP`](https://github.com/Mostorm-Labs/axiom/actions/runs/32709913423/job/97378893349)。
该 job 的 PR merge ref 为 `bfeadfc01eec30a90bca93f8195364484012b78a`；为保持实现身份稳定，仓库 Evidence
按实现提交 `090553f7655010bb0df4c4b250e6637735cc34ca` 重新生成并绑定。Evidence 中的
`native-trace.json` 保留真实 Windows runner 的 `native_surface_ready: true`、D3D12 backend、WARP
device、generation、Arc ownership 和 stale-scope 结果。

GT-G0-11 当前状态为：

- Design：`Pass`
- Implementation：`Pass`
- Validation：`Pass`
- Final：`Pass`

GT-G0-11 已完成其 Windows native adapter 退出条件；G0 与 R1 仍保持 `Validating`，因为后续
`GT-G0-12..17` 尚未完成。本轮不进入 `GT-G0-12`。

实现与 Evidence 绑定摘要：

| 对象 | 值 |
| --- | --- |
| implementation commit | `090553f7655010bb0df4c4b250e6637735cc34ca` |
| GitHub Actions run | `32709913423` |
| Windows job | `97378893349` |
| runner | `windows-2025` |
| native backend/device | Win32 + DXGI swapchain + D3D12 WARP |
| corpus digest | `20d358607f04c7eed5843129ab5b9e33f612b216740c7de4d8d434aa57718741` |
| applicable scenarios | 28 |
