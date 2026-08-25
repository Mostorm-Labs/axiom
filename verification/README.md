# Axiom Verification Workspace

这是 G0 验证基础的受控工作区。它是验证工具、语料和 Evidence 的仓库边界，不是
Axiom 产品 Runtime，也不是产品公共 ABI。

## 当前实现边界

`GT-G0-01..07` 已依次建立 TypeScript workspace、目录 ownership、workspace manifest、13 份
Platform/Protocol schema、协议包、Reference Runner、scripted adapter/transport、56 条 protocol
vectors，以及共享 CLI 与 protocol CI gate。任务状态和 commit-bound Evidence 以
`docs/planning/GATE_TASK_TRACKER.md` 为准。

`GT-G0-10` 已增加 Web reference adapter：它只负责 browser/WASM host facts 和 normalized
observation，不读取 expected 或判定 PASS/FAIL；Web Arc-only 场景按共享 contract 输出 N/A。

`GT-G0-11` 已增加 Windows native reference adapter：它只负责 Win32/D3D12 profile、host/surface/device
generation、DPI/metrics、pointer history normalization、Arc ownership 和 stale-scope facts，不读取
expected 或判定 PASS/FAIL。macOS host-side 只验证可移植 logical contract；真实 Win32/D3D12 编译、28 个
Windows 场景和物理 Evidence 已在 Windows runner 上完成，`GT-G0-11` 已记录为 `Pass`。

下列内容仍由后续任务负责，不由共享 CLI 的存在提前声明完成：

- `GT-G0-12` 已增加 Android Activity/View/JNI instrumentation adapter：它只负责 Android surface/device
  generation、MotionEvent history 到 `PointerSampleBatch` 的事实归一化和 Arc ownership observation；
  Shared Runner 仍负责 expected/comparator。Hosted emulator 与 physical Android device 的 Evidence 必须
  分开记录，emulator 不得冒充 physical PASS。
- `GT-G0-13` 已增加独立的 iPhone/iPadOS RN/Fabric + ObjC++/Metal facts-only adapter；两端 profile、
  observation 和 Evidence 必须分开，macOS 只保留 shared-core logical conformance。
- `GT-G0-14`：PR CI DAG、Shared Platform Comparator、semantic bootstrap、run-set 与 PR decision；
- `GT-G0-15`：Nightly/Release wiring、Full Run-set、Platform Evidence Index、Platform Release Decision 与 reproducibility contract；当前仍等待 commit-bound hosted validation。
- `GT-G0-16`：Gate Report schema 和 G0 aggregator，尚未实现。

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
