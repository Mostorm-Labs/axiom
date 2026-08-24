# Verification packages

这是 TypeScript workspace 的 package boundary。

- `platform-harness-protocol`：`GT-G0-02` 的 typed IDs、enums 与 envelope codec；
- `platform-harness-runner`：`GT-G0-03..04` 的 Reference Runner；
- `platform-harness-transport` 与 `platform-harness-scripted-adapter`：`GT-G0-05` 的受控验证边界；
- `platform-conformance-cli`：`GT-G0-07` 的共享验证入口。

这些包属于 Verification Foundation，不是 Axiom 产品 Runtime 或公共产品 SDK。
