# GT-G1-04-A Re-entry Reconciliation Evidence

状态：`PASS_REENTRY_READY_A0_A3_IMPLEMENTATION_NOT_STARTED`

本 Evidence 绑定 source commit `5ca4b6a227df7a037f4ff4f9338dc2e2f002b620`。该 source 在已合并的 Current Authority `main@17ff844f3b72e1976dee248550aa80f59ad38990` 上完成 G1-04-A A0–A3 re-entry：保留此前 `BLOCKED_AUTHORITY` 审计为历史 lineage，发布当前 reconciliation / contract matrix，并建立只覆盖 A0–A3 的 RED→GREEN 实施计划。

本 source 相对 authority baseline 只包含文档、历史归档和实施计划；没有修改 runtime、Proto、schema machine authority 或 verification corpus。A0–A3 当前状态是 `READY_FOR_A0_A3_IMPLEMENTATION`，不是实现或 Gate Pass。

明确未授权：`GT-G1-04-B`、`GT-G1-04-C`、`GT-G1-05`。若 A2 无法从现有 WirePreflight / DTO boundary 保留 `schema_version` / `payload_version` field occurrence，实施必须停止为 `WIRE_SCHEMA_CHANGE_REQUIRED`，不得默认版本或自行修改 Proto。

详细 provenance、scope 与 review 记录见 `reentry-reconciliation.json`。
