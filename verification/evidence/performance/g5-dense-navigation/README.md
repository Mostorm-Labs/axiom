# 密集混合场景导航修复

实现：`32418c783170eea56a4131579fac627c003c91bb`，分支 `codex/geometry-navigation-integration`。
P31 范围见 `docs/planning/G5_DENSE_NAVIGATION_FRONTIER.md`。结果为此项工程修复的
`READY_FOR_CONTROL_REVIEW`；没有签发完整 G5、P34、跨平台或产品性能 Gate。

## 查明并修改的路径

SDK 输入 → Runtime camera/Scene 查询 → SkiaNavigationTiles → provider present。

- 原先按当前位置恰好覆盖几格来选 coarse level，纯平移跨网格也会切换层级，打断关键覆盖准备。
  改为按视口尺寸选择任何原点均至多2×2格的粗层；独立 RED 回归已复现旧逻辑的不稳定查询跨度。
- 小块新露出区域原先也等待整个粗瓦片的全部笔画。现在每张图像携带最多8个明确的有效区域，
  只先准备缺口，并在暴露的视口边缘保留四分之一瓦片的准备余量。只发布已完成的区域。
- 原有清晰重叠区继续按最高有效密度组合。增量图像先复制旧有效像素，再 Src clear／重画新条带及采样 gutter。
  复杂度溢出、资源拒绝和未知区域保持保守处理；没有用未画完的像素冒充覆盖。
- 完整路径按设备需求筛选，栅格化仍用原始 Skia 网格／clip。直接裁窄 raster clip 会改变 analytic-AA，
  开发中曾观察到该错误并撤回。Brush 几何、材质、文档 mutation 和 canonical receipt 契约不变。
- 保持原有 work/time/GPU/cache 限制。新增固定 region／Job metadata 计入预算。

## 对照结果

四类场景（选中／未选中 × 平移／缩放平移），各自 ON/OFF、修复前后配对。
636×923 CSS、DPR2、1272×1846物理像素、Zoom1.59；fixture 读回11个 Runtime 对象，
包含5条各2048输入样本的不同交叠轨迹、2个 Shape、Image、Table、Sticky 及 SDK 生成的关联内容。
这是一份固定压力／轨迹的压力样本，不是用户4218原文档的副本。

先确认 SDK RAF 完全停止，再开始输入。选中缩放场景另含60秒前台闲置，所有场景包含冻结／恢复。
指标为输入期间**最长默认 framebuffer 提交空窗**（ms），不是物理 input-to-photon，也不是 GPU 执行时间。
下表来自默认 OFF 构建：

| 场景 | 修复前 | 修复后 |
| --- | ---: | ---: |
| 首次平移 | 237.5 | 43.7 |
| 选中后重复平移 | 268.5 | 19.1 |
| 选中后闲置平移 | 268.1 | 19.0 |
| 缩放／平移 | 218.8 | 101.5 |
| 选中＋冻结恢复后的缩放／平移 | 623.4 | 368.8 |

主验收为两种平移场景各 repeat-0／repeat-1 的4个 gap 最大值之中位数：
ON `269.7 → 20.6 ms`，
OFF `268.7 → 22.8 ms`。
两者均满足任务约定至少减半；这不构成产品 SLO。

所有配对 phase 的最终**完整画布** SHA256、初始画布像素、文档／选择／Operation／相机检查一致。
ON 有 Runtime 诊断，OFF 无 timing strings；两者使用同一个 root SDK fixture。
具体尾部、receipt wait、CPU、texel-density quality 与 raw traces 均保留在 comparison.json、on/、off/。
密度统计只由 ON 提供；其低清占比是 texel-density 指标，不是光学清晰度。暖缩放部分阶段的占比约增加4.6个百分点，
更早提交的粗级新区域和仍受限的细节容量均须继续跟踪，不能只报告平移中位数。

## 正确性与边界

- 旧85f01aba + 本轮最终回归：dense frontier 和 coarse camera stability 均以预期错误 RED。
  final-red.json 绑定测试 SHA256 和被替换的旧源码 SHA256；测试后恢复实现，再完整重建。
- 原生相关83个程序构建成功；112项测试通过107项，5项均为原合并基线失败，无新增失败。
  失败名称和完整日志在 checks/，没有修改 semantic fixture 或旧 Gate。
- 7组真实 Chrome WebGL2 oracle 全部通过：frontier、quality、affine、background、contour、replay、exact-replay。
  包括 partial/full coarse 像素、清晰 overlap、扩展区域、canonical Direct 像素与原有 GPU quotas。
- Node `25` 项通过；Runtime boundary `118` 份契约通过。
- ON/OFF 各36个正常安装文件，无私有头文件或依赖 archive；OFF 无诊断字符串。
- Demo UI smoke 已通过：正常 example.html 用 DOM 工具创建笔画和矩形并平移，另运行预置混合场景。
  smoke 不是受控性能对照，其冷启动 receipt wait 约782ms也原样保留，不能把它描述为已修复的精确收敛。

仍有明确边界：缩放平移的约100ms空窗和冻结恢复时约369ms尾部尚未消除。
既有三图像保守瓦片 reservation 限制高DPR导航密度／驻留；本轮没有提前释放GPU未完成资源，
降低该记账需要独立的 completion-owned staging 生命周期设计与证明。
完整 Demo DOM 刷新、真实 Mac 触控板 input-to-photon、OS休眠／GPU驱逐和100K/1M正式验收不由此对照建立。

## 复测与身份

- 正常 Demo：<http://127.0.0.1:4219/example.html>
- 预置压力场景：<http://127.0.0.1:4219/mixed-navigation.html?dense=1>
- SDK：0.3.9，clean source `32418c783170eea56a4131579fac627c003c91bb`，`out/g5-dense-final-off/js`。
- 旧4218安装目录及用户页面未改动。服务由本任务独立启动。

复现矩阵（在仓库根目录，先使用锁定 SDK 构建对应安装目录；不同时运行构建／回归和性能采样）：

```sh
node verification/runners/g5/dense_navigation_matrix.mjs out/g5-large-on/js out/g5-dense-final-on/js out/g5-dense-matrix-on on
node verification/runners/g5/dense_navigation_matrix.mjs out/g5-large-off/js out/g5-dense-final-off/js out/g5-dense-matrix-off off
```

每个 browser worker 独立持有进程家族、180秒／3GiB summed-RSS 上限，并记录退出与清理。
on/、off/ 的部分大 JSON 使用无损 gzip；原始 worker SHA 文件描述原输出名称，顶层 SHA256.json 描述此归档。
consumer-artifacts.json 绑定完整安装文件；source-inputs.json 和每次 worker summary 绑定 source/runner identity。
exploratory/ 保留被拒绝候选和最初探索对照；这些未提交的 prototype source 仅有 hash／artifact 记录，
不作为最终实现可复现性的证据。最终实现／package source 均已提交并精确绑定。
