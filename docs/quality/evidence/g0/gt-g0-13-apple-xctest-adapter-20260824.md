# GT-G0-13 Apple XCTest-style Adapter Evidence

> 任务：`GT-G0-13`（Notion locator：`WP-G0-13 / IH-13`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 当前状态：`Validating`

## 1. 来源与边界

本任务建立 iOS/iPadOS 的薄验证适配器，为后续 React Native Fabric Product Shell 提供 Native
CanvasView、ObjC++ Bridge、`CAMetalLayer`/Metal drawable、生命周期和输入边界。iPhone 与 iPadOS
使用独立 profile、独立 observation 和独立 Evidence，不能合并为一个 Apple Pass。

适配器只记录 platform facts，不读取 scenario `expected`，不自行比较或判定 PASS/FAIL。Pencil/coalesced
touch history 以批量形式进入 ObjC++/C++；RN JS 不进入输入热路径。macOS 只保留 shared-core/reference
conformance，不建立 native 产品门禁。

## 2. Task Matrix 对账

| 项目 | 结果 |
| --- | --- |
| Reuse | GT-G0-08 hooks/common host、GT-G0-09 共享 28 场景和 fixtures、Apple POC-01/POC-05 的 Metal/RN 边界经验 |
| Modify | verification CMake、Apple C++ adapter、ObjC++ CAMetalLayer seam、TypeScript Apple package、CLI、Evidence generator 和 workflow |
| Missing（本轮补齐） | iPhone/iPadOS 独立 profile、surface/device generation、coalesced Pencil batch、Arc ownership、facts-only Apple Evidence |
| Conflict（已避免） | 不把 macOS 当作 native 产品目标；不合并 iPhone/iPadOS 报告；不把 POC Apple runner 或 RN POC bridge 升级为产品 ABI |
| Blocked | iPadOS 物理设备当前不可用；iPhone 真机已独立采集，不能替代 iPadOS Evidence |

## 3. 实现交付物

- `verification/native/platform/apple/`：C++20 Apple harness、profile、输入批处理、surface seam 和 host test；
- `verification/native/platform/apple/src/apple_surface_host.mm`：仅保留 `CAMetalLayer`/Foundation/QuartzCore 的 ObjC++ seam，不泄漏到共享 C++ 接口；
- `verification/packages/platform-harness-apple/`：iPhone/iPadOS profiles、facts-only adapter 和 pointer batch normalization；
- `verification/tools/generate_apple_adapter_evidence.mjs`：生成独立 `ios/` 与 `ipados/` 两组 28 场景 Evidence、summary、applicability 和 hash manifest；
- `.github/workflows/g0-apple-xctest-adapter.yml`：macOS hosted runner 上的 C++ host、shared corpus 和独立 Apple Evidence 验证。

## 4. 当前验证结果

本轮 iPhone 物理采集于 2026-08-25 在已配对的 iPhone 15 Pro 上完成。设备端使用现有
POC-01 Apple Metal Release runner 作为共享 C++/Metal seam 的物理执行探针；这不是把
POC runner 或其接口升级为产品 ABI，也不把它表述为完整 RN Fabric XCTest UI 测试。

| 检查 | 结果 |
| --- | --- |
| Apple C++ adapter build | Pass；macOS host，ObjC++ Metal surface seam 编译通过 |
| CTest | Pass；8/8 verification tests，包括 Apple adapter |
| Apple TypeScript package | Pass；3/3 adapter tests |
| Shared CLI | Pass；iOS 与 iPadOS profile、28 场景 facts-only run 测试通过 |
| Apple Evidence generator | Pass；iPhone 与 iPadOS 各 28 个 observation/result，schema 与 manifest hash 校验通过 |
| macOS boundary | Pass；`macos-core-summary.json` 明确为 `HOST_LOGICAL_CONTRACT_ONLY`，不构成产品门禁 |
| iPhone physical device execution | Pass（共享 C++/Metal seam）；iPhone 15 Pro / iOS 26.6，Ganesh Metal；100 lifecycle、60 秒/3,600 帧 smoke、max frame 4.80675 ms、内存分析通过；视觉匹配 99.982708%；采集时 thermal state 为 `serious`，因此不作产品级性能结论 |
| iPadOS physical device execution | Pass（共享 C++/Metal seam）；iPad Air 4 / iPadOS 26.6 / Apple A14 GPU；100 lifecycle、60 秒/3,600 帧 smoke、max frame 5.26979 ms、内存分析通过；视觉匹配 99.982708%；第一次重测的内存失败保留为负面观察，未覆盖 |

## 5. Evidence 与状态

结构化 Evidence：[`verification/evidence/g0/gt-g0-13/`](../../../../verification/evidence/g0/gt-g0-13/)。本轮
Evidence 使用 `sourceCommit: WORKTREE`，只能作为工作区验证结果；实现提交后仍需重新生成
commit-bound Evidence。`ios/ios-physical-report.json` 与 `ipados/ipados-physical-report.json` 是
两份独立物理报告，不能合并成一个平台报告。

GT-G0-13 当前状态：

- Design：`Pass`
- Implementation：`Validating`
- Validation：`Pass`（iPhone 与 iPadOS physical 均 Pass；两份 Evidence 独立）
- Final：`Validating`

G0 与 R1 继续保持 `Validating`；本轮不进入 `GT-G0-14`。
