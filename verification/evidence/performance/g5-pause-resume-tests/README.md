# Pause/resume regression test closure

The user confirmed that the fixed 4242 Demo feels correct and requested stopping
performance optimization here, adding the scenarios to unit tests, and pushing.
This follow-up changes tests, CTest registration and runner documentation only.
The running Demo and production renderer remain at `0d581f89`.

`G5MixedNavigationReentry` covers a decoded PNG, rectangle/ellipse/triangle,
sticky, table, and five compact curves. Eight combinations exercise zoom-out/pan,
one/three narrow canonical completions and selected/unselected state. Four more
checks reject Scene changes, surface generation changes, a foreign provider and
explicit cache clearing. Assertions check immediate present within one bounded
work opportunity, visible image pixels outside the narrow source, no canonical
receipt from feedback, unchanged input content and final Direct pixel equality.

The unit test represents idle as no renderer calls between completion and
reentry. It does not sleep or impose unstable wall-clock limits. Actual 1000 ms
pause and synthetic 2.33 MB image loading remain covered by the existing
`pause_resume.mjs` root-SDK browser scenario and the previous 14-run bundle at
`../g5-pause-resume-repair/`; those frozen records were not rewritten.

`red.json` and `red.log.gz` show the expanded test failing at the intended missing
coverage assertion when the pre-fix renderer source from `108151fb` is compiled
and substituted into a separate test executable. Other libraries/headers are
current; this is an isolated regression sensitivity check, not a historical full
build. The normal worktree/build are never reverted. To repeat it, first produce
`out/g5-reentry-tests-link.txt` with `ninja -C out/host -t commands
runtime/render/axiom_render_cache_tests | tail -1`, then execute the preserved
`reproduce-red.py` from the repository root with a configured native build.

`result.json` binds the exact test/document/CMake bytes, full native failed-name
comparison, Node tests and Chrome/WebGL reentry probe. Compressed logs are kept
beside it. Full CTest retains existing failures, listed by name in `result.json`;
the suite is not described as entirely green. The GPU check is an actual render
correctness test, not an input latency measurement. No new Gate verdict or
platform qualification is issued here.
