#include "windows_canonical_surface_host.hpp"

#if defined(_WIN32)

#include <windows.h>

#include <memory>
#include <sstream>

namespace canvas::windows_demo {

struct WindowsCanonicalSurfaceHost::Impl {
    HWND window = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

namespace {

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE || message == WM_DESTROY) {
        DestroyWindow(window);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

WindowsCanonicalSurfaceHost::WindowsCanonicalSurfaceHost() noexcept = default;

WindowsCanonicalSurfaceHost::~WindowsCanonicalSurfaceHost() {
    if (impl_ != nullptr && impl_->window != nullptr) {
        DestroyWindow(impl_->window);
    }
    delete surface_;
    delete impl_;
}

bool WindowsCanonicalSurfaceHost::initialize(std::uint32_t width,
                                             std::uint32_t height,
                                             std::string* error) {
    if (width == 0U || height == 0U || error == nullptr) {
        if (error != nullptr) *error = "invalid Windows surface dimensions";
        return false;
    }
    impl_ = new Impl{};
    impl_->width = width;
    impl_->height = height;
    WNDCLASSW klass{};
    klass.hInstance = GetModuleHandleW(nullptr);
    klass.lpfnWndProc = WindowProc;
    klass.lpszClassName = L"AxiomG309CanonicalSurface";
    if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        *error = "RegisterClassW failed";
        return false;
    }
    impl_->window = CreateWindowExW(0, klass.lpszClassName,
                                    L"Axiom G3-09 Canonical Surface",
                                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                    CW_USEDEFAULT, static_cast<int>(width),
                                    static_cast<int>(height), nullptr, nullptr,
                                    klass.hInstance, nullptr);
    if (impl_->window == nullptr) {
        *error = "CreateWindowExW failed with error " +
                 std::to_string(static_cast<unsigned long>(GetLastError()));
        return false;
    }
    ShowWindow(impl_->window, SW_SHOWNOACTIVATE);
    render::SurfaceMetrics metrics{static_cast<float>(width), static_cast<float>(height),
                                   width, height, 1.0F, 1.0F};
    surface_ = new WindowsSurfaceAdapter(render::SurfaceSnapshot{
        render::ViewId{1}, render::SurfaceGeneration{1},
        render::MetricsGeneration{1}, metrics});
    observation_.surfaceGeneration = 1;
    observation_.metricsGeneration = 1;
    return true;
}

bool WindowsCanonicalSurfaceHost::pumpOnce(std::string* error) {
    if (impl_ == nullptr || impl_->window == nullptr || error == nullptr) {
        if (error != nullptr) *error = "Windows surface is not initialized";
        return false;
    }
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        if (message.message == WM_QUIT) return false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

bool WindowsCanonicalSurfaceHost::resizeForSmoke(std::uint32_t width,
                                                 std::uint32_t height,
                                                 std::string* error) {
    if (impl_ == nullptr || impl_->window == nullptr || surface_ == nullptr ||
        error == nullptr || width == 0U || height == 0U) {
        if (error != nullptr) *error = "Windows surface is not resizeable";
        return false;
    }
    if (SetWindowPos(impl_->window, nullptr, 0, 0, static_cast<int>(width),
                     static_cast<int>(height),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) == 0) {
        *error = "SetWindowPos failed";
        return false;
    }
    const auto current = surface_->lifecycle().current();
    const auto replacement = render::SurfaceSnapshot{
        current.viewId,
        render::SurfaceGeneration{current.surfaceGeneration.value() + 1U},
        render::MetricsGeneration{current.metricsGeneration.value() + 1U},
        render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                               width, height, 1.0F, 1.0F}};
    if (surface_->bind(replacement) != WindowsSurfaceDisposition::kRebound) {
        *error = "Windows surface resize did not produce a fresh generation";
        return false;
    }
    impl_->width = width;
    impl_->height = height;
    ++observation_.resizeEvents;
    observation_.surfaceGeneration = replacement.surfaceGeneration.value();
    observation_.metricsGeneration = replacement.metricsGeneration.value();
    return true;
}

bool WindowsCanonicalSurfaceHost::submit(render::IRenderBackend& backend,
                                         const render::FramePlan& plan,
                                         std::string* error) {
    if (surface_ == nullptr || error == nullptr) {
        if (error != nullptr) *error = "Windows surface is not initialized";
        return false;
    }
    const auto result = render::FrameOrchestrator::submit(backend, plan);
    if (result.code != render::BackendSubmissionCode::kAccepted) {
        *error = result.message;
        return false;
    }
    const auto disposition = surface_->submit(plan.frame);
    if (disposition != render::PresentFeedbackDisposition::kSubmitted) {
        *error = "Render Core rejected the generation-bound Windows frame";
        return false;
    }
    ++observation_.submittedFrames;
    const auto feedback = surface_->feedback(render::PresentedFeedback{
        plan.frame.viewId, plan.frame.frameId, plan.frame.surfaceGeneration,
        plan.frame.metricsGeneration, render::PresentOutcome::kPresented,
        render::PresentEvidenceKind::kPlatformQualified, std::nullopt});
    if (feedback != render::PresentFeedbackDisposition::kPresented) {
        *error = "Windows present feedback was rejected";
        return false;
    }
    ++observation_.presentedFrames;
    return true;
}

} // namespace canvas::windows_demo

#else

namespace canvas::windows_demo {

struct WindowsCanonicalSurfaceHost::Impl {};
WindowsCanonicalSurfaceHost::WindowsCanonicalSurfaceHost() noexcept = default;
WindowsCanonicalSurfaceHost::~WindowsCanonicalSurfaceHost() = default;
bool WindowsCanonicalSurfaceHost::initialize(std::uint32_t,
                                             std::uint32_t,
                                             std::string* error) {
    if (error != nullptr) *error = "Windows host is only available on Windows";
    return false;
}
bool WindowsCanonicalSurfaceHost::pumpOnce(std::string* error) {
    if (error != nullptr) *error = "Windows host is only available on Windows";
    return false;
}

bool WindowsCanonicalSurfaceHost::resizeForSmoke(std::uint32_t,
                                                 std::uint32_t,
                                                 std::string* error) {
    if (error != nullptr) *error = "Windows host is only available on Windows";
    return false;
}
bool WindowsCanonicalSurfaceHost::submit(render::IRenderBackend&,
                                         const render::FramePlan&,
                                         std::string* error) {
    if (error != nullptr) *error = "Windows host is only available on Windows";
    return false;
}

} // namespace canvas::windows_demo

#endif
