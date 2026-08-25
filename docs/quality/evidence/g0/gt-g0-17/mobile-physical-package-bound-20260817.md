# GT-G0-17 mobile physical package-bound authority

这份报告采用 G0 的 package-bound physical authority 规则，不要求设备再次安装当前 `main` 提交。真机运行来自一个已识别的安装包/SDK 组合，并与固定 fixture、Skia commit 和 POC digest 绑定；当前 `main` 的代码变化另由 commit-bound CI 覆盖。

## Evidence identity

- package/source baseline：`5ab8b16bdac8f982a9d221d1f48d3867dda7b43c`
- Skia：`b6d106297ff9ef2ff8094033695d045e87775581`
- Skia SDK set：`debcbb7b9376806c94ffb9af5950ebd8a6de0547833f9b57df96a20531ca7817`
- fixture digest：`47826449b895ac4f4a57b4f386379775`
- result bundle SHA-256（`SHA256SUMS`）：`0fbd18514c88344b8f31abcc27811cdc695329ef3c2266090312fbf8273e379a`

## Device results

| 平台 | 设备 | backend | digest | lifecycle | smoke | visual | package identity |
| --- | --- | --- | --- | ---: | --- | --- | --- |
| iPhone | iPhone 15 Pro / A17 Pro | Metal | `47826449b895ac4f4a57b4f386379775` | 100 | 60 s / 3600 frames | 99.982708% | `ios-installed-5ab8b16-skia-debcbb7` |
| iPadOS | iPad Air 4 / A14 | Metal | `47826449b895ac4f4a57b4f386379775` | 100 | 60 s / 3600 frames | 99.982708% | `ipados-installed-5ab8b16-skia-debcbb7` |
| Android | OPPO PKD130 / Android 15 | GLES3 | `47826449b895ac4f4a57b4f386379775` | 100 | 60 s / 3600 frames | 99.982708% | `android-installed-5ab8b16-skia-debcbb7` |

每个平台的原始结果、RGBA、视觉指标和 `SHA256SUMS` 保存在本地物理设备证据 bundle `out/evidence/poc01-mobile-physical-2026-08-17/`；本报告记录其身份和摘要，不把设备序列号、签名身份或私有安装包写入仓库。
