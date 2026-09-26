#include "canvas/render/skia_surface_provider.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <limits>

namespace canvas::render {

struct RasterSkiaSurfaceProvider::Impl final {
    sk_sp<SkSurface> surface;
};

RasterSkiaSurfaceProvider::~RasterSkiaSurfaceProvider() { delete impl_; }

RenderTargetInfo RasterSkiaSurfaceProvider::describe() const noexcept {
    return {"cpu-raster", RenderTargetKind::kCpuRaster, RenderTargetBackend::kRaster,
            RenderTargetFormat::kRgba8888,
            {static_cast<float>(width_), static_cast<float>(height_), width_, height_, 1.0F, 1.0F},
            {false, false, false, true, false, false}};
}

SkiaSurfaceAcquireResult RasterSkiaSurfaceProvider::acquire() noexcept {
    if (lost_ || impl_ == nullptr || !impl_->surface || width_ == 0U || height_ == 0U) {
        return SkiaSurfaceAcquireResult::rejected(
            SkiaSurfaceAcquireCode::kUnavailable, "raster Skia surface is not initialized");
    }
    acquired_ = true;
    return SkiaSurfaceAcquireResult::acquired({impl_->surface.get(), generation_});
}

void RasterSkiaSurfaceProvider::release() noexcept { acquired_ = false; }

BackendSubmissionResult RasterSkiaSurfaceProvider::lose() noexcept {
    acquired_ = false;
    lost_ = true;
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult RasterSkiaSurfaceProvider::present() noexcept {
    if (impl_ == nullptr || !impl_->surface) return BackendSubmissionResult::rejected("Skia surface is not ready");
    ++presents_;
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult RasterSkiaSurfaceProvider::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
    if (width == 0U || height == 0U || width > 16384U || height > 16384U) {
        return BackendSubmissionResult::rejected("invalid Skia raster surface dimensions");
    }
    if (static_cast<std::uint64_t>(width) * height >
        std::numeric_limits<std::size_t>::max() / 4U) {
        return BackendSubmissionResult::rejected("Skia raster surface size overflow");
    }
    if (impl_ == nullptr) impl_ = new Impl{};
    if (impl_->surface && width_ == width && height_ == height && !lost_) {
        return BackendSubmissionResult::accepted();
    }
    const auto info = SkImageInfo::MakeN32Premul(static_cast<int>(width), static_cast<int>(height));
    auto surface = SkSurfaces::Raster(info);
    if (!surface) return BackendSubmissionResult::rejected("Skia raster surface creation failed");
    impl_->surface = std::move(surface);
    width_ = width;
    height_ = height;
    ++generation_;
    acquired_ = false;
    lost_ = false;
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult RasterSkiaSurfaceProvider::readbackRgba(
    std::span<std::uint8_t> destination) noexcept {
    if (impl_ == nullptr || !impl_->surface || width_ == 0U || height_ == 0U) {
        return BackendSubmissionResult::rejected("Skia raster surface is not initialized");
    }
    const auto required = static_cast<std::size_t>(width_) * height_ * 4U;
    if (destination.size() < required) return BackendSubmissionResult::rejected("readback buffer too small");
    const auto info = SkImageInfo::Make(static_cast<int>(width_), static_cast<int>(height_),
                                        kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                                        SkColorSpace::MakeSRGB());
    if (!impl_->surface->readPixels(info, destination.data(), static_cast<std::size_t>(width_) * 4U, 0, 0)) {
        return BackendSubmissionResult::rejected("Skia raster readback failed");
    }
    ++readbacks_;
    ++copies_;
    return BackendSubmissionResult::accepted();
}

std::uint64_t RasterSkiaSurfaceProvider::readbackCount() const noexcept { return readbacks_; }
std::uint64_t RasterSkiaSurfaceProvider::cpuCopyCount() const noexcept { return copies_; }
std::uint64_t RasterSkiaSurfaceProvider::presentCount() const noexcept { return presents_; }

BackendSubmissionResult RasterSkiaSurfaceProvider::advanceGeneration() noexcept {
    if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
        return BackendSubmissionResult::rejected("Skia raster surface generation exhausted");
    }
    ++generation_;
    acquired_ = false;
    lost_ = false;
    return BackendSubmissionResult::accepted();
}

RenderTargetInfo TransparentOverlaySkiaSurfaceProvider::describe() const noexcept {
    auto info = raster_.describe();
    info.profileId = "transparent-overlay";
    info.kind = RenderTargetKind::kExternal;
    info.capabilities.supportsPresent = true;
    return info;
}

SkiaSurfaceAcquireResult TransparentOverlaySkiaSurfaceProvider::acquire() noexcept {
    return raster_.acquire();
}

void TransparentOverlaySkiaSurfaceProvider::release() noexcept { raster_.release(); }

BackendSubmissionResult TransparentOverlaySkiaSurfaceProvider::lose() noexcept {
    visible_ = false;
    return raster_.lose();
}

BackendSubmissionResult TransparentOverlaySkiaSurfaceProvider::present() noexcept {
    const auto result = raster_.present();
    if (result.code == BackendSubmissionCode::kAccepted) visible_ = true;
    return result;
}

BackendSubmissionResult TransparentOverlaySkiaSurfaceProvider::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
    visible_ = false;
    return raster_.resize(width, height);
}

BackendSubmissionResult TransparentOverlaySkiaSurfaceProvider::readbackRgba(
    std::span<std::uint8_t> destination) noexcept {
    return raster_.readbackRgba(destination);
}

std::uint64_t TransparentOverlaySkiaSurfaceProvider::generation() const noexcept {
    return raster_.generation();
}

BackendSubmissionResult TransparentOverlaySkiaSurfaceProvider::advanceGeneration() noexcept {
    visible_ = false;
    return raster_.advanceGeneration();
}

std::uint64_t TransparentOverlaySkiaSurfaceProvider::readbackCount() const noexcept {
    return raster_.readbackCount();
}

std::uint64_t TransparentOverlaySkiaSurfaceProvider::cpuCopyCount() const noexcept {
    return raster_.cpuCopyCount();
}

std::uint64_t TransparentOverlaySkiaSurfaceProvider::presentCount() const noexcept {
    return raster_.presentCount();
}

} // namespace canvas::render
