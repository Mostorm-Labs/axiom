#include "canvas/render/render_target.hpp"

#include <cassert>

namespace {
canvas::render::RenderTargetInfo profile(const char* id,
                                         canvas::render::RenderTargetKind kind,
                                         canvas::render::RenderTargetBackend backend,
                                         bool gpu, bool present) {
    return {id, kind, backend, canvas::render::RenderTargetFormat::kRgba8888,
            {640.0F, 480.0F, 640U, 480U, 1.0F, 1.0F}, {gpu, gpu, false, true, present, gpu}};
}
} // namespace

int main() {
    using namespace canvas::render;
    SurfaceProfileRegistry board{ViewId{7}, profile(
        "cpu-raster", RenderTargetKind::kCpuRaster, RenderTargetBackend::kRaster,
        false, false)};
    assert(board.profiles().size() == 1U);
    assert(board.registerProfile(profile(
        "webgl-window", RenderTargetKind::kGpuWindow, RenderTargetBackend::kWebGL2,
        true, true)));
    assert(!board.registerProfile(profile(
        "webgl-window", RenderTargetKind::kGpuWindow, RenderTargetBackend::kWebGL2,
        true, true)));
    assert(board.switchTo("missing") == RenderTargetSwitchDisposition::kUnknownProfile);
    assert(board.switchTo("webgl-window") == RenderTargetSwitchDisposition::kSwitched);
    assert(board.activeProfile().backend == RenderTargetBackend::kWebGL2);
    assert(board.lifecycle().current().surfaceGeneration.value() == 2U);
    assert(board.lifecycle().current().metricsGeneration.value() == 2U);
    assert(board.lifecycle().available());
    assert(board.markLost() == RenderTargetSwitchDisposition::kSwitched);
    assert(!board.lifecycle().available());
    return 0;
}
