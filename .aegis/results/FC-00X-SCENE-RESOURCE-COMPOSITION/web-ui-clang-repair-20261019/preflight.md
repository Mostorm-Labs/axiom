# Web UI width and strict compiler follow-up

User authorization: “这一轮验证中，整体功能是可以的，只是debug ui的宽度非常大，几乎占据了浏览器画面的4/5，需要修复下，然后就是你报的这个clang诊断阻塞需要修复下”.
This explicitly authorizes the two previously excluded local source diagnostics and their focused regression.
Start revision f4254a0d75c88267685831d4c234972014759b4c, remote execution head matches; original baseline
f1d72f22ad20209c8ccdbdd7474d86c651434914 remains unchanged and is an ancestor. Reuse the clean isolated worktree.
The clone has narrow remote fetch refspecs; direct ls-remote and explicit fetch reconcile the execution branch.
Nine frozen package/oracle Git-blob hashes match. No package or Authority mutation is needed.

UI cause: CSS canvas and Web platform surface/ImGui viewport all use 900 px. Use the existing Windows
host's 410 px panel width on Web as well; keep CSS/backing/viewport/input mapping consistent. Existing
workbench layout, control routing, ownership and Runtime gesture policies remain unchanged.

Compiler causes: integral promotion gives StableOrderKey::value()'s conditional bound type int while
the loop uses size_t; use an explicitly size_t bound without changing the 8-byte cap. A dangling else
in referencesResource binds later content branches to an inner RichText run condition. Explicit braces
restore the already intended per-content resource reference checks. Verify both referenced and unrelated
resource removal for Image, RichText, VectorStroke, DabStroke, BrushStroke and DabBrushStroke.

REDs: real browser rejects the current UI's 485.5 px width after the fixture's resize; two fresh Clang
syntax checks reject the known original diagnostics. A focused native reference-removal test is run
before the corresponding production change. Preserve original FAIL evidence.

Validation: remove both local diagnostic exceptions from the actual Web consumer configuration;
build strict Web ON/full and OFF; browser regressions and measured UI width; rebuild affected native
regressions with MSVC 19.51 /WX and exercise semantic prepare/removal plus Runtime/host checks. Reuse
locked SDKs, never rebuild dependency SDKs. Keep earlier result directories and other checkouts intact.
No new architecture, schema/ABI, registry, gesture recognition or performance project. No Gate/PHYS claim.
