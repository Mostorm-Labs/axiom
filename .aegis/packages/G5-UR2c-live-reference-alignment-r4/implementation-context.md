本轮是UR2c test-only live reference对齐，不是新的生产优化，也不改AA容差。锚点为已发布edcbce18；Runtime测试主体仍是R2 d9bd0a26及原72SDK，R3 GLreader和同步失败capture已通过真实S1八项控制，均不改。

控制已独立复核R3停机：整块109200像素ROI用未改动old comparePixels比较，无AA/HUD排除，0 mismatched，最大delta2。唯一像素[13,27,51,255]→[13,29,53,255]违反了“对固定palette13,26,51的±2”，但不违反“对实际固定baseline的≤2”。这属于live probe参考对象错误，不足以宣称产品不满足已接受pairwise契约。并未证明其物理成因一定是AA。全部102个已保存live的原opaque选择也符合baseline pairwise2。另6个DPR4全ROI较大差异已用原独立CPU before-AA边界裁切验证，内部差异0，没有按candidate差异扩边。

只改qualifySentinel的after比较参考为固定before buffer，保持原baseline-only nearink selection、SENTINEL、min100、numeric2。不得用current输出选mask，不得滚动刷新baseline导致累积漂移漏检，不得把原生Retry像素byteeq0改成2。新unit须证明真实R3反例修正、channel3/alpha3/clear/missing/无基线仍拒绝及0->1->2->3固定参考漂移。先旧RED再改行为；历史R3状态不改写。

新增一个normal worker UR2CLP4-N1最多600s，原90starts/6251s保持，8370cap余2119s；不用重跑S1，不重编SDK，不额外smoke。沿用原R2八正例计划，运行原10+全部6continuous，尤其补完S2/DPR1cancel/terminal/after/UndoRedo和DPR2/4。GLtarget/即时fullfailurecapture/原cold独立oracle/receipt规则不变，新的相对规则不放宽2。不重新要求缺失的历史baselineGL数字作Gate，若方便可记录新的baseline元数据。

所有新输出在G5-UR2c-live-reference-alignment-r4；原Runtime/SDK/reader/failurecapture/oracle/plan/既有raw和4297均保持。若新固定参考2仍失败，携完整真实target数据停止回报，不改产品或加容差。设计解清即连续执行到READY或明确终态，按人类持续授权主动通知控制线程01a1b6f3-e69a-7fe2-8d9b-0222e8741f03 hostlocal，带exactSHA、测试、账本、清理及余项。不commit/push，不进UR2d，不自签官方P34/性能PASS。
