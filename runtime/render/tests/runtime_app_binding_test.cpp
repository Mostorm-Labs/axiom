#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cassert>
#include <memory>

namespace {
canvas::render::RenderTargetInfo info(const char* id) {
    return {id, canvas::render::RenderTargetKind::kCpuRaster,
            canvas::render::RenderTargetBackend::kRaster,
            canvas::render::RenderTargetFormat::kRgba8888,
            {32.0F, 32.0F, 32U, 32U, 1.0F, 1.0F},
            {false, false, false, true, false, false}};
}
}

int main() {
    using namespace canvas::render;
    RuntimeAppBinding binding{ViewId{1}, SurfaceProfile{info("cpu"),
                                                        std::make_unique<RasterSkiaSurfaceProvider>()}};
    assert(binding.registerInputSource({7U, "android"}));
    assert(!binding.registerInputSource({7U, "android"}));
    canvas::input::PlatformPointerBatch batch;
    batch.samples.push_back({7U, 3U, 1U, 1U, 4.0F, 5.0F, 0.5F, 0.0F, 0.0F,
                             {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                             canvas::input::PointerPhase::kDown});
    const auto result = binding.submit(7U, batch);
    assert(result.accepted && result.normalized.samples.size() == 1U);
    assert(result.normalized.samples.front().key.valid());
    assert(binding.notifySourceLost(7U, 2U, 2U));
    assert(binding.registerSurfaceProfile(
               SurfaceProfile{info("second"), std::make_unique<RasterSkiaSurfaceProvider>()}) ==
           SurfaceProviderDisposition::kCommitted);
    assert(binding.selectRenderProfile("second", RenderTargetFormat::kRgba8888) ==
           SurfaceProviderDisposition::kCommitted);
    assert(binding.surfaces().activeInfo().profileId == "second");
    assert(binding.surfaces().lifecycle().current().surfaceGeneration.value() == 2U);
    assert(binding.selectRenderProfile("cpu", RenderTargetFormat::kRgba8888) ==
           SurfaceProviderDisposition::kCommitted);
    assert(binding.surfaces().activeInfo().profileId == "cpu");
    return 0;
}
