# ADR-0015: 平台支持分级与 Shell 选择策略

- Status: Accepted
- Date: 2026-08-17
- Related stages: POC-01, R1, R3, R5
- Clarifies: ADR-0001, ADR-0002
- Superseded in part by: ADR-0025（产品 Tier/Shell 与 Apple/macOS 范围）

## Context

POC-01 在六个平台验证共享 Runtime，但首批产品 Shell 只有 Web、Windows 和 Android。
如果 portability harness、Web reuse target、Headless utility 与正式产品 release 使用同一
门禁，会让“能运行共享 Runtime”被误解为“承诺完整产品支持”。同时，Shell 可替换不应
让当前 React/Tauri 和 React Native 选择变成 Runtime 的不可替换依赖。

## Historical Decision

本节记录 POC/早期产品阶段的平台分级。当前 Windows RNW、iOS/iPadOS RN 和 macOS deferred
矩阵以 ADR-0025 为准；POC-01 六端可移植性证据与“Shell 不分叉 Runtime”原则继续有效。

以下表格是 2026-08-17 的历史分级快照；当前产品矩阵以 ADR-0025 为准：

| Tier | Targets | V1 责任 |
| --- | --- | --- |
| Product Tier A | Web、Windows、Android | 正式产品 Shell、完整用户流、性能/IME/Input/Surface/发布与支持门禁 |
| Portability Tier B | macOS、iOS、iPadOS | 共享 C++ Runtime、C ABI/ObjC++ harness、Ganesh/Metal bring-up 和核心 conformance；不承诺 V1 产品 Shell |
| Reuse Target | ChromiumOS | 复用 Web 产品 target；平台 FastInk 是可选 capability |
| Utility Target | Headless | test/reference/golden 和内部受控 export；V1 不承诺公共 server/batch rendering API |

- ADR-0025 已替代本节的产品分级：当前产品目标为 Web、Windows RNW、Android RN、
  iOS/iPadOS RN；macOS native deferred/Web reuse。长期架构不变量是窄 Bridge、Windows
  native canvas region、Android Native CanvasView/JNI、Apple Native Canvas/ObjC++ 以及高频
  Pointer/IME/Render 数据面不经不必要的 JS 往返。
- 更换 React Native 或其他 Shell framework 需要产品/平台决策和对应 contract/
  regression evidence；若 Bridge、native data path、surface ownership 或 Runtime 边界不变，
  不视为推翻 Visual Document Runtime 架构。若改变这些不变量，则必须新增 Architecture ADR。
- POC-01 继续对六平台执行完整共享引擎 acceptance。产品阶段的发布阻断以 Tier A 为主；
  历史 Tier B 维持 core compile/conformance 和定时完整验证，不能被产品特例分叉 Runtime；
  当前 iOS/iPadOS 已进入产品目标，macOS 继续使用 core/Web conformance 分支。
- Headless 的 server-side export、thumbnail service、PDF/image batch conversion 和公共稳定
  API 属于未来产品能力，进入前另建 ADR 与安全/资源预算门禁。

## Historical Consequences

- 以下结果只描述本 ADR 接受时的历史分级，不再约束当前产品矩阵：当时 R5 “Release”只指
  三个 Tier A 平台，Tier B 失败被归类为 portability regression；CI 也按该 tier 划分。
- 当前 iOS/iPadOS 已提升为产品目标，Windows 已切换 RNW，macOS native deferred；对应发布、
  CI、真实设备和支持责任全部以 ADR-0025 及现行 Gate 路线为准。
- 继续有效的不变量只有：共享 Runtime 不因 Shell 分叉，平台热路径不绕入 JS 状态层，且平台
  特例不得污染 Document/Operation/Scene/Stroke/Text 语义。

## Validation

POC-01 对六平台保持 digest/golden/lifecycle/smoke。当前产品阶段的 core/public ABI 变更同时
编译 Web、Windows RNW、Android RN、iOS/iPadOS RN，并运行 macOS core/Web-reuse harness；
定时任务覆盖完整产品矩阵与 macOS conformance。R5 只在产品目标完成安装、升级、真实设备、
性能、稳定性和支持演练后发布。任何平台特例导致共享 Document/Operation/Scene/Stroke/Text
语义分叉均阻断所有目标。
