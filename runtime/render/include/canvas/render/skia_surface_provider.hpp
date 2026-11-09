#pragma once

#include "canvas/render/render_backend.hpp"
#include "canvas/render/render_target.hpp"

#include <cstdint>
#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <utility>

class SkSurface;

namespace canvas::render {

// A platform owns the native window/context and exposes only a Skia drawing
// target to the common adapter. It must not interpret BrushPackage or Scene
// semantics. SurfaceLifecycle remains the logical lifetime; this interface is
// the resource lifetime (acquire/release/present/readback).
struct SkiaSurfaceFrame final {
    SkSurface* surface = nullptr;
    std::uint64_t generation = 0;
};

enum class SkiaSurfaceAcquireCode : std::uint8_t {
    kAcquired,
    kUnavailable,
    kLost,
};

struct SkiaSurfaceAcquireResult final {
    SkiaSurfaceAcquireCode code = SkiaSurfaceAcquireCode::kUnavailable;
    SkiaSurfaceFrame frame{};
    std::string message;

    [[nodiscard]] static SkiaSurfaceAcquireResult acquired(SkiaSurfaceFrame frame) {
        return {SkiaSurfaceAcquireCode::kAcquired, frame, {}};
    }
    [[nodiscard]] static SkiaSurfaceAcquireResult rejected(
        SkiaSurfaceAcquireCode code, std::string message) {
        return {code, {}, std::move(message)};
    }
};

// Source/developer-provider contract only; no consumer JS/DLL GPU handle API.
// service() is owner-thread, nonblocking and non-reentrant: no draw/present or
// Runtime/document callbacks. A shared service retains the context independently
// of the logical provider. Callback state outlives it without retaining context.
struct GpuCompletionState final {
    // Identity of this retained context lifetime, never a native numeric handle.
    const std::uint64_t lifetime = nextLifetime_.fetch_add(1, std::memory_order_relaxed);
    std::atomic<bool> abandoned=false,physicalLost=false;
  private:
    inline static std::atomic<std::uint64_t> nextLifetime_{1};
};
class GpuCompletionService {
  public:
    virtual ~GpuCompletionService()=default;
    virtual void service()noexcept=0;
    [[nodiscard]] virtual std::shared_ptr<const GpuCompletionState> state()const noexcept=0;
};
class SkiaSurfaceProvider {
  public:
    virtual ~SkiaSurfaceProvider() = default;
    // Null means unsupported; GPU optional caches then use Direct fallback.
    // Retaining this service must keep the physical context alive. Logical
    // lose/resize cannot report physical loss. Destruction marks abandoned
    // before draining Skia callbacks; only actual context loss sets physicalLost.
    [[nodiscard]] virtual std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept {return {};}

    [[nodiscard]] virtual RenderTargetInfo describe() const noexcept = 0;
    [[nodiscard]] virtual SkiaSurfaceAcquireResult acquire() noexcept = 0;
    virtual void release() noexcept = 0;
    virtual BackendSubmissionResult lose() noexcept {
        release();
        return BackendSubmissionResult::accepted();
    }
    [[nodiscard]] virtual BackendSubmissionResult present() noexcept = 0;
    [[nodiscard]] virtual BackendSubmissionResult resize(
        std::uint32_t width, std::uint32_t height) noexcept = 0;
    [[nodiscard]] virtual BackendSubmissionResult readbackRgba(
        std::span<std::uint8_t> destination) noexcept = 0;
    // Resource generation is intentionally exposed so the composition root
    // can bind a provider generation to SurfaceLifecycle. A frame acquired
    // from an old provider must never be presented after resize/profile
    // replacement.
    [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;
    // Advance the provider resource generation without changing its logical
    // dimensions. Profile switches use this to align a prepared provider
    // with the next logical SurfaceLifecycle generation.
    [[nodiscard]] virtual BackendSubmissionResult advanceGeneration() noexcept = 0;
    // Candidates keep native visuals detached until the owner has prepared
    // both roles. Activation is part of preparation and may fail; publishing
    // the accepted binding happens only after both activations succeed.
    [[nodiscard]] virtual BackendSubmissionResult activateBinding() noexcept {
        return BackendSubmissionResult::accepted();
    }
    virtual void deactivateBinding() noexcept { setOverlayVisible(false); }
    [[nodiscard]] virtual std::uint64_t readbackCount() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t cpuCopyCount() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t presentCount() const noexcept = 0;
    // Optional overlay visibility seam. Canonical providers ignore it; an
    // ARC preview provider hides only after a successful canonical-visible
    // handoff.
    virtual void setOverlayVisible(bool) noexcept {}
    [[nodiscard]] virtual bool overlayVisible() const noexcept { return true; }
};

// Deterministic reference provider. Android and Windows may use this provider
// for their current raster/readback paths; only the provider/present seam is
// platform-specific, while drawing remains in SkiaRenderer.
class RasterSkiaSurfaceProvider final : public SkiaSurfaceProvider {
  public:
    RasterSkiaSurfaceProvider() = default;
    ~RasterSkiaSurfaceProvider() override;
    RasterSkiaSurfaceProvider(const RasterSkiaSurfaceProvider&) = delete;
    RasterSkiaSurfaceProvider& operator=(const RasterSkiaSurfaceProvider&) = delete;

    [[nodiscard]] RenderTargetInfo describe() const noexcept override;
    [[nodiscard]] SkiaSurfaceAcquireResult acquire() noexcept override;
    void release() noexcept override;
    [[nodiscard]] BackendSubmissionResult lose() noexcept override;
    [[nodiscard]] BackendSubmissionResult present() noexcept override;
    [[nodiscard]] BackendSubmissionResult resize(
        std::uint32_t width, std::uint32_t height) noexcept override;
    [[nodiscard]] BackendSubmissionResult readbackRgba(
        std::span<std::uint8_t> destination) noexcept override;
    [[nodiscard]] std::uint64_t readbackCount() const noexcept override;
    [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override;
    [[nodiscard]] std::uint64_t presentCount() const noexcept override;
    [[nodiscard]] std::uint64_t generation() const noexcept override { return generation_; }
    [[nodiscard]] BackendSubmissionResult advanceGeneration() noexcept override;
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }

  private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t readbacks_ = 0;
    std::uint64_t copies_ = 0;
    std::uint64_t presents_ = 0;
    bool acquired_ = false;
    bool lost_ = false;
};

// Runtime-owned preview realization used by host adapters until a platform
// supplies a native transparent window/canvas. It deliberately has its own
// raster surface and generation; it is never the canonical provider.
class TransparentOverlaySkiaSurfaceProvider final : public SkiaSurfaceProvider {
  public:
    TransparentOverlaySkiaSurfaceProvider() = default;
    ~TransparentOverlaySkiaSurfaceProvider() override = default;
    [[nodiscard]] RenderTargetInfo describe() const noexcept override;
    [[nodiscard]] SkiaSurfaceAcquireResult acquire() noexcept override;
    void release() noexcept override;
    [[nodiscard]] BackendSubmissionResult lose() noexcept override;
    [[nodiscard]] BackendSubmissionResult present() noexcept override;
    [[nodiscard]] BackendSubmissionResult resize(std::uint32_t, std::uint32_t) noexcept override;
    [[nodiscard]] BackendSubmissionResult readbackRgba(std::span<std::uint8_t>) noexcept override;
    [[nodiscard]] std::uint64_t generation() const noexcept override;
    [[nodiscard]] BackendSubmissionResult advanceGeneration() noexcept override;
    [[nodiscard]] std::uint64_t readbackCount() const noexcept override;
    [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override;
    [[nodiscard]] std::uint64_t presentCount() const noexcept override;
    void setOverlayVisible(bool visible) noexcept override { visible_ = visible; }
    [[nodiscard]] bool overlayVisible() const noexcept override { return visible_; }
    [[nodiscard]] RasterSkiaSurfaceProvider& raster() noexcept { return raster_; }

  private:
    RasterSkiaSurfaceProvider raster_;
    bool visible_ = false;
};

} // namespace canvas::render
