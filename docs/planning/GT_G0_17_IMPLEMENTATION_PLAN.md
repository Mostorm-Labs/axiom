# GT-G0-17 实施计划

## 工作包

1. 读取 G0 总路线、验证策略、GT-G0-00～16 Evidence 和 Gate Report；
2. 对照 G0 退出条件建立逐项 review matrix；
3. 复算 GT-G0-16 Gate Report 与 manifest hash；
4. 独立判定 G0 `PASS`/`FAIL`/`BLOCKED`，记录 first blocking issue；
5. 回写 Gate Task Tracker、R1 状态和 review Evidence。

## 历史首次审核结果

- GT-G0-00～GT-G0-16 lineage：全部 `Pass`；
- GT-G0-16 Gate Report：schema-valid，E1/E2/E3 `PASS`，E4 `BLOCKED`；
- manifest/artifact hash：可复算；
- G0 退出条件：C++ runner Evidence、全量十次确定性 Evidence、完整 clean-checkout Gate Report Evidence 均未关闭；
- G0 review：首次审核为 `BLOCKED`，不得晋级 G1；该结论由后续 package-bound revalidation 重新评估。

## Package-bound revalidation 结果

- 五个平台均已提供可核对的 package-bound physical authority；
- G0-16 在合并后的 `main` 上重新聚合，E1～E4 全部 `PASS`；
- GT-G0-17 revalidation：`Pass`；G0：`Pass`；可以进入 G1。

## Physical authority 规则

GT-G0-17 不要求每个平台都重新构建当前 `main`。Physical authority 绑定实际安装包，而不是强制绑定当前源码提交。安装包必须记录 package identity、构建基线（若可得）、fixture/corpus digest、artifact hash、设备环境和原始结果位置；当前 `main` 的新增代码由 commit-bound CI、Gate Report 和 E1～E3 覆盖。这样移动端、Windows 和 Web 使用同一套证据规则，不因平台工具链差异产生重复真机测试。

## 重新进入条件

必须补齐并重新生成 Gate Evidence：

1. 完整 C++ runner 对 seed/invalid corpus 的运行记录；
2. 同一 G0 语料连续 10 次的结果、operation sequence/digest 和比较摘要；
3. clean checkout 中从固定 commit 重建 Gate Report 的日志与 artifact hash；
4. Web/Windows/Android/iPhone/iPadOS 的 package-bound physical release authority；不要求安装包重新绑定当前 `main`，但必须能核对包身份、fixture/corpus digest 和 artifact hash。

补齐后重新运行 GT-G0-16 aggregator，再重新执行 GT-G0-17 review；不得编辑本报告把 `BLOCKED`
改成 `PASS`。
