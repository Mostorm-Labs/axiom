# FC-03 Crop 交互合同（P11/P17 冻结候选）

状态：**FROZEN_FOR_P20/P31_CANDIDATE**。这是 FC03 产品/平台合同，不新增 ImageContent schema 字段。

## 进入、更新和提交

1. Select 工具选中 Image 后选择 Crop，进入 `ImageCropSession`；会话保存完整进入快照、PointerKey、surface/view generation 和 resource binding。
2. Crop 框是自由比例矩形；不支持旋转、focal point 或固定比例模式。
3. 拖动底图只改变临时 content offset/scale；拖动裁剪框只改变临时可见范围；顶部 slider 改变临时 content scale。
4. source rect 被限制在 `[0,1]×[0,1]`，`x + width <= 1`、`y + height <= 1`，width/height 必须大于 `1/4096`。显示框 width/height 必须大于 `1 DU`。
5. slider 的有效范围为 `0.25..4.0`，默认值为 `1.0`；超出范围在 session 层拒绝并保留上一份 preview。
6. 内容拖动和缩放始终 clamp 到裁剪框仍覆盖 source plane 的有效区域，不生成空 source。
7. `Done` 生成一个完整 `SetImageContent`：保留 resource/intrinsic/contentMode，写入 normalized `source_rect` 和最终 display width/height；成功后只产生一个 History entry。
8. `Esc`/Cancel、输入源注销、Surface loss、关闭请求或 binding generation 变化均丢弃 session，零 Operation、零 History、零 SemanticGeneration 增量。

## 普通缩放

普通态四角缩放默认保持当前有效 source aspect ratio；按修饰键才允许自由拉伸。普通缩放提交一个 `SetObjectSize`，不改 resource、intrinsic、source rect、contentMode 或 Transform。

## 导入与失败

Windows file picker 与 Web file input 接受 PNG/JPEG/WebP；host 提供不可变 bytes 和 MIME/file facts，Runtime 在 owner safe point 完成 manifest/hash/length/decode 校验。MIME 与实际 codec 不匹配、读取失败、hash/length 不匹配或 decode 失败时，不创建 Image object、不写 History，并返回稳定失败分类。

已提交 Image 的资源随后 Missing/Ready/Failed 只改变 derived placeholder/render state；不得修改 canonical ImageContent。

## 平台一致性

Windows/Web 共享上述 source rect、mode、clamp、Done/Cancel 和 History 语义；平台差异只限文件采集、Surface/GPU provider 和呈现反馈。Host 不直接写 SemanticDocument 或 Runtime 容器。
