# Axiom Verification Workspace

这是 G0 验证基础的受控工作区。它是验证工具、语料和 Evidence 的仓库边界，不是
Axiom 产品 Runtime，也不是产品公共 ABI。

## 当前任务边界

`GT-G0-01` 只建立目录 ownership、workspace manifest、manifest schema 和静态校验器。
下列内容由后续任务负责，本轮不提前实现：

- `GT-G0-02`：protocol package、envelope codec 和 wire schema；
- `GT-G0-03..05`：runner、transport、fault/fence 行为；
- `GT-G0-06`：56 个 protocol vectors；
- `GT-G0-07`：共享 CLI 和 CI gate；
- `GT-G0-08..13`：native/platform adapters 与平台场景；
- `GT-G0-14..16`：CI wiring、Gate Report schema 和 aggregator。

## 骨架约定

- `workspace.json` 是唯一的工作区入口，使用稳定 JSON 顺序和 UTF-8 编码；不写入绝对路径、
  runner 临时状态、机器时间或构建产物 hash。
- `schema/workspace-manifest.schema.json` 只约束工作区 manifest；它不冻结产品 IDL、
  Operation wire format 或 Gate Report schema。
- `tools/validate_workspace.py` 只做结构、路径 ownership 和确定性检查，不执行产品测试，
  也不自动生成 golden expected。
- 每类语料目录先由 `README.md` 声明 owner 和 expected policy，再由后续任务填充版本化语料。

运行检查：

```text
python3 verification/tools/validate_workspace.py
python3 -m unittest discover -s verification/tests -p 'test_*.py' -v
```
