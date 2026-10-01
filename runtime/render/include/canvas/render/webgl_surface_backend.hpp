#pragma once

#include "canvas/render/render_backend.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace canvas::render {

struct WebGlSurfaceConfig final {
    // Legacy compatibility only. New Web apps must bind an app-owned
    // Emscripten WebGL context and leave this empty; the provider never
    // queries the DOM or creates a canvas in that mode.
    std::string canvasSelector;
    std::uint32_t physicalWidth = 0;
    std::uint32_t physicalHeight = 0;
    std::int32_t appOwnedContext = 0;
};

// Web-owned resource provider. WebGL is a Skia surface realization, not a
// competing renderer: it creates the context/framebuffer, exposes SkSurface,
// and owns flush/present only.
class WebGlSurfaceProvider final : public SkiaSurfaceProvider {
  public:
    explicit WebGlSurfaceProvider(WebGlSurfaceConfig config);
    // The Web App creates the canvas/context and makes it current. This
    // factory only wraps that already-owned context in a Skia surface.
    [[nodiscard]] static std::unique_ptr<WebGlSurfaceProvider> fromCurrentContext(
        std::uint32_t physicalWidth, std::uint32_t physicalHeight);
    ~WebGlSurfaceProvider() override;
    WebGlSurfaceProvider(const WebGlSurfaceProvider&) = delete;
    WebGlSurfaceProvider& operator=(const WebGlSurfaceProvider&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] const std::string& error() const noexcept;
    [[nodiscard]] RenderTargetInfo describe() const noexcept override;
    [[nodiscard]] SkiaSurfaceAcquireResult acquire() noexcept override;
    void release() noexcept override;
    [[nodiscard]] BackendSubmissionResult lose() noexcept override;
    [[nodiscard]] BackendSubmissionResult present() noexcept override;
    [[nodiscard]] BackendSubmissionResult resize(std::uint32_t width,
                                                 std::uint32_t height) noexcept override;
    [[nodiscard]] BackendSubmissionResult readbackRgba(
        std::span<std::uint8_t> destination) noexcept override;
    [[nodiscard]] std::uint64_t readbackCount() const noexcept override { return 0; }
    [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override { return 0; }
    [[nodiscard]] std::uint64_t presentCount() const noexcept override;
    [[nodiscard]] std::uint64_t generation() const noexcept override;
    [[nodiscard]] BackendSubmissionResult advanceGeneration() noexcept override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Compatibility facade for existing hosts. Drawing is delegated to the same
// SkiaRenderer used by raster providers; this class no longer owns a second
// Web-specific brush renderer.
class [[deprecated("use WebGlSurfaceProvider with SkiaRenderer")]] WebGlSurfaceBackend final : public IRenderBackend {
  public:
    explicit WebGlSurfaceBackend(WebGlSurfaceConfig config);
    ~WebGlSurfaceBackend() override;
    WebGlSurfaceBackend(const WebGlSurfaceBackend&) = delete;
    WebGlSurfaceBackend& operator=(const WebGlSurfaceBackend&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] const std::string& error() const noexcept;
    [[nodiscard]] BackendSubmissionResult submit(const FramePlan& plan) override;
    [[nodiscard]] BackendSubmissionResult submitBrushPrimitives(
        std::span<const canvas::ink::BrushPrimitive> primitives);
    [[nodiscard]] BackendSubmissionResult submitBrushPoints(
        std::span<const BrushRenderPoint> points);
    [[nodiscard]] BackendSubmissionResult submitBrushPoints(
        std::span<const BrushRenderPoint> points,
        CanonicalViewportTransform viewport);
    [[nodiscard]] std::uint64_t submissionCount() const noexcept;
    [[nodiscard]] std::uint64_t flushCount() const noexcept;
    [[nodiscard]] std::uint64_t readbackCount() const noexcept { return 0; }
    [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept { return 0; }

  private:
    WebGlSurfaceProvider provider_;
    SkiaRenderer renderer_;
};

} // namespace canvas::render
