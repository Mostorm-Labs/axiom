# GT-G1-04-A Authority Publication Evidence

状态：`AUTHORITY_PUBLISHED_IMPLEMENTATION_NOT_STARTED`

本 Evidence 绑定提交 `8adcf8599775dcdf8852699f5f8725a7add1864e`，记录精确批准
`APPROVE_G1_04_SEMANTIC_FREEZE_V1` 后的 authority 发布结果。四份 Current Authority 已发布到
Notion `04 Semantic Schema / Operation Model` 的 `00 Overview`、`01 Object Schema`、
`02 Operation Model` 和 `05 Leaf Schemas` 父页，并在仓库形成对应镜像。

本轮还生成了三份只表达 A0–A3 无状态语义的 YAML projection：ObjectKind version registry、
Operation structural profile、semantic leaf constraints。它们不包含 B 的 store/reference/ApplyPlan
状态规则，也不包含 C 的 reviewed outcome、golden 或 differential oracle。

验证结果：

- `python3 tools/check_docs.py`：251 个 Markdown 通过；
- `git diff --check`：通过；
- Ruby Psych：manifest 与三份 projection YAML 语法通过；
- Notion 四页 round-trip fetch：Current Authority 标记存在，旧的 `PROPOSED UNTIL HUMAN APPROVAL`
  标记已移除；
- Proto、runtime、verification corpus、CI：未修改。

详细机器记录、页面 ID 与 SHA-256 见同目录 `authority-publication.json`。

这不是 `GT-G1-04` overall Pass，也不表示 A0–A3 已实现；下一步是在合并本 authority branch 后，
回到 G1-04 implementation branch 做 A0–A3 re-entry 对账。GT-G1-04-B/C 与 GT-G1-05 仍未授权。
