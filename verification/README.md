# Axiom Verification Workspace

这是 G0 验证基础的受控工作区。它是验证工具、语料和 Evidence 的仓库边界，不是
Axiom 产品 Runtime，也不是产品公共 ABI。

## 当前实现边界

`GT-G0-01..07` 已依次建立 TypeScript workspace、目录 ownership、workspace manifest、13 份
Platform/Protocol schema、协议包、Reference Runner、scripted adapter/transport、56 条 protocol
vectors，以及共享 CLI 与 protocol CI gate。任务状态和 commit-bound Evidence 以
`docs/planning/GATE_TASK_TRACKER.md` 为准。

下列内容仍由后续任务负责，不由共享 CLI 的存在提前声明完成：

- `GT-G0-08..13`：native/platform adapters 与平台场景；
- `GT-G0-14..16`：CI wiring、Gate Report schema 和 aggregator。

## 骨架约定

- `workspace.json` 是唯一的工作区入口，使用稳定 JSON 顺序和 UTF-8 编码；不写入绝对路径、
  runner 临时状态、机器时间或构建产物 hash。
- `schema/workspace-manifest.schema.json` 只约束工作区 manifest；它不冻结产品 IDL、
  Operation wire format 或 Gate Report schema。
- `tools/validate_workspace.py` 只做结构、路径 ownership 和确定性检查，不执行产品测试，
  也不自动生成 golden expected。
- `npm run validate` 统一加载 13 份 Draft 2020-12 schema、合法 fixture 和负向 meta-tests；
  `npm run build`、`npm run typecheck` 验证 TypeScript workspace。
- 每类语料目录先由 `README.md` 声明 owner 和 expected policy，再由后续任务填充版本化语料。

运行检查：

```text
python3 verification/tools/validate_workspace.py
python3 -m unittest discover -s verification/tests -p 'test_*.py' -v
cd verification
npm run validate
npm run build
npm run typecheck
```
