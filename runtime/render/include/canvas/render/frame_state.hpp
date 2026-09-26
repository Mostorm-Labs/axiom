#pragma once

#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/surface_metrics.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <compare>
#include <cstdint>

namespace canvas::render {

template <typename Tag> class Identity final {
  public:
    constexpr Identity() = default;
    explicit constexpr Identity(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const Identity&) const = default;

  private:
    std::uint64_t value_ = 0;
};

struct ViewIdTag;
struct CameraGenerationTag;
struct SurfaceGenerationTag;
struct MetricsGenerationTag;
struct FrameIdTag;

using ViewId = Identity<ViewIdTag>;
using CameraGeneration = Identity<CameraGenerationTag>;
using SurfaceGeneration = Identity<SurfaceGenerationTag>;
using MetricsGeneration = Identity<MetricsGenerationTag>;
using FrameId = Identity<FrameIdTag>;

struct CameraState final {
    const foundation::WorldPoint worldCenter{};
    const float zoom = 1.0F;
    const float rotationRadians = 0.0F;
    const CameraGeneration generation{};

    [[nodiscard]] constexpr bool operator==(const CameraState& other) const noexcept {
        return worldCenter.x == other.worldCenter.x &&
               worldCenter.y == other.worldCenter.y && zoom == other.zoom &&
               rotationRadians == other.rotationRadians && generation == other.generation;
    }
};

// Immutable renderer-neutral input for one view and frame. Scene identity is
// shared G2 truth; every remaining identity is scoped to this view/surface.
struct FrameState final {
    const ViewId viewId{};
    const CameraState camera{};
    const foundation::WorldRect worldViewport{};
    const SurfaceMetrics metrics{};
    const semantic::SemanticGeneration sceneGeneration{};
    const foundation::SceneRevision sceneReadToken{};
    const SurfaceGeneration surfaceGeneration{};
    const MetricsGeneration metricsGeneration{};
    const FrameId frameId{};
    // View-space clip for the renderer.  worldViewport remains the query
    // rectangle in world coordinates; callers that do not have a separate
    // clip retain the legacy fallback in DirectReferenceSource.
    const foundation::WorldRect viewportClip{0.0F, 0.0F, -1.0F, -1.0F};

    bool operator==(const FrameState&) const = default;
};

} // namespace canvas::render
