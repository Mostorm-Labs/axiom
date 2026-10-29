# FC04 D07 audit receipt

Active product subject is Selection Batch3 c7f87439, not main. See result.json, report.md, matrix.json and commands.json. The current-selection directory contains separately executed current-branch raw evidence; main-comparison retains the original baseline run only. No current PASS/RED is inferred from main.

Build/test commands are recorded as argv arrays with exit codes. configure.py and run_existing.py are local reproduction drivers; discover SDK/toolchain paths rather than assuming these machine paths exist on another device. Locked SDK resolver environment was imported, no Semantic/Skia dependency was rebuilt. Frozen ignored H03/H04 inputs were copied only after checking committed accepted hashes; cache restoration records identify exact bytes. No expected values or historical results were overwritten.

Audit sources are opt-in verification/fc04_d07 files. The two group-gap REDs are counterexamples, not a future Gate runner or a request to change the safe lower span guard. The receipt-growth process records std::terminate and exits86 to avoid an OS dialog; it is not inverted into a passing test. Direct logs preserve full passed-test observations beyond CTest truncation. Native only, no full multi-intent end-to-end/physical/Gate claim.

Later evidence commits change no tested product or harness source bytes. Original Selection/main branch refs and the public Windows exe are preserved.
