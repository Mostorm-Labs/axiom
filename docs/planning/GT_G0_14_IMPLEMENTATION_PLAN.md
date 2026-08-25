# GT-G0-14 实施计划

状态：`Pass`（实现提交 `7d9c22df422e8e983d49cf3011865804aa09189c`；hosted 验证见 Evidence）
任务范围：只完成 PR CI dependency graph，不进入 `GT-G0-15`。

## 工作包

1. **Comparator contract**
   - 先增加 required/forbidden/partial-order/capability/OPEN 语义及确定性 first-divergence 的失败测试。
   - 在 runner 包实现共享比较器；CLI 增加 `compare`，并删除 adapter `run` 伪造结果的行为。
2. **PR decision contract**
   - 先增加 schema/protocol/semantic/platform 四类故障归层和 retry 保留测试。
   - 实现 provider-neutral layer record 与 PR decision 聚合器；它不生成 G0 Gate Report。
3. **Change classifier 与 run-set**
   - 先验证 schema、protocol、common、single-adapter 和 unknown 路径规则。
   - 实现确定性分类器和版本化 run-set manifest。
4. **Semantic bootstrap trusted root**
   - 增加 bootstrap manifest/schema 和生成工具。
   - 只绑定 POC-01 host-core operation/replay/digest/projection 测试，明确不是 G1 semantic suite。
5. **PR CI wiring**
   - 建立 `schema → protocol + semantic → selected platform → aggregate` workflow。
   - 添加静态 DAG 检查和四类 deliberate-failure meta tests。
6. **Evidence 与追踪**
   - 生成 commit-bound Evidence，记录设计、实现、验证和限制。
   - 保存 hosted 正常 DAG 与四类故意失败归层产物，更新 Gate Task Tracker 与 R1 覆盖状态。

## 验证命令

```bash
cd verification
npm run build
npm run typecheck
npm run validate
npm run test
npm exec -- axiom-platform-conformance compare --suite platform-seed-v0.1 --observations <path> --output <path>
npm exec -- axiom-platform-conformance aggregate --run-set <path> --records <path> --output <path>
cd ..
python3 verification/tools/validate_workspace.py
python3 tools/check_docs.py
git diff --check
```

GitHub PR 绿色 DAG bundle 以及 schema、protocol、semantic、platform 四类故意失败归层 Evidence
已保存；本地成功未被用于替代 hosted CI。
