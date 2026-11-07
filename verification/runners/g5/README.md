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

## 放大、暂停、再次缩小／平移回归

`G5MixedNavigationReentry` 使用真实 SkiaRenderer/provider 和轻量混合场景：
已解码 PNG、矩形／椭圆／三角形、便签、表格及 5 条曲线。固定覆盖再次缩小／
同层平移、单次／连续 3 次精确帧完成、选中／未选中，共 8 个组合；另检查
Scene 变化、Surface generation 变化、不同 provider 和显式缓存清理后的旧图拒绝。
断言下一次有界工作机会即 present、宽图中可见的图片像素保留、feedback 不发出
canonical receipt，以及最终 canonical 与 Direct 路径逐像素一致。

```sh
ctest --test-dir out/host -R 'G5(RetainedZoomReentry|MixedNavigationReentry)$' --output-on-failure
```

它们也包含在 `G5RenderCache` 和 G5 原生验证子项目中。已有私有 WebGL probe 的
`--reentry-only` 同样运行这些用例。单元测试不 sleep 或依赖墙钟阈值：Render
没有 idle timer，精确帧完成到下一次调用之间不做工作即可覆盖其保留状态。

真实 1000ms 暂停、大图及公共 SDK wheel 路由继续通过浏览器回归运行：

```sh
G5_USER_CORPUS=1 G5_PINCH_DELTA=1 G5_EXPECT_RESPONSIVE=1 \
python3.12 verification/runners/g5/bounded_worker.py \
  --prefix out/reentry-zoom-worker --timeout 180 --rss-mib 3072 -- \
  node verification/runners/g5/pause_resume.mjs /absolute/path/to/diagnostics-on/js out/reentry-zoom zoom
```

将末尾 `zoom` 改为 `pan` 并使用新的 output/prefix 可测再次平移。诊断 OFF 包不传
`G5_EXPECT_RESPONSIVE=1`，其 native accepted-submit 数据为 unavailable，不能当作
0ms。计时、worker 清理、文档状态和最终像素都保存在结果中；本地响应诊断不等于
物理输入到显示延迟或 G5 Gate PASS。当前修复基线及 14 组结果见
`verification/evidence/performance/g5-pause-resume-repair/`。

## 合并主线后的编辑／导航基线

`post_edit_navigation.mjs` 在同一混合文档中执行复制粘贴、变换、三次不同位置的
局部擦除、便签文本、表格文本和图片资源替换。每一步检查真实 generation／operation／
presented receipt、Undo/Redo 的 committed export 像素往返，以及随后的平移／缩放。
最终再覆盖选中／未选中的 1000ms 暂停后缩小及平移。图片为约 2.33MB 的确定性
PNG，笔画经实际公共 SDK 输入生成。没有 JS Brush／Scene 几何实现。

```sh
python3.12 verification/runners/g5/bounded_worker.py \
  --prefix out/post-edit-dpr2-worker --timeout 180 --rss-mib 3072 -- \
  node verification/runners/g5/post_edit_navigation.mjs /absolute/path/to/matching-sdk/js out/post-edit-dpr2 2
```

分别使用匹配的 ON/OFF SDK 和新的输出目录执行 DPR1/2；DPR4 单列为压力观察。
加 `G5_CONTEXT_RESTORE=1` 可在编辑后触发实际 WebGL context loss/restore，再检查
文档／像素与导航。`firstReceiptWaitMs` 从 restore 请求开始计时，不包含显式100ms
loss 等待；`completeMs` 包含该等待与后续安静帧。ON 表面 generation 来源于既有
frame diagnostic，OFF 保持 unavailable，不新增公开调试 API。

所有 readback/export/snapshot 都在 timed navigation 之外。raw GL 内容调用、
Runtime accepted-submit、精确 receipt 与物理显示延迟保持独立。计数、失败、worker
上限、source/artifact 身份与清理结果须一起保留。单次通过不是统计性能资格。

## 复制长线拖动与 live DPR 像素诊断

`drag_copies.mjs` 通过公共 root SDK 画7200个样本，复制8份，拖动最后一份48次。
`drag` 记录同步输入时间、RAF frame间隔、GPU内容调用和最终Operation/receipt；
`G5_PROFILE=1` 只对拖动阶段录CPU profile，不能混入普通延迟对照。

```sh
G5_PROFILE=0 python3.12 verification/runners/g5/bounded_worker.py \
  --prefix out/copies-drag-worker --timeout 180 --rss-mib 3072 -- \
  node verification/runners/g5/drag_copies.mjs /absolute/path/to/matching-sdk/js out/copies-drag drag 2
```

将 `drag` 改为 `pixel` 会使用128样本的小场景，在擦除contact仍按下时读取远离
擦除区域的ROI，检查原有5×5不透明内部像素保持不变，避免仅在抬笔恢复后验收。
最后参数可为DPR1/2/4。像素捕获与计时分开；`MEASURED` 只代表观察完成且状态
检查成立，不代表拖动达到性能目标或任何正式Gate通过。
私有真实GPU测试 `--live-dpr-only` / `--translated-v4-only` 进一步对比独立CPU
几何/AA oracle，检查半透明、自交、mask和缓存/coverage计数。
