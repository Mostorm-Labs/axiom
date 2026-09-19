#pragma once

#include "windows_surface_adapter.hpp"

#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"

#include <cstdint>
#include <string>

namespace canvas::windows_demo {

struct WindowsSmokeObservation final {
    std::uint32_t resizeEvents = 0;
    std::uint32_t submittedFrames = 0;
    std::uint32_t presentedFrames = 0;
    std::uint64_t surfaceGeneration = 0;
    std::uint64_t metricsGeneration = 0;
    bool gdiLivePresentUsed = false;
};

// Windows-only native host boundary. The host owns HWND/surface observations;
// Render Core owns frame construction and backend policy.
class WindowsCanonicalSurfaceHost final {
  public:
    WindowsCanonicalSurfaceHost() noexcept;
    ~WindowsCanonicalSurfaceHost();

    WindowsCanonicalSurfaceHost(const WindowsCanonicalSurfaceHost&) = delete;
    WindowsCanonicalSurfaceHost& operator=(const WindowsCanonicalSurfaceHost&) = delete;

    [[nodiscard]] bool initialize(std::uint32_t width,
                                  std::uint32_t height,
                                  std::string* error);
    [[nodiscard]] bool pumpOnce(std::string* error);
    [[nodiscard]] bool resizeForSmoke(std::uint32_t width,
                                      std::uint32_t height,
                                      std::string* error);
    [[nodiscard]] bool submit(render::IRenderBackend& backend,
                               const render::FramePlan& plan,
                               std::string* error);
    [[nodiscard]] const WindowsSmokeObservation& observation() const noexcept {
        return observation_;
    }

  private:
    struct Impl;
    Impl* impl_ = nullptr;
    WindowsSmokeObservation observation_{};
    WindowsSurfaceAdapter* surface_ = nullptr;
};

} // namespace canvas::windows_demo
