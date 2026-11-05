# G5 第三步：chunk 索引、共享 source 与 Scene 快照

结果为 `READY_FOR_CONTROL_REVIEW`，仅指本切片可供审查。没有签发 G5/P34 或真机 PASS，没有发布 SDK。

仓库为 github.com/Mostorm-Labs/axiom，受控分支为 `codex/geometry-navigation-integration`。
优化前 SDK revision 为 `3af0f5259eab12e65543ab079868970508e70e9c`；最终实现、SDK、采样上下文与测试 revision 为 `645052b3801198a0e4efe5177502ba55d92645ac`。
完整输入文件 SHA256 见 `source-inputs.json`，安装包每个资源的 SHA256 见 `consumer-install.json`，私有测试二进制绑定见 `test-inputs.json`。每次浏览器运行同时保留采样上下文 revision 和 SDK 自报 revision，两者用途不同。

执行包为 `docs/planning/G5_CHUNK_INDEX_AND_SHARED_SNAPSHOTS.md`。没有修改 Semantic/Skia lock、schema、旧 Gate 记录、公开 JS/DLL API 或 SDK 版本（仍为0.3.9）。4216 的旧 Demo 保持原来的安装包；新 Demo 为 <http://127.0.0.1:4217/example.html>，使用默认关闭诊断的安装包 `out/g5-shared-reclaimed-off/js`。

## 改动与所有权

- Semantic/OperationEngine 仍拥有文档 mutation。RuntimeScene 的 derived content 改用不可变共享值，reference command 通过别名持有同一个 variant allocation。引用列表、tile、prepared geometry 和 Scene 老快照共享笔画数据；不把 Semantic store 的可写数据别名交给 Render。
- chunk 逻辑 key 为 object、ordinal、point offset/count；版本有效性还必须匹配完整不可变 content lifetime 和 chunk table。不能把范围 key 或 FNV geometry digest 当作内容 authority。相同内容与变换修改复用 payload/index，压力等内容变化失效。
- Scene 拥有 local chunk BVH，Render 做保守的逆变换 broad phase，随后保留原来的 world-space leaf test、margin 和顺序。缺失、stale、奇异或未证明安全的情况使用完整对象 fallback。
- RuntimeScene projection 和 Scene publication metadata 使用 COW table。老读者持有自己的 const view；prepare/rollback 不改旧发布值，commit 仍通过原来的 publication gate/noexcept 路径。没有缩小既有依赖闭包。
- canonical、navigation、prepared cache 共用 retention pool，按 allocation owner 一次计费；不同 budget owner、不同内容版本分别计费。最后一个缓存/在途使用者退出后释放 lease。
- 导航开始时回收已经空闲的 exact scratch/coverage targets；正在执行的 raster job 不可回收。保留 canonical pixels、source、GPU quota、frame/query/memory budget 和 receipt 规则。

主要实现位于 `runtime/scene/include/canvas/scene/shared_scene_value.hpp`、`runtime/scene/src/scene.cpp`、`runtime/scene/src/geometry_chunk.cpp`、`runtime/render/src/chunk_query.hpp`、`runtime/render/src/source_retention.hpp` 及 Skia renderer/cache。

## 可重复的结果

| 检查 | 结果 |
| --- | --- |
| 稀疏长线，65,536 / 262,144 knots | 均检查8个 chunk，分别访问11 / 15个 index nodes；原线性循环分别需遍历256 / 1024个 chunk |
| 同一65,536点 source 的第二个 tile | Recorded 计费2,127,231 → 2,127,886 bytes，仅增加655 bytes；退休后 budget 为0 |
| 不变 source / snapshot | buffer identity 共用；连续发布、变换、内容更新后旧读者保持有效；完整内容改变使旧索引失效 |
| 回收预算回归 | 同一1MiB soft limit，保留 active raster，完成后回收262,144 bytes，使 prepared path 恢复获准，最终 budget 为0 |
| 真实 root SDK 浏览器 | 18次 MEASURED 配对运行、39组导航最终像素配对、2次编辑最终像素配对，全部一致；source/artifact guard 均通过 |
| 九对象 group update | 诊断 ON 矩阵每个 phase 的累计 CPU 中位数：选中2.8 → 0.4ms，未选中2.8 → 0.45ms；不是总帧耗时 |
| 默认 OFF，九对象 | 本轮导航提交空窗约18–22ms；冻结恢复18.8 → 21.6ms。此为合成输入/GL submission 观察，不是 SLO 或物理显示延迟 |
| 两次真实新增笔画 | 对象、semantic generation、operation、presented generation 均正确推进；分别复用9 / 10个 payload，只建立1个新笔画索引 |

`paired-results.json` 保留所有 phase 的 gap、settle、group CPU、质量与 source 计数，不剔除尾部。`supplemental-comparisons.json` 包含编辑与发布、光学对照。各目录的 `wheel.json.gz` / console log 含原始观察。光学 readback 与 latency 分开运行。

两次编辑的 dispatch 约38.2/40.1 → 41.5/41.5ms，quiescent completion 约224.1/225.8 → 233.0/221.5ms；没有稳定的编辑总耗时下降。completion 包含 harness 的额外 RAF 等待，不应当作纯发布 CPU。

初次共享预算接线在 `d3f24381` 产物中把 prepared path 挤到拒绝路径，出现约60–68ms恢复空窗。该轮数据完整保存在 `experiments/`，未作为最终结果。回收空闲 exact buffers 后，最终九对象运行恢复3条 prepared contour、0拒绝，冻结恢复约19–22ms。修正不可变 GPU fixture 的失败样本另存为 `immutable-gpu-fixture-red.json`；它是测试 builder 未冻结修改，并非旧 GPU distance-phase 失败。最终修正后的独立 contour 模式通过。

## 检查与未关闭项

完整 native CTest 为1006/1072：失败/未构建的66项与优化前名单完全一致，见 `regression-comparison.json` 和完整日志。包含新增共享数据/索引/预算/旧读者测试以及原有 atomic publication/recovery 回归。

真实 Chrome WebGL2 的 quality、exact replay、affine、background、contour 五个私有模式通过。完整 GPU candidate 仍在既有 `denied geometry lost full-stroke distance phase` 用例失败，后面的用例不能由此推定通过。

完整 Node 为133/144，同一11项因缺少编译后的 platform-harness dist 失败；workspace 的9个测试包仍缺少 tsc。Runtime boundary checker 通过118个 contract files。正常 ON/OFF SDK 包各36个文件，没有 Runtime headers、私有测试导出或独立 dependency archives；OFF WASM 不含 navigation/scene timing 标记。

仍然保留以下边界：

- full compiler/recovery materialization、lookup construction 和 COW 首次 metadata 写仍可能为线性工作；大量重叠 chunk 仍可能全部命中。没有100K/1M规模资格结论。
- `source_copy_bytes` 只描述被观察的 reference source payload copy，不是全进程复制量；Semantic、full compiler 和 metadata 的复制不能被这个0掩盖。
- `metadata_rows_copied` 只计 COW table detach，不计所有 compiler materialization；这两次混合文档编辑进入全量路径，所以该项为0不代表没有 metadata 建造。
- WASM计时存在量化，prepare/publish计时0不代表零成本。新增编辑观测的 prepare 为0/0.1ms、publish为0，不能据此签发发布耗时保证。
- 九对象精确收敛通常仍约0.7–0.8秒，冻结后仍约2.7–2.9秒；大幅缩放仍可放大低清 retained plane（约4.22倍）。本轮没有解决这些长尾和缩放细节债务。
- 60秒前台闲置与 CDP freeze 不是物理休眠、GPU eviction 或长期内存泄漏证明。公开 `onFrame` 是 coarse post-input receipt，不是 camera-qualified receipt。
- G3 snapshot authority、全套旧失败、跨平台实机/ABI qualification 均未被此切片升级为通过。

## 本地复核

先复用现有已锁定的 Semantic/Skia SDK 和既有 `out/host`、`out/web-sampling`、`out/web-sampling-off` 配置；不要为了复核重建这些依赖。

```sh
cmake --build out/host -- -k0 -j3
ctest --test-dir out/host --output-on-failure -j3
node --test verification/tests/*.test.mjs
python3 tools/check_runtime_boundaries.py --root .
node verification/runners/g5/navigation_quality_matrix.mjs out/g5-shared-before-on/js out/g5-shared-reclaimed-on/js out/g5-shared-final-matrix-on on
node verification/runners/g5/navigation_quality_matrix.mjs out/g5-shared-before-off/js out/g5-shared-reclaimed-off/js out/g5-shared-final-matrix-off off
```

不要在 latency 采样时并行构建。光学模式使用 `optical`；bounded optical 与 edit-publication 的完整选项在各自 summary/raw JSON 中，其中编辑通过 `AXIOM_NAV_EDIT_SAMPLES=2` 启用。`collect_evidence.py` 从冻结的本地 out 产物收集、校验对比结果；`SHA256.json` 覆盖本目录除它自身之外的全部文件。
