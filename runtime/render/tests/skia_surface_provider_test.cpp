#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace canvas::render;
    RasterSkiaSurfaceProvider provider;
    assert(provider.describe().backend == RenderTargetBackend::kRaster);
    assert(provider.acquire().code == SkiaSurfaceAcquireCode::kUnavailable);
    assert(provider.resize(32U, 32U).code == BackendSubmissionCode::kAccepted);

    SkiaRenderer renderer;
    const canvas::ink::BrushPrimitive primitive{16.0F, 16.0F, 8.0F, 0.0F, 1.0F,
                                                canvas::ink::BrushRepresentation::kVector};
    assert(renderer.renderPrimitives(provider, {&primitive, 1U}, 1.0F, 0.0F, 0.0F).code ==
           BackendSubmissionCode::kAccepted);
    assert(renderer.submissionCount() == 1U);
    std::vector<std::uint8_t> pixels(32U * 32U * 4U);
    assert(provider.readbackRgba(pixels).code == BackendSubmissionCode::kAccepted);
    assert(provider.readbackCount() == 1U);
    assert(provider.cpuCopyCount() == 1U);
    auto acquired = provider.acquire();
    assert(acquired.code == SkiaSurfaceAcquireCode::kAcquired);
    assert(acquired.frame.surface != nullptr);
    assert(provider.present().code == BackendSubmissionCode::kAccepted);
    provider.release();
    // renderPrimitives owns the complete submit/present path; this explicit
    // present is a second presentation and must be observable.
    assert(provider.presentCount() == 2U);
    assert(provider.resize(64U, 64U).code == BackendSubmissionCode::kAccepted);
    assert(provider.acquire().frame.generation > acquired.frame.generation);
    provider.release();

    const FrameState frame{
        ViewId{1}, CameraState{canvas::foundation::WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F,
                               CameraGeneration{1}},
        canvas::foundation::WorldRect{0.0F, 0.0F, 64.0F, 64.0F},
        SurfaceMetrics{64.0F, 64.0F, 64U, 64U, 1.0F, 1.0F},
        canvas::semantic::SemanticGeneration{1}, canvas::foundation::SceneRevision{1},
        SurfaceGeneration{1}, MetricsGeneration{1}, FrameId{1}};
    ReferenceDrawList drawList{
        frame, canvas::foundation::WorldRect{0.0F, 0.0F, 64.0F, 64.0F}, 0U, 0U,
        WorldToViewAffine{}, canvas::foundation::WorldRect{0.0F, 0.0F, 64.0F, 64.0F},
        {}, ReferenceTraversalDiagnostics{}, ReferenceDrawList::kCanonicalEncodingVersion,
        CanonicalByteOrder::kLittleEndian, {}, {}};
    const FrameState staleFrame{
        frame.viewId, frame.camera, frame.worldViewport, frame.metrics,
        frame.sceneGeneration, frame.sceneReadToken, SurfaceGeneration{2},
        frame.metricsGeneration, frame.frameId};
    FramePlan stalePlan{staleFrame, drawList};
    assert(renderer.renderFrame(provider, stalePlan).code == BackendSubmissionCode::kRejected);
    const FrameState backgroundFrame{
        frame.viewId, frame.camera, frame.worldViewport, frame.metrics,
        frame.sceneGeneration, frame.sceneReadToken, SurfaceGeneration{provider.generation()},
        frame.metricsGeneration, frame.frameId};
    ReferenceDrawList backgroundList{
        backgroundFrame, canvas::foundation::WorldRect{0,0,64,64}, 0U, 0U,
        WorldToViewAffine{}, canvas::foundation::WorldRect{0,0,64,64},
        {}, ReferenceTraversalDiagnostics{}, ReferenceDrawList::kCanonicalEncodingVersion,
        CanonicalByteOrder::kLittleEndian, {}, {}};
    canvas::foundation::DocumentBackground background;
    background.color = {0,0,1,1}; background.patternColor = {1,0,0,1};
    std::vector<std::uint8_t> backgroundPixels(64U*64U*4U);
    const auto renderBackground = [&] {
        const auto plan = FramePlanBuilder::build(backgroundFrame, backgroundList, background);
        assert(plan && renderer.renderFrame(provider, plan.value()).code == BackendSubmissionCode::kAccepted);
        assert(provider.readbackRgba(backgroundPixels).code == BackendSubmissionCode::kAccepted);
    };
    const auto red = [&](unsigned x,unsigned y) { return backgroundPixels[(y*64U+x)*4U]; };
    renderBackground();
    assert(red(12,12) == 0 && backgroundPixels[(12U*64U+12U)*4U+2U] == 255);
    background.mode = canvas::foundation::BackgroundMode::kDots;
    renderBackground(); assert(red(24,24) > 100 && red(12,12) == 0 && red(24,12) == 0);
    background.mode = canvas::foundation::BackgroundMode::kGrid;
    renderBackground(); assert(red(24,12) > 0 && red(12,12) == 0);
    return 0;
}
