#include "canvas/render/render_view_runtime.hpp"

#include <cassert>
#include <type_traits>

namespace {

using canvas::foundation::SceneRevision;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::FrameId;
using canvas::render::FrameState;
using canvas::render::MetricsGeneration;
using canvas::render::RenderViewRuntime;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::ViewId;
using canvas::semantic::SemanticGeneration;

void twoViewsShareSceneIdentityWithoutSharingPerViewState() {
    constexpr SemanticGeneration sharedSceneGeneration{41};
    constexpr SceneRevision sharedSceneReadToken{73};

    const RenderViewRuntime primary{FrameState{
        .viewId = ViewId{1},
        .camera = CameraState{
            .worldCenter = WorldPoint{10.0F, 20.0F},
            .zoom = 2.0F,
            .rotationRadians = 0.25F,
            .generation = CameraGeneration{11},
        },
        .worldViewport = WorldRect{-90.0F, -30.0F, 110.0F, 70.0F},
        .metrics = SurfaceMetrics{
            .logicalWidth = 800.0F,
            .logicalHeight = 400.0F,
            .physicalWidth = 1600,
            .physicalHeight = 800,
            .devicePixelRatio = 2.0F,
            .displayScale = 1.0F,
        },
        .sceneGeneration = sharedSceneGeneration,
        .sceneReadToken = sharedSceneReadToken,
        .surfaceGeneration = SurfaceGeneration{101},
        .metricsGeneration = MetricsGeneration{201},
        .frameId = FrameId{301},
    }};
    const RenderViewRuntime overview{FrameState{
        .viewId = ViewId{2},
        .camera = CameraState{
            .worldCenter = WorldPoint{-300.0F, 500.0F},
            .zoom = 0.5F,
            .rotationRadians = 0.0F,
            .generation = CameraGeneration{12},
        },
        .worldViewport = WorldRect{-700.0F, 100.0F, 100.0F, 900.0F},
        .metrics = SurfaceMetrics{
            .logicalWidth = 600.0F,
            .logicalHeight = 600.0F,
            .physicalWidth = 1800,
            .physicalHeight = 1800,
            .devicePixelRatio = 3.0F,
            .displayScale = 1.25F,
        },
        .sceneGeneration = sharedSceneGeneration,
        .sceneReadToken = sharedSceneReadToken,
        .surfaceGeneration = SurfaceGeneration{102},
        .metricsGeneration = MetricsGeneration{202},
        .frameId = FrameId{302},
    }};

    const FrameState& first = primary.frameState();
    const FrameState& second = overview.frameState();

    assert(first.sceneGeneration == sharedSceneGeneration);
    assert(second.sceneGeneration == sharedSceneGeneration);
    assert(first.sceneReadToken == sharedSceneReadToken);
    assert(second.sceneReadToken == sharedSceneReadToken);

    assert(first.viewId == ViewId{1});
    assert(second.viewId == ViewId{2});
    assert(first.camera == (CameraState{WorldPoint{10.0F, 20.0F}, 2.0F, 0.25F,
                                               CameraGeneration{11}}));
    assert(second.camera == (CameraState{WorldPoint{-300.0F, 500.0F}, 0.5F, 0.0F,
                                                CameraGeneration{12}}));
    assert(first.worldViewport == (WorldRect{-90.0F, -30.0F, 110.0F, 70.0F}));
    assert(second.worldViewport == (WorldRect{-700.0F, 100.0F, 100.0F, 900.0F}));
    assert(first.metrics == (SurfaceMetrics{800.0F, 400.0F, 1600, 800, 2.0F, 1.0F}));
    assert(second.metrics == (SurfaceMetrics{600.0F, 600.0F, 1800, 1800, 3.0F, 1.25F}));
    assert(first.surfaceGeneration == SurfaceGeneration{101});
    assert(second.surfaceGeneration == SurfaceGeneration{102});
    assert(first.metricsGeneration == MetricsGeneration{201});
    assert(second.metricsGeneration == MetricsGeneration{202});
    assert(first.frameId == FrameId{301});
    assert(second.frameId == FrameId{302});
}

} // namespace

int main() {
    static_assert(!std::is_assignable_v<FrameState&, FrameState>);
    static_assert(std::is_same_v<decltype(RenderViewRuntime{FrameState{}}.frameState()),
                                 const FrameState&>);

    twoViewsShareSceneIdentityWithoutSharingPerViewState();
    return 0;
}
