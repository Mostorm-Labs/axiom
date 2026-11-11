# Axiom 版本规则

产品、SDK/API 显示 **`X.Y.Z.Build`**。`X.Y.Z` 是面向用户和生态的逻辑版本；
`Build` 标识一次流水线构建实体。此规则自 0.4.4 起替代此前按功能规模划分的版本规则；
历史发布、冻结 authority 和历史验证证据保持原样。

| 字段 | 递增条件 | 递增方式 |
| --- | --- | --- |
| X | 不兼容的公开 API 变更 | X + 1，Y/Z 清零 |
| Y | 向下兼容的新功能或能力废弃 | Y + 1，Z 清零 |
| Z | 向下兼容的修复、性能优化、安全修补，无新增功能 | Z + 1 |
| Build | 每次新的编译/打包流水线运行 | 主发布工作流的正整数运行序号 |

同次发布取最高变更级别。三段遵循 [SemVer 2.0.0](https://semver.org/lang/zh-CN/)，
无前导零，不使用 preview 后缀。0.x 属于早期开发，但仍如实声明兼容性。

## 身份与分发

- `sdk/version.json` 是逻辑版本与本地默认 `buildNumber` 的来源。
- SDK/API 的 `SDK_VERSION`、Runtime/Controller 的 `version`、`readBuildInfo().sdkVersion`
  都显示 `X.Y.Z.Build`。`SDK_SEMVER` / `sdkSemver` 返回三段，
  `SDK_BUILD_NUMBER` / `buildNumber` 返回可无损传输的整数。
- npm 的 package.json 保持如下结构；**四段显示版本不是 npm SemVer**：

```json
{"name":"@mostorm/axiom","version":"0.4.4","buildNumber":1234}
```

- npm 以 `name@X.Y.Z` 识别版本；自定义 buildNumber 不能让同一个版本重复发布。
  已发布逻辑版本不可覆盖。只有构建而无代码变化时，可产生不同 Build 的验证产物，
  不重复发布同一个 npm 版本。新 npm 发布须使用未发布的逻辑版本。
- GitHub tag 为 `vX.Y.Z`，Release 标题为 `Axiom SDK X.Y.Z.Build`。
  Web、Windows、各 RN 平台包、Demo 和 manifest 必须来自同一提交、同一逻辑版本和 Build。
  manifest 同时保存 `version`、`buildNumber`、`sdk_version`（四段）与提交身份。

## Build 生命周期

主发布工作流 `sdk-preview-release.yml` 的 `GITHUB_RUN_NUMBER` 从 1 开始单调递增，
可能因 PR、验证或失败任务而有空号。一次运行只分配一次，所有矩阵任务及可复用的
RN 发布工作流通过 `AXIOM_BUILD_NUMBER` 共享该值；RN 不独立分配发布构建号。
数值范围为 1–9007199254740991，保持 JSON/JavaScript 整数精度。无需重编依赖栈。

重新编译必须启动新的 workflow run（新的 Build）。旧 run 的构建阶段拒绝 run_attempt > 1。
仅重试上传/发布阶段时复用已有验证产物及 Build，不重新编译。已发布标签、assets、npm
版本不可覆盖；不允许靠重建旧 run 或修改 buildNumber 冒充相同产物。
未发布候选可按 AGENTS.md 的取消旧任务、保留证据、核对未发布状态和精确 lease 规则修复。

本地开发默认 Build=0，明确不属于正式流水线产物。可用
`AXIOM_BUILD_NUMBER=<number>` / CMake `-DAXIOM_BUILD_NUMBER=<number>` 验证构建身份；
正式流水线不修改源码中的 0，而是在构建/安装目录生成匹配的 API 常量、声明和元数据。

版本显示不改变 ABI version、transport version、Operation/Snapshot/styleVersion。
平台验证与兼容性声明仍由各自契约和实际证据负责。

参考：[npm package.json](https://docs.npmjs.com/cli/v11/configuring-npm/package-json/)、
[npm 发布不可覆盖](https://docs.npmjs.com/cli/v11/commands/npm-publish/)、
[GitHub Actions 运行变量](https://docs.github.com/en/actions/reference/workflows-and-actions/variables)。
