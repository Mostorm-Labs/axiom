# G5 preparation execution results

本轮完成用户授权的前置核对和可重复测量工具准备。没有实现 G5 optimizer，没有修改 production Runtime/SDK、第三方依赖或旧 Gate。Ink v2 在相同 worktree 的其他改动不属于本轮交付。

## 核对结果

`out/g5-preparation/audit.json` 记录 21 项前置绑定、6 条保留 reference 路径、G4.x/FC 本地记录及可解析 revision ancestry。前置结论为 `BLOCKED_UNVERIFIED_PREREQUISITES`：保留 G4 历史 functional PASS_WITH_FINDINGS，同时不从 G4.x readiness、DEV 实现、hosted build 或 source presence 推导完整 G5 准入。

Semantic native/Web runtime 从 v2 SDK Store offline 解析；没有下载或重编 Protobuf。Skia 使用当前 lock 对应 release SDK ID，manifest/lock SHA256 随执行结果记录。永久 reference、当前 Damage owner、full projection fallback 等具体差距见 `G5_PREPARATION.md`。

## 实际运行

- 原生 `vector-solid-v1` component：64/256/1024/4096 点 × 1/8/64 batch × 五次，共 60 个正式 run，另有 warmup。最终 `out/g5-preparation/native/result.json` 为 `MEASURED`，source-before/after 完全一致，样本数和 seal/fresh-replay 几何校验均通过。它只测 BrushSession，不包含 SDK、Scene、Skia 或实际显示。
- Web：matching Release SDK consumer 已实际在 Headless Chrome / ANGLE Metal Apple M2 跑通 64/256 点 smoke，以及 64/256/1024/4096 点各三次的长笔画诊断。事件保留、commit、canonical receipt、独立 targets、idle callback 与 close 检查通过。但长笔画执行期间工具/Ink v2 源码变化，外层归类 `SOURCE_CHANGED`，不能作为正式前后对照基线。原始 browser 记录与 screenshot 保存在 `out/g5-preparation/web-smoke/previous/`。
- 最终 Web 重编译收到终止信号 `-15`，源码树同时变化，`out/g5-preparation/web-smoke/result.json` 保留 `SOURCE_CHANGED`。未反复追逐并发版本；Ink v2 稳定 revision 后按 README 重采。

组件当前单样本 append 的历史复制量随 stroke 增长，4096 点达到 4096 个历史样本。浏览器诊断也显示 up 的组合提交耗时随笔画增长。它们提示后续同时采样 append、seal/replay、Operation/Scene 和 canonical handoff；不能把组合指标直接解释为某一个子系统的独占成本。

这些结果不代表 1K/10K/50K/100K 大画布 profile、FC-12 mixed、真实设备 input-to-photon 或其他平台。未暴露的精确内部阶段、GPU time/upload bytes、preview present counters 明确为 `null + reason`。

## 验证记录

新增 Python 8/8、Node 3/3 通过；verification workspace valid。原生 measurement C++ target 编译并完成上述 60 个 run。新增文件没有尾随空白；全 worktree diff check 在并发 Ink v2 的 CRLF/whitespace 行报告失败，本轮未改写这些文件。

全量 Python verification 最后执行 193 项：187 通过、4 失败、2 skipped。日志 `out/g5-preparation/python-tests-final.log`；失败均显式保留：

1. `G102RDescriptorRefreezeTest.test_every_descriptor_change_maps_to_a_closed_machine_projection_defect`：defect-set 缺 `G45-C4-SR01`。
2. `G102RSemanticDifferentialTest.test_leaf_fixture_differential_compares_wire_and_semantic_projection`：默认路径缺预构建 `canvas_semantic_golden_probe`。
3. `ImportedFontCatalog.test_catalog_bytes_licenses_and_ids_are_exact`：catalog 13 fonts，旧断言期望 11。
4. `SemanticPublicBoundaryTest.test_object_record_leaf_values_are_typed_and_not_opaque_blobs`：现有 semantic header 的 `std::vector<std::uint8_t>` 触发旧结构断言。

`npm run validate` 的前序 117 项及其他已执行 workspace suites 通过，但 semantic-conformance-cli 的 `C8 generator consumes the native observation envelope and emits exactly eight artifacts` 因 `spawnSync git ENOBUFS` 失败。日志 `out/g5-preparation/node-validate.log`，该命令整体失败。未以本轮工具测试通过代替全仓库回归通过。

工具运行命令和指标解释见 `verification/runners/g5/README.md`。后续顺序是固定 Ink v2 source/artifact identity → 在稳定机器负载下重采 Web/组件 → 补齐正式 G5 acceptance bindings → WP-G5-00 fixture/profile freeze；本次不签发 P34/Gate PASS。
