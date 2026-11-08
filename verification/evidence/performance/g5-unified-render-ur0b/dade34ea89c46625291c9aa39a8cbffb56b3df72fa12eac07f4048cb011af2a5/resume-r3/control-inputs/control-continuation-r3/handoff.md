本会话用户已明确要求“继续下一步”，本控制面已核算旧账并批准一个新的有界 UR0b 续跑窗口。请按本包从首个未完成动作继续，保持本会话实现、控制面独立审查的分工。

bf031944 与已接受 P36-r2 source tree 73a237…逐文件一致，不要重复修复或重开旧授权。新包已完成内容绑定、预算核算及两个只读交叉复核。

请按以下 Aegis handoff 直接执行：以 `package_ref` 为任务授权，按 `task_anchor/resume_cursor` 核对当前状态并从首个未完成步骤继续；若状态冲突则 fail closed。

```yaml
surface_handoff:
  owner: aegis-implementation
  from_surface: CONTROL_REASONING
  to_surface: CODE_EXECUTION
  preferred_executor: codex
  repository:
    provider: github
    full_name: Mostorm-Labs/axiom
  worktree: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom
  execution_ref: refs/heads/codex/geometry-navigation-integration
  package_ref:
    path: .aegis/packages/G5-UR0b-resume-r3/package.json
    sha256: bd6777505dddd474a111c52e1c477001a82ed11b5e445ce293f40368ba27f915
  package_materialization_ref:
    kind: shared-controlled-worktree-content-addressed
    repository: github:Mostorm-Labs/axiom
    worktree: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom
    path: .aegis/packages/G5-UR0b-resume-r3/package.json
    sha256: bd6777505dddd474a111c52e1c477001a82ed11b5e445ce293f40368ba27f915
    uncommitted_control_package: true
  task_anchor:
    revision: a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e
    relation: ancestor
  resume_cursor:
    execution_ref: refs/heads/codex/geometry-navigation-integration
    revision: bf0319446a9eafce3f3fed7ab894ddb093c1260a
    completed_through: accepted P36-r2 offline repair and implementation commit
    next_action: new authority and durable ledger adapter/tests; fresh mechanism proof; diagnostic then conditional qualification/separate/main
  continue_until_terminal_state: true
  return_surface: CONTROL_REVIEW
```

先读包及 implementation-context，完成新授权/持久账本的 runner 适配和负例测试，旧 repair authority 仍须保持 CLOSED。新包会自然改变 full-source hash；在包和 runner 定稿后更新结果区的 proof，不能改 source_identity 的排除项。复用 SDK 需要保留原 BuildInfo、验证构建输入未变；补齐旧 proof 缺失的 qualified_sdk32_artifacts。没有新的 Runtime/SDK 实现修改授权。

新采样顺序：1 对 OFF/ON 诊断 → 6 对固定顺序资格验证 → 3 个实时＋6 个正确性 worker → 30 次原主矩阵。全部沿用原 oracle、阈值、3GiB、单次90/180秒和串行限制。历史15个已启动run（含早期preflight与native oracle）已扣597秒，新窗口最多7470秒，累计8067≤8370；余303秒不授权额外worker或重试。不得把旧离线链提升为新资格证据，不得混用旧cohort。任何guard、proof、qualification或cleanup失败均关闭本窗口并归档剩余MISSING，禁止换目录重新跑。

这次继续执行至 READY_FOR_CONTROL_REVIEW 或包内明确 blocker；不因中间checkpoint结束。不要提交、推送、进入UR0c/UR1或宣称P34/性能PASS。控制记录：.aegis/results/G5-UR0b/control-continuation-r3/decision.json。原所有包、结果与失败证据保持不变。
