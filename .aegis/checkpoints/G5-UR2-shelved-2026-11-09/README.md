# UR2 暂存检查点（2026-11-09）

**已隔离、未验收、不参与当前构建。** 这笔提交只保存暂停前的工作，不启用UR2，也不把它认定为已接通实现。当前开发主线继续UR1，UR1c1的活动源码不属于本存档。

本目录的 `.gitattributes` 使用 `* -text` 禁止 Git 换行规范化，确保归档源码、补丁与 manifest 的逐字节哈希保持一致；不影响活动源码目录。

- `source/frozen-candidate/`：原UR2候选的9份完整源码与测试，哈希绑定原BLOCKED结果。
- `source/preimages/`：4份修改前的精确UR1源码/构建文件。
- `source/repair-wip/`：后续4份工具修复WIP；包含cold保全helper的RED/green，但真实worker接线尚未全量验证。
- `patches/`：候选补丁及后续WIP补丁，供隔离恢复和差异审查。
- `historical-packages/`：当时冻结包原件，仅作历史来源，**不构成当前执行授权**。
- `evidence/`：原阻断结论、已完成检查、采样账本、停止checkpoint和独立控制审查。原结论不改写为PASS。
- `checkpoint.json` / `manifest.json`：源码恢复映射及本目录全部负载的SHA256。
- `external-artifacts.json`：未纳入Git的大型SDK、native、像素和snapshot证据清单，明确标记LOCAL_ONLY_NOT_PUBLISHED；不把本机out路径当作远端artifact。

## 恢复与核对

运行 `python3 verify_checkpoint.py` 核对本目录所有负载。`recovery-verification.json` 记录临时目录中两份补丁的精确重放结果：候选9/9、WIP 4/4文件与归档哈希一致。源码、测试、补丁和状态说明可以仅从本目录恢复；无需读取原先本机的`.aegis/results`或`out`目录。

需要查看候选实现时，把`source/frozen-candidate/`导出到一个新的空目录；需要后续工具WIP时，再叠加`source/repair-wip/`。它们是改变过的源码树，不是可独立构建的完整仓库。原候选基线为`14ce6221`加已接受UR1a内容，4份精确preimage已随包保存；不能在未知版本上盲目应用补丁。

恢复进开发分支之前必须重新核对当前UR1状态并制定整合范围，不自动覆盖活动源码，不执行这里保存的旧handoff。精确重现旧二进制和原始像素证据还需要独立取得大型artifact；本提交不声称这些数据已远端发布。
