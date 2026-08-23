# GT-G0-01 验证工作区骨架记录

> 任务：`GT-G0-01`（Notion locator：`WP-G0-01 / IH-01`）
>
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation
>
> 执行时间：2026-08-23（Asia/Shanghai）
>
> 原始结果：**PASS（已由后续依赖审计判定为不完整）**
>
> 现行结果：**PASS（完成 revalidation）**。原始误判、补齐和复核过程保留在本记录中。
>
> 基线提交：`1579a6e364680acbc0b46d1a2cecb89fb9196c4a`

## 1. 术语与任务边界

- **Verification workspace（验证工作区）**：承载 corpus、fixture、runner、report、Evidence 和
  schema 的仓库目录边界。它不是产品 Runtime，也不构成产品公共 ABI。
- **Workspace manifest（工作区清单）**：声明工作区身份、目录、语料类别、owner 与治理规则的
  版本化 JSON 入口。
- **Schema skeleton（Schema 骨架）**：只冻结 workspace manifest v1 的字段和拒绝规则；不冻结
  protocol envelope、Operation wire format、平台场景或 Gate Report schema。
- **Reserved（预留）**：目录和 owner 已建立，但后续负责的任务尚未物化 schema、vectors 或
  runner。Reserved 不能被解释为实现或验证已完成。

`GT-G0-00` 已在提交 `1579a6e` 标记为 `Pass`，因此本任务上游依赖满足。本任务只建立
`verification/` 骨架及其自验证；本次复核后 `GT-G0-02` 的上游条件已满足。

## 2. 设计与实现结果

新增的 `verification/workspace.json` 是唯一工作区入口，固定以下顶层目录：

| 目录 | 用途 | 当前状态 |
| --- | --- | --- |
| `verification/corpus/` | protocol、semantic、platform、golden、performance、fault、physical 语料分栏 | `reserved` |
| `verification/fixtures/` | FakeClock、DeterministicRandom、StableIdFixture、FakeResourceProvider | `reserved` |
| `verification/runners/` | native/reference/WASM/platform runners | `reserved` |
| `verification/reports/` | 后续 Gate Report 及聚合结果 | `reserved` |
| `verification/evidence/` | 可复现验证产物索引 | `reserved` |
| `verification/schema/` | 验证基础自身 schema | workspace manifest v1 已建立 |
| `verification/schemas/platform/` | Platform/Protocol JSON Schema Draft 2020-12 | 13 份 schema 已建立 |
| `verification/platform/` | 平台验证协议说明与后续 adapter 边界 | `reserved` |
| `verification/packages/` | 验证 workspace packages 根目录 | `reserved` |

工作区 manifest 使用 `schema_version = 1` 和
`format = axiom-verification-workspace`。它不保存绝对路径、机器时间、临时 runner 状态或构建
产物 hash。规范化 identity 使用 workspace-v1 规则：UTF-8、对象 key 递归排序、紧凑分隔符；
当前 manifest canonical SHA-256 为：

```text
4988903650a231b7e67a53646602018fed8327fa308d721be7f5cee51fe1efc2
```

`workspace-manifest.schema.json` 使用 JSON Schema Draft 2020-12，拒绝未知字段，并限制版本、
格式、相对路径、corpus entry、service entry 与 policy。仓库不新增第三方 JSON Schema runtime
依赖；G0-01 的无依赖 Python validator 解析 schema 元数据并显式执行本版全部工作区不变量。
以后若引入通用 schema engine，仍必须保留相同拒绝语料，不能放宽现有约束。

IH-01 的 Platform/Protocol schema 清单已 materialize 为 13 个文件（来源标题中的“12”按清单
与上层实施计划解释为计数笔误），每个 schema 均有一个合法 fixture。统一入口为
`cd verification && npm run validate`；workspace 还提供 `npm run build` 与 `npm run typecheck`。
本次使用仓库既无既有 package-manager convention，故采用 npm，并锁定 TypeScript `7.0.2`。

## 3. Ownership 与后续任务隔离

| 资产 | Owner | 本任务结论 |
| --- | --- | --- |
| protocol corpus / envelope codec | `GT-G0-02` | 仅预留目录，不冻结字段、编码或 vector |
| runner core / transport | `GT-G0-03..GT-G0-05` | 仅预留 runners 目录，不实现 lifecycle |
| semantic/protocol vectors | `GT-G0-06` | 未生成 56 个 vectors |
| shared CLI / CI gate | `GT-G0-07` | 未建立 CLI 或 CI workflow |
| native hooks / golden | `GT-G0-08` | 仅登记 golden ownership |
| platform/performance corpus | `GT-G0-09` | 未生成 28 个平台场景 |
| Web/Windows/Android/Apple adapters | `GT-G0-10..GT-G0-13` | 未实现平台 adapter，也未伪造物理设备结果 |
| CI、Gate Report、aggregator | `GT-G0-14..GT-G0-16` | reports/evidence 目录保持 reserved |

Golden policy 已在 manifest 和目录说明中固定为：blocking CI 只读 expected；更新必须通过单独
评审变更。本任务没有加入自动 bless 或 expected 生成路径。

## 4. 验证语料与结果

执行命令：

```text
python3 verification/tools/validate_workspace.py --print-digest
python3 -m unittest discover -s verification/tests -p 'test_*.py' -v
python3 -m py_compile verification/tools/validate_workspace.py verification/tests/test_workspace.py
cd verification && npm run validate && npm run build && npm run typecheck
```

最终结果：

```text
schema validation: 13 schemas and 13 fixtures valid
4 schema meta-tests passed
workspace build/typecheck scaffold: valid
workspace: valid
manifest_sha256: 4988903650a231b7e67a53646602018fed8327fa308d721be7f5cee51fe1efc2
Ran 9 tests
OK
```

9 项测试覆盖：

1. 仓库 manifest 正常通过；
2. 同一 manifest 连续计算得到同一 SHA-256；
3. 未知顶层字段拒绝；
4. 未知嵌套字段拒绝；
5. 绝对路径、Windows drive path 和 `..` 父路径拒绝；
6. 错误 corpus owner 拒绝；
7. 重复 corpus ID 拒绝；
8. 缺失目录拒绝；
9. JSON Schema 可解析、Draft 正确且顶层 `additionalProperties=false`。

首次测试运行曾出现 1 个失败：校验器当时只检查 owner 的 Task ID 格式，未检查特定 corpus 与
owner 的固定映射。随后改为显式 `CORPUS_OWNERS` 映射并增加重复 ID、嵌套未知字段和缺失目录
测试；最终 9 项全部通过。这里保留该修复轨迹，不把首次失败冒充成从未发生。

## 5. Evidence 等级与可复现性

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | workspace/schema/路径/ownership 规则及拒绝语料通过 |
| E2 Reference / Mock | not applicable | deterministic services 仅保留身份，尚未实现 reference 行为 |
| E3 Integration / Golden | not applicable | runner、codec、golden 和 aggregator 属于后续任务 |
| E4 Physical / Demo | not applicable | 工作区骨架不需要真实设备 |

关键文件 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `verification/workspace.json` | `1b1452b566c0f0a2f648fcf78fcca15558f0a70437ae0be3b7ac61ac3103deb6` |
| `verification/schema/workspace-manifest.schema.json` | `36db2b6891ac9affecc03a3dde86f74847fadb4677f7fb67f7784fb993290bfc` |
| `verification/tools/validate_workspace.py` | `191e89ed61355fd76b97b685aaef9f8e7b1e79e49bf74a188061a712be5810` |
| `verification/tests/test_workspace.py` | `ac6d685297ad1925de32ac9427ba43b404b65eeb0d7bdf70c0fe283f82692263` |
| `verification/package.json` | `8ab00da53aebda58f9e6ca453ad547ab81cd719e50f582e545e8632e677aa670` |
| `verification/package-lock.json` | `9fb7967aa22da0d87d51b4a2e34840e2f0f74fe9f338924b47c2cb397fe58cf1` |
| `verification/tools/validate_schemas.mjs` | `80f69e96b73438d00e1f3fc51793d315d7b04451463c0113be49222134f4ae3a` |
| `verification/tests/schema_meta.test.mjs` | `5b80c5bed474cf53b09fdcdb29592ae068537552ecbd539c09ed1138de2b81d7` |

这些是执行验证时的工作区字节摘要。提交前若修改了文件，必须重新运行验证并更新摘要，不能
沿用本记录中的旧值。最终提交身份在提交完成后由 Git 历史提供。

## 6. 状态结论

本节保留 2026-08-23 首轮验收时的历史结论：当时按照仓库内已抽取的范围，曾将
`GT-G0-01` 记录为 `Pass`。开始 `GT-G0-02` 前重读最新 IH-01 正文后确认，本记录遗漏了
TypeScript workspace、完整 Platform/Protocol schema 集、每 schema 合法 fixture、统一 validate
命令和 build/typecheck，因此该 Pass 曾被后续审计推翻。随后已按 IH-01 补齐并重新运行全部检查；
现行状态恢复为 `Pass`。

失败历史不删除、不改写成从未发生。G0 和 R1 Verification Foundation 继续为 `Validating`，
R1 不 Accepted；`GT-G0-02` 的上游阻塞已关闭。

本轮未修改产品 Runtime、POC、platform shell、CMake 或 workflow，也未进入 `GT-G0-02` 的实现。
