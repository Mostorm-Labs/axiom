# FC-02 文本格式增量合同

本轮由用户授权同时完成合同与实现。基线 c3256b0c；保留历史 Authority、Golden、SDK locks 和人工失败记录。

## 交互和编辑边界

- Ink 不通过双击进入文本编辑。Text/Select 模式的双击由 Runtime 再次准入。
- 每个固定宽文本框显示黑色一像素边框；边框属于派生显示，不创建 Shape 或额外历史。
- 格式通过 RuntimeFacade typed TextIntent 进入 Interaction 会话，再走 Semantic/History。
- 有选区时只修改相交字符的指定样式叶子；无选区时修改后续输入样式。段落格式作用于选区相交段落，空选区作用于当前段落。
- 对齐支持 Left/Center/Right；项目符号是段落属性，展示标记不属于文本投影，不影响 UTF-16/scalar 偏移。Ctrl+Enter 分段继承列表，Enter 仍结束编辑。
- Title=32、Heading=24、Subheading=20、Body=16；Runtime 接受任意合法颜色和字号，Debug UI 仅黑/蓝。
- 格式控件保留会话和选区；合成期间拒绝格式修改，等待真正的 native completion。

## 版本、持久化和兼容

- TextStyle tag7 `strikethrough` 和 ParagraphStyle tag5 `bulleted` 为可选 bool；缺省不编码，显式 false 非规范。
- 仅 RichText/3 可携带 true 新字段，必须保留固定宽度；RichText/1、/2 编码及语义不变。
- 首次启用删除线/列表以 guarded delta3 的完整状态替换原子升级到 /3；普通文本和宽度编辑不得降级。
- delta1/2 不接受新字段或 /3 前像/目标；delta3 使用现有 32 字节对象绑定 guard，支持普通步骤与独占 RestoreState/SetLayout。
- 精确 Undo 恢复原 kind_version、run 边界、段落身份、样式及宽度；失败无部分写入。
- Snapshot3 增加 `richtext-formatting-v3` capability，独立 reader bit4，严格排序且按实际内容精确匹配；旧 reader 拒绝。
- 版本3字体引用和版本2一样必须绑定 manifest Font；未知字体拒绝，布局缺字明确诊断，不用系统字体偷偷替代。

## 字体和验证

字体选择来自明确加载的字体资源目录。不存在能保证所有 Unicode/emoji 的单个字体文件；加载文件必须携带完整字节身份，并沿既有资源注册与 TextLayout 路径使用。字体覆盖不足必须可读回。

验证覆盖 Ink 双击、边框像素、部分/跨段格式叶子保真、typing style、非法值/IME busy、版本准入、Operation codec、Snapshot/replay/精确 Undo、列表几何、真实 Debug UI 控件和严格编译。人工文字/IME观察单独记录，不推定 PHYS PASS。

## 混合选区效果读回与导入字体

非空选区的 Bold / Italic / Underline / Strike 采用 OR 读回：任意相交字符启用即为启用。Bulleted list 对相交段落采用相同规则。选区为 end-exclusive，不计入边界外的 run 或下一段。关闭效果将选区内该叶子统一设为 false；无选区仍读取当前输入样式。颜色、字号、字体和对齐保持选区起点读回。

用户提供的 zip 已导入 11 个家族，固定字节、摘要、独立资源 ID 和随附许可证见 `assets/fonts/imported/catalog.json`。Windows 打包在 exe 相邻的 fonts/；Web 预加载同一目录。Runtime 从已准入字体字节读取家族名称，Debug UI 只通过 Facade 选择。

导入各家族常规或可变字体：Comic Neue、Fira Sans、IBM Plex Mono、IBM Plex Sans、Inter、Inter Tight、Open Sans、Oswald、Roboto、STIX Two Text、Ubuntu。保留既有 Noto Sans CJK SC 默认身份；这些字体不保证中文或 emoji 覆盖。没有准确粗斜体 face 时，固定宽文本沿现有派生样式路径绘制合成粗斜体；不读取系统字体。
