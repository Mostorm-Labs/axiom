# GT-G0-17 G0 Gate Review Revalidation

## 结论

本次复核绑定合并后的 `main` 提交 `a8d72489028e650def33630a979d05a2bbc0bf26`。在 clean checkout 中重新构建验证 workspace，并重新生成 G0 Gate Report。physical authority 按已确认的 package-bound 规则判定：不要求每个平台都重新构建当前提交，只要求安装包身份、fixture/corpus digest、artifact hash、设备环境和运行结果可核对。

- E1：`PASS`
- E2：`PASS`
- E3：`PASS`
- E4：`PASS`
- G0：`PASS`
- promotion：`true`

Windows D3D12、Windows 上的 Chrome Stable WebGL2、Android、iPhone 和 iPadOS 均被本次 lineage 以 `PHYSICAL` 记录。移动端使用已验证安装包的 package-bound 结果；当前 `main` 的代码覆盖由 commit-bound E1～E3 证据提供，因此不重复要求 Windows/Web 或移动端真机重测。

## 重建与校验

- Source commit：`a8d72489028e650def33630a979d05a2bbc0bf26`
- Branch：`main`
- G0-00～G0-15：16/16 `Pass`
- Clean checkout：`npm ci --ignore-scripts`、verification build、gate-report 均成功
- G0 Gate Report：`verification/evidence/g0/gt-g0-17/g0-gate-report-main-a8d7248.json`
- Report SHA-256：`65303b45497387ddc6e2d82eb6ca33cc7cfe1f0eca7fc7acf75b1bcfb670e497`
- Full artifact/input manifest：`verification/evidence/g0/gt-g0-17/revalidation-manifest.json`

## Platform authority

| 平台 | 本次状态 | Evidence |
| --- | --- | --- |
| Windows D3D12 physical | `PASS` | [PR #42 Windows/Web report](gt-g0-17/windows-web-physical-20260826.md) |
| Web Chrome Stable/WebGL2 physical（Windows 设备） | `PASS` | [PR #42 Windows/Web report](gt-g0-17/windows-web-physical-20260826.md) |
| Android physical | `PASS` | [package-bound authority](gt-g0-17/mobile-physical-package-bound-20260817.md) |
| iPhone physical | `PASS` | [package-bound authority](gt-g0-17/mobile-physical-package-bound-20260817.md) |
| iPadOS physical | `PASS` | [package-bound authority](gt-g0-17/mobile-physical-package-bound-20260817.md) |

## 状态纪律

本报告不覆盖 `gt-g0-17-gate-review-20260825.md`，也不把历史 `BLOCKED` 改写为 `PASS`；它是按新规则生成的独立 revalidation。GT-G0-17 与 G0 在本次 revalidation 中达到 `Pass`，可以进入 G1；R1 仍需按其独立退出条件判定。
