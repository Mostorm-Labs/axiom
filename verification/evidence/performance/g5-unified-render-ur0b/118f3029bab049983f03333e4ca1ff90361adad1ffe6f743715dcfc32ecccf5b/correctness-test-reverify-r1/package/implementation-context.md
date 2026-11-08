# Narrow regression-fixture repair

The r2 harness is implemented; preserve it. One legacy positive test accidentally
uses ambient current compiler files with a frozen old-compiler reuse packet.
remaining_runner rejects this correctly. Fix only the test context and explicitly
retain actual-current compiler rejection. Do not change any reader or old packet.

No new product or Verification design is needed. Temporary archived compiler
fixtures are allowed; broad success mocks, skips and weakened checks are not.
Run focused and mandatory regressions, bind unchanged r2 compiler/evidence, then
return READY_FOR_CONTROL_REVIEW with proactive notification. Do not redo the
harness implementation or unchanged17 recompilations. No sampling/build/push.
