# FC-02 语义设计诊断

日期：2026-10-19；源码基准 `d15acacceda1cf12f8d89846b15a4ec44dbd3cc0`。
本记录证明设计选择的原因，不是 Gate、PHYS 或 FC-02 实现验收。

## 实际执行：旧文字步骤不能保证精确 Undo

命令（从独立 worktree 根目录）：

```powershell
& docs/planning/fc02/diagnostics/run_inverse_probe.ps1
```

先在现有构建目录增量构建仓库 Semantic consumer target 和 RichText 测试，再运行诊断；
该脚本消费 `out/fc01-closeout/build/runtime/semantic/canvas_runtime_semantic.lib`，
未重建 Semantic SDK、protobuf 或 Skia。工具链为 VS2026 BuildTools 18.10.2 / MSVC19.51.36260
x64；编译选项 `/std:c++20 /EHsc /W4 /WX /MT /O2`，CRT 与现有库一致。
首次诊断使用 /MD 与 /MT 库不匹配，产生 LNK2038；改正诊断脚本为 /MT 后成功运行。

输入：同一 paragraph 的两个 run `A,S`、`B,S`；调用真实 normalizer、validator；
在 scalar1 插入 `X,S`，随后删除 scalar1 的一个 scalar。

实际输出：

```text
normalized=true valid=true
before_runs=2 inserted_runs=1 inverse_runs=1
inverse_text=AB exact_preimage=false
CONFIRMED: reverse V1 text steps do not guarantee exact canonical Undo
```

| 检查 | expected | actual | 结果 |
| --- | --- | --- | --- |
| 前像是合法规范 V1 | normalize/validate 接受且保留两 run | 一致 | PASS |
| 假设：Insert/Delete 反向步骤精确恢复前像 | 两 run，完整结构相等 | 一 run，结构不等，文字仍 AB | **FAIL / RED：该设计假设不成立** |
| 诊断识别上述反例 | 非精确且文本仍 AB，退出码0 | 一致 | PASS（诊断本身） |
| 新 delta2 Restore 精确 Undo | 完整前像恢复 | 尚无生产实现 | IMPLEMENTATION_PENDING |

这是新 editor 的 Undo 设计反例，不把冻结 V1 的既有正常化规则追认为违规。
脚本退出码0表示反例被确认，不能把它换写为“生产文本 Undo PASS”。

原始日志：`out/fc02-semantic-contract/compile.log`、`inverse-probe.log`。
诊断 exe：`out/fc02-semantic-contract/rich_text_inverse_probe.exe`；SHA-256：
`3fed495e8b62a6a43cf2e1be188c640d72242e1430af5e2078f4b2ad33de6c68`。
它是独立 CLI 诊断，不是已更新的 Minimal Host 产物。重编时 PE 时间戳可能改变 exe hash。

## 本轮执行的基线回归

在 VS2026 x64 developer environment 下：

```powershell
cmake --build out/fc01-closeout/build --target canvas_runtime_semantic canvas_semantic_rich_text_state_plan_test
& out/fc01-closeout/build/runtime/semantic/tests/canvas_semantic_rich_text_state_plan_test.exe --gtest_output=xml:out/fc02-semantic-contract/rich-text-baseline.xml
$fc02Facts = Get-Content out/fc01-closeout/build-environment.json -Raw | ConvertFrom-Json
$env:AXIOM_PROTOC = $fc02Facts.environment.AXIOM_PROTOC
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract -v
```

结果：consumer/test target 构建成功；RichTextStatePlan 12/12 PASS（含 scalar、跨 run 删除、
split/merge/style、第二步失败不改输出）；schema/current 与历史 descriptor 检查 9/9 PASS，
包括使用锁定 protoc 的真实负例。原始日志在 `out/fc02-semantic-contract/`：
`build-baseline.log`、`rich-text-baseline.log`、`schema-tests.log`。
这些是现有行为与受保护合同的基线，不是新增 V2 功能 PASS，也不代表全量 CI。

## 只读合同核对

已从远端取回精确 `057464f9fab2db4b67c032134cce3f5f417a64e0`，读取原包
`capability_registry.v1.yaml`、`snapshot_binding.v1.md`、`digest_projection.v1.md`；
没有 checkout/覆盖工作树，也没有修改这些历史文件。当前仓库中 ResourceManifest
implementation-context 明确绑定该 accepted revision；原文件标题中的 candidate 不改写。

| 检查 | expected | actual | 结果 |
| --- | --- | --- | --- |
| Snapshot3 Triangle 与资源组合 | 对象所需 capability 可组合 | codec/bootstrap 只允许 Triangle 在 schema2 | FAIL：已定位集成缺口，未修复 |
| schema3 required_ids 准入 | 严格解析、检查支持集合及实际需求 | 目前只判 capability bytes 非空 | FAIL：已定位集成缺口，未修复 |
| semantic digest 与绑定投影一致 | accepted framing/domain 投影 | 当前 snapshotDigest 使用裁剪 protobuf hash | FAIL：精确差异已定位；新持久化 writer 先 fail closed |
| Windows/Web IME、宽度与混合恢复 | 按新合同运行 | 本轮未构建产品实现、未人工操作 | PHYSICAL_PENDING |

上述静态发现不能冒充已执行的混合 Snapshot 回归；后续应补真实 RED 后窄修复。
Authority/Golden、历史 Gate/PHYS 和延期 Android 状态保持原样。
