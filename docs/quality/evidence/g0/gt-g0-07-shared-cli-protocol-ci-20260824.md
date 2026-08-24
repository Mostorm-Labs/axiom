# GT-G0-07 Shared CLI 与 Protocol CI Gate Evidence

> 任务：`GT-G0-07`（Notion locator：`WP-G0-07 / IH-07`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 状态：`Validating`；本地 commit-bound 验证已收口，托管 CI 结果待完成

## 1. 目标与边界

本任务建立共享 `axiom-platform-conformance` CLI 和 `platform/protocol-seed` CI gate，作为
后续真实平台 scenario job 的前置条件。`validate` 与 `protocol` 本轮完整可用；`list`、
`profile`、`run`、`compare`、`aggregate` 保留稳定的 `NOT_IMPLEMENTED (30)` stub。

本任务不实现真实平台 adapter、真实 transport、性能门禁，也不把验证协议升级为产品 ABI。
CLI 禁止 `--bless` 和 `--update-golden`；协议 corpus 只能读，Evidence 输出必须位于 corpus
之外。

## 2. 交付物

- `verification/packages/platform-conformance-cli/`：TypeScript CLI 包、稳定 exit codes、命令
  解析、schema/corpus validate、双 boundary protocol 执行、ProtocolMetaResult 与完整性验证。
- `verification/tools/run_protocol_vectors.mjs`：支持 `IN_PROCESS`、`OUT_OF_PROCESS` 单独或双边界
  执行；保留 corpus integrity 与测试注入钩子。
- `.github/workflows/g0-platform-protocol-seed.yml`：逻辑 job `platform-protocol-seed`，执行
  clean install、build、validate、双 boundary protocol、corpus read-only 检查和 artifact 上传。
- `verification/package.json` / `package-lock.json`：workspace CLI 入口和 Node 类型依赖锁定。

## 3. 命令与结果契约

| 命令/结果 | 语义 |
| --- | --- |
| `validate` | 运行 13 份 schema/fixture 校验与 protocol vector meta-tests |
| `protocol --suite protocol-seed-v0.1 --boundary in-process --boundary serialized-loopback` | 执行 56 vectors × 2 boundaries，生成 112 个 ProtocolMetaResult、`protocol.log`、`summary.json` 与 `corpus-integrity.json` |
| `0` | SUCCESS |
| `2` | INVALID_ARGUMENTS；包括未知参数、非法 boundary、`--bless`/`--update-golden` |
| `10` | INVALID_SCHEMA_OR_CORPUS |
| `20` | INVALID_EVIDENCE；包括缺失/损坏/不完整结果、完整性 hash 漂移、corpus 变更 |
| `21` | RUNNER_EXPECTATION_MISMATCH；runner 返回非零或出现 FAIL 结果 |
| `30` | NOT_IMPLEMENTED；保留命令 stub |

Protocol verifier 会校验 suite membership、每个 boundary 的完整文件集合、每个结果的严格键集、
vector/boundary 身份、manifest 的逐条结果、数量和 SHA-256。任何缺失、额外或损坏 Evidence
都不会被当作通过。

## 4. 本地验证

使用新安装的 workspace 依赖执行，与 CI 命令一致：

```text
npm ci --ignore-scripts
npm run build
npm exec -- axiom-platform-conformance validate
npm exec -- axiom-platform-conformance protocol --suite protocol-seed-v0.1 \
  --boundary in-process --boundary serialized-loopback \
  --output /tmp/axiom-gt-g0-07-protocol-evidence-20260824
```

结果：

```text
workspace build/typecheck scaffold: valid
schema validation: 13 schemas and 13 fixtures valid
protocol vector/meta tests: 63 passed
protocol execution: 112/112
corpusHashBefore = corpusHashAfter = 87ce3451ae6d005453ba7061d15f5b6338869e60a306e2d105bb920116bbcd3b
integritySha256 = ee2f85dd532ee59abb5bb8e1079fd92f95f0541797ebdd5bc3e187a3b89decfb
```

CLI package tests：7/7；覆盖帮助/未知命令、保留 stub、禁止 bless、invalid schema/corpus、双边界
通过、runner mismatch、缺失 Evidence、corpus 内输出拒绝。版本化
`verification/platform/protocol/v1` 的 `git diff` 为空。

## 5. 故障分类验证

- `AXIOM_PROTOCOL_MUTATION=duplicate-completion`：稳定返回 `21`，summary 为
  `RUNNER_EXPECTATION_MISMATCH`；不冒充平台实现失败。
- `AXIOM_PROTOCOL_EVIDENCE_MUTATION=missing-integrity`：稳定返回 `20`，summary 为
  `INVALID_EVIDENCE`；不接受部分结果包。
- schema/corpus 校验失败：稳定返回 `10`。
- `--bless`、`--update-golden`、corpus 内输出路径：稳定返回 `2`。

## 6. Evidence 身份与剩余条件

实现提交后已重新运行同一命令并填写 commit-bound 身份。由于托管 CI 尚未完成，任务状态继续保持
`Validating`；CI 通过后再追加 run、artifact 和最终任务状态：

| 对象 | 当前值 |
| --- | --- |
| implementation commit | `2292c16c1fbeb90a5f1cb434d25263ff99de2919` |
| implementation tree | `8ee8db129766fbd01a3b61fef5daced5820ad539` |
| `git archive` SHA-256 | `703f6fa950c6b296d52482d3e28284b74c4041e9d04e6893edfff3bb242bbdb8` |
| CLI package SHA-256 | `184833d6a7bc5dae5b328889c66dd30d163a956db93aee7feb40cd839f4be52b` |
| CLI entry SHA-256 | `b709ce7f35464eb8cb35bd4e6b6b577da1d99c62cae272fa54943cdca4a3ceb4` |
| protocol verifier SHA-256 | `f1d5027c2a290a49eabe8ba1b910b54a7d689e684e2f5905b88b2eaa421c9fa3` |
| vector executor SHA-256 | `281681189379ac0100d5758638e6c975453dc84718295c940c69cf783717bc7e` |
| workflow SHA-256 | `9ad25ca35f2093c0f3776411036dd9e9d53e02173e691a54e4d02169d4044d9d` |
| summary.json SHA-256 | `510077a2490df57faed43049919f330512d969fd6b49a727431ff45066e5a9a0` |
| corpus-integrity.json SHA-256 | `9c2417d2e08081410d49a2ff555cf5f847ee7fe35de46badf5de3d822ebb5ba4` |
| protocol.log SHA-256 | `5909cee682480dce9a23ca04dc58d9a86284dfd72856c0d0df25c85d4df8cad4` |
| protocol integrity SHA-256 | `ee2f85dd532ee59abb5bb8e1079fd92f95f0541797ebdd5bc3e187a3b89decfb` |
| hosted CI run / artifact | `Pending` |

`GT-G0-08` 保持 `Not Started`。G0 与 R1 仍不能晋级；必须完成后续 G0 任务并执行 G0 Gate
Review，不能以本任务单独 Pass 代替 Gate Pass。
