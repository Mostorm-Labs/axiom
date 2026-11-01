# Runtime qualification fixtures

This directory retains the regression fixtures and qualification runners formerly
hosted by `apps/ink_playground`. They are test consumers, not another product
Runtime or a shipped SDK application. Existing executable/CTest identifiers are
retained so previous regression selections remain usable.

`AXIOM_BUILD_RUNTIME_QUALIFICATION=ON` builds these fixtures when `BUILD_TESTING`
is enabled. Product applications use `apps/minimal_runtime_host`,
`apps/axiom_canvas_demo`, or the installed SDK examples instead.

The legacy Web qualification fixture remains available only with
`AXIOM_BUILD_QUALIFICATION_WEB_HOST=ON`. Its build does not establish current SDK
or browser product acceptance. The Arc-dependent Windows/Android applications,
Arc module, FastInk experiment, and their application-specific static tests have
been retired. Historical packages/results still refer to their original source
revisions and have not been rewritten.
