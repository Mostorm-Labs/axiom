# Exact saturated-interior acceleration

Follow-up to G5_GPU_COVERAGE_RESULTS.md under the user-authorized
G5_GPU_COVERAGE_ALIGNMENT.md. Existing Runtime/Scene/Skia/provider ownership,
shader assets/digests and public SDK contracts remain unchanged.

Full gradient GPU evaluation remains byte-inconsistent at AA edges. A WebGL
shader-source capture shows highp coordinate/geometry arithmetic, mediump return
and blending; changing generated precision in a throwaway test did not fix the
Direct differential. No raw GL shader interception ships in production.

The implemented optimization uses Skia GPU coverage only for a conservatively
inscribed saturated interior of algorithm 3 round/marker. It obtains radius and
taper points from the existing ink::compactCoverage, then subtracts AA, float,
region scan conversion and full-pixel guard. Inscribed nib-space polygons and
segment rectangles classify work; they are never used as substitute brush
outline. Outside that subset the pinned CPU shader evaluates every original
segment with the original matrix, caps, distance/taper and 32-knot resume units.
The GPU target starts with the pinned saturated N32 value 11/255. Exact CPU edge
pixels replace its complement using Src before the unchanged material shader
executes once. There is no readback or CPU wait in this rendering path.

This removes CPU pixel work instead of accepting a tolerance, moving geometry
or changing the encoded package. Legacy gradients, thin/empty interiors, CPU
surfaces, huge origin/scale, >1024 knots, >4096 expanded points, >8192 surface
height/region complexity, denied region budgets all retain CPU coverage. Eight
full/edge CPU/GPU target slots have scratch leases; region/path work reserves a
conservative transient budget and releases with the job. Backend snapshot/COW
and driver allocations remain separate from this explicit accounting.

Both sync cache construction and cooperative exact preparation use the same
job. CPU Direct remains unchanged. Source/target/camera/lifecycle invalidation,
preview retirement and full-frame canonical receipts are unchanged. Android
raster/Windows GDI stay CPU; GPU providers may reuse the shared realization, but
only Chrome ANGLE Metal has new GPU execution evidence here.

Verification and final timings are retained in
verification/evidence/performance/g5-coverage-alignment. Baseline evidence from
g5-exact-coverage remains historical; its GPU blocker described the old full
shader candidate, not the new hybrid. Full GPU edge authority is still open.

Final Chrome diagnostic (revision3, 800x500 DPR2): 16 strokes exact receipt
447.7ms vs prior 677.8ms; 64 strokes 1495.2ms vs prior 2282.5ms. CPU attempt
total 226.4/1164.6ms vs 436.6/1730.1ms; peak attempt 14.1/20.0ms vs 24.5/23.1ms.
Final RGBA SHA256 equals prior CPU fallback for both corpora. Earlier hybrid
64-stroke run observed 1315.3ms, illustrating machine-load variance. No SLO or
physical trackpad/GPU execution guarantee follows. Native G5 27/27, v3 tuning,
actual WebGL extended pixel corpus, SDK lifecycle and navigation pass.

Default root Release verification exposes old NDEBUG/assert-only test build
errors. Ignored diagnostic build rules enable assertions for those tests only;
source and production libraries stay unchanged. Complete failure names/logs are
in the new evidence directory; no full-suite PASS is claimed. The original
G3 snapshot/semantic blocker is not bypassed.

The final diagnostic suite passes 218/238 with 20 remaining failures; all 20
names occur in the historical failure set. Two historical names are absent, but
this slice does not claim to have fixed those tests. The default Release build
is still blocked by assertion-only unused variables in old tests. No repository
test source/CMake policy was changed to bypass it.
