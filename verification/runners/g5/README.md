# G5 preparation runners

这是用户授权的前置审计与测量准备，不执行 G5 优化，不签发 Gate。owner/前置核对见 `docs/planning/G5_PREPARATION.md`。

从仓库根目录执行：

```bash
python3.12 verification/tools/g5_preflight.py audit --output out/g5-preparation/audit.json
python3.12 -m unittest verification.tests.test_g5_preflight -v
node --test verification/tests/g5_performance.test.mjs
```

Web SDK 路径必须用同一次 Release 构建，不能把旧 WASM 搭配当前 wrapper：

```bash
python3.12 verification/tools/g5_preflight.py web \
  --output out/g5-preparation/web \
  --web-toolchain /absolute/path/to/emscripten/cmake/Modules/Platform/Emscripten.cmake \
  --skia /absolute/path/to/accepted/skia-sdk/web-wasm-webgl2/release \
  --browser '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' \
  --samples 64,256,1024,4096 --repeats 5
```

工具通过 `setup_build_environment` 的 offline v2 Store resolver 与共享 CMake consumer arguments 获取 Semantic；使用调用者提供的已接受 Skia SDK，记录 manifest 和 lock hash。不会 bootstrap Protobuf/Skia。当前 Store 不完整时先修复 resolver/Store，不添加替代依赖。

输出包括 `source-before/built/after.json`、configure/build/browser logs、`sdk-artifacts.json`、`browser.json`、计时结束后截图、`result.json` 和 SHA256 `manifest.json`。`audit` 返回零表示审计执行成功，`g5_admission` 仍可 BLOCKED。`web` 只有 source/artifact/receipt 检查均成功才返回零和 `MEASURED`。

首次 smoke 可用 `--samples 64,256 --repeats 2`。`--renderer swiftshader` 仅作软件实现对照；默认模式也要检查结果里的实际 WebGL renderer，不能根据选项推断硬件加速。机器负载、headless、CDP pacing 与 observer 开销都限制性能外推。

原始数据不删掉慢样本，warmup 独立保存。不可观测内部阶段为 `null`。idle 检查只检查 canonical callbacks，不冒充 preview submit/present counter 证明。没有 Android/Windows/真实触控设备的运行时，不生成其 PASS。

可单独测量同一 owner 的原生 BrushSession，分离 append/seal/replay，并对照 1/8/64 分批；这不是 SDK、Scene 或 renderer 耗时：

```bash
python3.12 verification/tools/g5_preflight.py native \
  --output out/g5-preparation/native \
  --skia /absolute/path/to/accepted/skia-sdk/macos-arm64-metal/release \
  --samples 64,256,1024,4096 --batch-sizes 1,8,64 --repeats 5
```

该组件 corpus 暂时只冻结 `vector-solid-v1`；不会用输入数代替 compact canonical sample 数。它通过独立 CMake subproject 链接现有 `canvas_runtime_ink`，不修改生产 CMake 或增加公开 hook。seal 与单批 replay 几何用 FNV1a64 做相等性诊断，source/executable/output 的身份仍由外层 SHA256 固定。新 Ink v2 corpus 需要单独冻结后对照，不能混在旧 profile 结果中。

重跑同一 output 会将上一轮顶层 logs/source/result/manifest/screenshot 移到 `previous/<run>/`，保留 build cache。推荐为正式对照的每次测量指定不同 output。

## Ink v2 后的局部化切片

已交付算法基点固定在 `6c46f540`，独立 workload 见
`verification/corpus/performance/g5/ink-v2-locality.json`。`round-handdrawn`
和 `chalk-compact-v1` 的 component 结果与旧 vector-solid corpus 分开。
使用 Python >= 3.10；macOS 上若 `emcc` 默认找到 Xcode Python 3.9，设置
`EMSDK_PYTHON=/absolute/path/to/python3.12`，不要修改锁定 SDK。

累积 browser workload：给 `web` 子命令传
`--profile round-handdrawn --samples 8 --accumulate 32 --repeats 1`。
每次 invocation 是一个新文档；正式重复用不同 output 运行三次。
32 次测量加 1 次 warmup，因此最后实际对象数为 33。

隔离原生验证子项目还提供 `g5_accumulation`（64 笔可见/视口外前缀）、
`G5RuntimeLookupLocality`、`G5CompactCoverageLocality`，以及现有 A6
atomic publication/recovery 与 InkV2 material/runtime 回归。其 CMake 链接生产
owner，结果仍是 host raster。通过 `ctest --test-dir <native-build> --output-on-failure`
执行。`g5_locality_guards raster` 保留旧 full-clip raster 作为逐像素 oracle。

本切片不等于正式 G5 全部前置闭合，未实现 Damage/RenderGroup/Tile。
