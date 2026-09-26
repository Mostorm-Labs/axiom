#pragma once

#include "canvas/render/surface_lifecycle.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <functional>
#include <memory>

namespace canvas::render { class SkiaSurfaceProvider; }

namespace canvas::render {

// A render target is the output contract between Render Core and a platform
// host. It deliberately contains no HWND, ANativeWindow, DOM canvas, EGL,
// SkSurface, or other backend handle. Those handles remain private to the
// selected target adapter.
enum class RenderTargetKind : std::uint8_t {
    kCpuRaster,
    kGpuWindow,
    kGpuOffscreen,
    kExternal,
};

enum class RenderTargetBackend : std::uint8_t {
    kRaster,
    kOpenGL,
    kWebGL2,
    kMetal,
    kVulkan,
    kD3D12,
    kPlatform,
};

enum class RenderTargetFormat : std::uint8_t {
    kRgba8888,
    kBgra8888,
    kRgba16Float,
};

struct RenderTargetCapabilities final {
    bool gpuAccelerated = false;
    bool supportsMsaa = false;
    bool supportsHdr = false;
    bool supportsReadback = false;
    bool supportsPresent = false;
    bool supportsPartialUpdate = false;

    bool operator==(const RenderTargetCapabilities&) const = default;
};

struct RenderTargetInfo final {
    std::string profileId;
    RenderTargetKind kind = RenderTargetKind::kCpuRaster;
    RenderTargetBackend backend = RenderTargetBackend::kRaster;
    RenderTargetFormat format = RenderTargetFormat::kRgba8888;
    SurfaceMetrics metrics{};
    RenderTargetCapabilities capabilities{};

    bool operator==(const RenderTargetInfo&) const = default;
};

enum class RenderTargetSwitchDisposition : std::uint8_t {
    kSwitched,
    kUnknownProfile,
    kInvalidProfile,
    kGenerationExhausted,
};

// Owns only surface profile metadata and logical SurfaceLifecycle state. It
// never selects a renderer: SkiaRenderer remains the sole render adapter and
// the selected platform provider realizes the active profile.
class SurfaceProfileRegistry final {
  public:
    SurfaceProfileRegistry(ViewId viewId, RenderTargetInfo initial);

    [[nodiscard]] bool registerProfile(RenderTargetInfo profile);
    [[nodiscard]] RenderTargetSwitchDisposition switchTo(std::string_view profileId);
    [[nodiscard]] RenderTargetSwitchDisposition markLost() noexcept;
    [[nodiscard]] const RenderTargetInfo& activeProfile() const noexcept { return *active_; }
    [[nodiscard]] const SurfaceLifecycle& lifecycle() const noexcept { return lifecycle_; }
    [[nodiscard]] SurfaceLifecycle& lifecycle() noexcept { return lifecycle_; }
    [[nodiscard]] const std::vector<RenderTargetInfo>& profiles() const noexcept {
        return profiles_;
    }

  private:
    [[nodiscard]] static bool validProfile(const RenderTargetInfo& profile) noexcept;

    std::vector<RenderTargetInfo> profiles_;
    std::optional<RenderTargetInfo> active_;
    SurfaceLifecycle lifecycle_;
};

using RenderTargetSwitchboard [[deprecated("use SurfaceProfileRegistry; it does not switch renderers")]] =
    SurfaceProfileRegistry;

// Platform-owned realization seam. The runtime receives a target descriptor
// and lifecycle event; the adapter owns native window/context handles and
// returns a backend-neutral profile for registration.
class RenderTargetProvider {
  public:
    virtual ~RenderTargetProvider() = default;
    [[nodiscard]] virtual RenderTargetInfo describe() const noexcept = 0;
    [[nodiscard]] virtual SurfaceLifecycleDisposition resize(
        SurfaceMetrics metrics) noexcept = 0;
    [[nodiscard]] virtual SurfaceLifecycleDisposition lose() noexcept = 0;
};

struct SurfaceProfile final {
    RenderTargetInfo info;
    std::unique_ptr<SkiaSurfaceProvider> provider;
    SurfaceProfile() = default;
    SurfaceProfile(RenderTargetInfo value, std::unique_ptr<SkiaSurfaceProvider> target);
    ~SurfaceProfile();
    SurfaceProfile(const SurfaceProfile&) = delete;
    SurfaceProfile& operator=(const SurfaceProfile&) = delete;
    SurfaceProfile(SurfaceProfile&&) noexcept;
    SurfaceProfile& operator=(SurfaceProfile&&) noexcept;
};

enum class SurfaceProviderDisposition : std::uint8_t {
    kCommitted,
    kUnknownProfile,
    kDuplicateProfile,
    kInvalidProfile,
    kProviderUnavailable,
    kFormatMismatch,
    kMetricsMismatch,
    kGenerationExhausted,
    kStaleFrame,
};

// Owns the actual provider/profile transaction. Metadata-only profile
// selection remains available through SurfaceProfileRegistry for compatibility;
// new composition roots must use this registry.
class SurfaceProviderRegistry final {
  public:
    SurfaceProviderRegistry(ViewId viewId, SurfaceProfile initial);
    ~SurfaceProviderRegistry();
    SurfaceProviderRegistry(const SurfaceProviderRegistry&) = delete;
    SurfaceProviderRegistry& operator=(const SurfaceProviderRegistry&) = delete;

    [[nodiscard]] SurfaceProviderDisposition registerProfile(SurfaceProfile profile);
    [[nodiscard]] SurfaceProviderDisposition select(
        std::string_view profileId, RenderTargetFormat format);
    [[nodiscard]] SurfaceProviderDisposition resize(SurfaceMetrics metrics);
    [[nodiscard]] SurfaceProviderDisposition markLost() noexcept;
    [[nodiscard]] SurfaceProviderDisposition rebind() noexcept;
    [[nodiscard]] const RenderTargetInfo& activeInfo() const noexcept { return *activeInfo_; }
    [[nodiscard]] SkiaSurfaceProvider* activeProvider() const noexcept { return activeProvider_.get(); }
    [[nodiscard]] const SurfaceLifecycle& lifecycle() const noexcept { return lifecycle_; }
    [[nodiscard]] SurfaceLifecycle& lifecycle() noexcept { return lifecycle_; }

  private:
    struct Entry final {
        RenderTargetInfo info;
        std::unique_ptr<SkiaSurfaceProvider> provider;
    };
    [[nodiscard]] static bool validMetrics(const SurfaceMetrics& metrics) noexcept;
    [[nodiscard]] SurfaceProviderDisposition commit(
        RenderTargetInfo info, std::unique_ptr<SkiaSurfaceProvider>& provider);

    std::vector<Entry> profiles_;
    std::unique_ptr<SkiaSurfaceProvider> activeProvider_;
    std::optional<RenderTargetInfo> activeInfo_;
    std::string activeProfileId_;
    SurfaceLifecycle lifecycle_;
};

} // namespace canvas::render
