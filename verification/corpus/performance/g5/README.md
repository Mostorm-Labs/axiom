# G5 preparation profiles

Owner：用户授权的 G5 前置核对与性能采样准备。不是 WP-G5-00 的 fixture acceptance。

`profiles.json` 固定 seed=42，Ink 单笔 64/256/1024/4096 样本和五次重复。原前置 corpus 的 package selector 是 `vector-solid-v1@1`。
Ink v2 交付后的独立 corpus 见 `ink-v2-locality.json`，选择 `round-handdrawn@1`
和 `chalk-compact-v1@1`，包 digest 与 `6c46f540` 绑定。`browser_metrics.mjs` 生成确定性位置与顺序；真实 CDP 测量使用浏览器 native timestamps，不伪造 4ms 设备采样率。

大画布 1K/10K/50K/100K、1M engineering、text-heavy/dense-grid/font-cold/font-warm/FC-12 mixed 仅登记为后续义务，尚未生成/验收。不得用 Ink timing 替代。

本轮没有新的 golden output 或性能通过阈值。结果仅用于在相同源码、SDK、renderer、机器、输入节奏下比较前后变化；所有原始 samples、warmup、长尾、失败与缺失 metric 均保留。
