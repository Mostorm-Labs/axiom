#pragma once

#include "canvas/render/render_backend.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace canvas::render {

struct WebGlSurfaceConfig final {
    std::string canvasSelector;
    std::uint32_t physicalWidth = 0;
    std::uint32_t physicalHeight = 0;
};

// Private production backend target for the G3 Web reference host. The public
// seam is renderer-neutral; Emscripten, GLES and Skia stay in the .cpp file.
class WebGlSurfaceBackend final : public IRenderBackend {
  public:
    explicit WebGlSurfaceBackend(WebGlSurfaceConfig config);
    ~WebGlSurfaceBackend() override;
    WebGlSurfaceBackend(const WebGlSurfaceBackend&) = delete;
    WebGlSurfaceBackend& operator=(const WebGlSurfaceBackend&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] const std::string& error() const noexcept;
    [[nodiscard]] BackendSubmissionResult submit(const FramePlan& plan) override;
    [[nodiscard]] BackendSubmissionResult submitBrushPrimitives(
        std::span<const canvas::ink::BrushPrimitive> primitives,
        CanonicalViewportTransform viewport = {});

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace canvas::render
