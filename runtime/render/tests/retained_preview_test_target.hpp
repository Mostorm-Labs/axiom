#pragma once

#include "canvas/render/skia_surface_provider.hpp"

namespace canvas::render::testing {
// Model the same retained-target capability used by preserveDrawingBuffer WebGL.
class RetainedPreviewTarget final : public SkiaSurfaceProvider {
public:
    RenderTargetInfo describe() const noexcept override {
        auto info = target.describe();
        info.capabilities.supportsPartialUpdate = true;
        return info;
    }
    SkiaSurfaceAcquireResult acquire() noexcept override { return target.acquire(); }
    void release() noexcept override { target.release(); }
    BackendSubmissionResult present() noexcept override { return target.present(); }
    BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override { return target.resize(w,h); }
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out) noexcept override { return target.readbackRgba(out); }
    std::uint64_t generation() const noexcept override { return target.generation(); }
    BackendSubmissionResult advanceGeneration() noexcept override { return target.advanceGeneration(); }
    std::uint64_t readbackCount() const noexcept override { return target.readbackCount(); }
    std::uint64_t cpuCopyCount() const noexcept override { return target.cpuCopyCount(); }
    std::uint64_t presentCount() const noexcept override { return target.presentCount(); }
private:
    RasterSkiaSurfaceProvider target;
};
} // namespace canvas::render::testing
