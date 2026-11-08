用户本轮明确授权：“继续下一步，准备好后通知隔壁开始执行，同样让它执行完后再通知你这边”。控制面已准备并批准剩余38项 UR0b 续跑包，176处引用/预算/旧数据复用校验通过。请直接按以下新包完整执行，不要重做已接受的离线修复。

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
    path: .aegis/packages/G5-UR0b-resume-r4/package.json
    sha256: 2cadd6e12c73dc4006fd20691feca9402974a9f31fe705b2200c4654e020b569
  package_materialization_ref:
    kind: shared-controlled-worktree-content-addressed
    repository: github:Mostorm-Labs/axiom
    path: .aegis/packages/G5-UR0b-resume-r4/package.json
    sha256: 2cadd6e12c73dc4006fd20691feca9402974a9f31fe705b2200c4654e020b569
    worktree: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom
    uncommitted_control_package: true
  task_anchor:
    revision: a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e
    relation: ancestor
  resume_cursor:
    execution_ref: refs/heads/codex/geometry-navigation-integration
    revision: f651523a40c9b383b7d8e2115fcd32f75ddc38a7
    accepted_dirty_source_sha256: 2ca47a17ae94034a574379e32ae118b95b5f7c35fee9706be91dc21c46477ebb
    completed_through: accepted init repair and recorded diagnostic/cohort reuse; old r3 closed
    next_action: new r4 runner/reuse/ledger tests and current source/artifact proof, then8separate and conditional30main
  continue_until_terminal_state: true
  return_surface: CONTROL_REVIEW
```

先逐步读取 package.json、implementation-context.md、execution-contract.json。新包增加9个控制文件，因此当前 full-source 在实施前已自然变为 60fd2073f0c5951048199857d2f1bc1a0a482ccfd7dca354f7f3a78111a592d3；这是相对已接受2ca47a…仅新增本包文件的差异，不是产品代码漂移。保留f651523a上的已接受dirty与未跟踪初始化代码。不要依赖历史HEAD相等，若是同源有效descendant按cursor核对。

执行范围：只新增 verification/runners/g5/ur0/remaining_runner.py 和 verification/tests/test_g5_ur0_remaining_runner.py，复用已有atomic/charge/execute/check_worker/phase等机制；原Window硬编码r3身份与限额不能直接拿来跑新窗口。新固定独占持久账本位于.aegis/results/G5-UR0b/resume-r4/。旧r1/r3入口和窗口不改、不重开；原compiler/collector/Runtime/SDK/harness/metrics schema/阈值保持已接受字节。

reuse-inputs.json 已把原15份recording、其新v2派生proof与独立接受记录全部精确绑定：明确走接受旧证据的复用途径，不把OFFLINE/旧source改名为fresh，不弱化require_bound_compilation。新实际worker仍走真正WORKER_COMPILE+当前source/SDK/compiler/spec检查。诊断、六对AB/BA overhead和60Hz已可复用，不授权重跑它们。若复用绑定不能满足，直接按blocker返回，不用新cohort兜底。

runner和测试完成后生成当前机制证明，重新哈希两档现有SDK和原native/build/export/install证据并复用，不重建产物。然后串行执行run-plan.json中的120/240Hz两项＋六类correctness；全部原条件成立后直接继续30行原主矩阵。本次38项新worker，单次仍90/180秒、3GiB，Render32/96及64/192、诊断16MiB含1MiB reserve不变。历史30次累计1133s；38项名义限额6120s，计划合计7253<=原8370。余时不授权重试或额外browser probe。全部守护/实际账本如实记账，第一处guard/proof/identity/cleanup/资格失败即关闭新窗口并保留剩余MISSING。

本次包已冻结全部required tests/oracles、terminal blockers、结果边界与通知要求。请持续做到READY_FOR_CONTROL_REVIEW或明确blocker，不在实现检查点结束。仅在状态冲突/包内真实blocker时停止；不要增加新的产品性能门槛，不提交、不推送、不进入UR0c/UR1、不自签P34/性能PASS。控制批准在 .aegis/results/G5-UR0b/control-continuation-r4/activation.json。

完成或发生明确阻塞后，必须主动通知本会话：send_message_to_thread(threadId=01a1b6f3-e69a-7fe2-8d9b-0222e8741f03,hostId=local)。用户已明确授权此终态回传。附package/status、revision/dirty/current source SHA、result/bundle路径与SHA、测试和复用/新录制区分、38项账本/累计耗时、guard/cleanup、未完成项与blocker。若工具不可用，保存终态结果并明确“通知未送达”，不能只结束本会话等待用户转述。