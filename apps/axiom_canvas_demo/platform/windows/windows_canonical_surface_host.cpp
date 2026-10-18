#include "windows_canonical_surface_host.hpp"
#include "canvas/input/platform_input_contract.hpp"

#if defined(_WIN32)

#include <windows.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <vector>

namespace canvas::windows_demo {

struct WindowsCanonicalSurfaceHost::Impl {
    HWND window = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool releasingCaptureForPointerUp = false;
};

namespace {

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE || message == WM_DESTROY) {
        DestroyWindow(window);
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_SIZE) {
        auto* host = reinterpret_cast<WindowsCanonicalSurfaceHost*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (host != nullptr && wParam != SIZE_MINIMIZED) {
            std::string ignored;
            (void)host->resizeForWindow(static_cast<std::uint32_t>(LOWORD(lParam)),
                                        static_cast<std::uint32_t>(HIWORD(lParam)),
                                        &ignored);
            InvalidateRect(window, nullptr, FALSE);
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
    if (message == WM_CAPTURECHANGED) {
        auto* host = reinterpret_cast<WindowsCanonicalSurfaceHost*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (host != nullptr && !host->releasingCaptureForPointerUp()) {
            const auto sequence = host->nextInputSequence();
            (void)host->composition().unregisterSource(
                1U, sequence, sequence * 1'000'000U);
            (void)host->composition().cancelAllPointers();
            (void)host->composition().registerSource(1U);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    if (message == WM_LBUTTONDOWN || message == WM_MOUSEMOVE || message == WM_LBUTTONUP) {
        auto* host = reinterpret_cast<WindowsCanonicalSurfaceHost*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (host != nullptr) {
            if (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON) == 0) {
                return DefWindowProcW(window, message, wParam, lParam);
            }
            const auto phase = message == WM_LBUTTONDOWN ? canvas::input::PointerPhase::kDown
                : message == WM_LBUTTONUP ? canvas::input::PointerPhase::kUp
                                           : canvas::input::PointerPhase::kMove;
            if (phase == canvas::input::PointerPhase::kDown) SetCapture(window);
            canvas::input::PlatformPointerBatch batch;
            const auto sequence = host->nextInputSequence();
            batch.samples.push_back({1U, 1U, sequence,
                sequence * 1'000'000U,
                static_cast<float>(static_cast<short>(LOWORD(lParam))),
                static_cast<float>(static_cast<short>(HIWORD(lParam))), 0.5F, 0.0F, 0.0F,
                {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, phase});
            (void)host->composition().submit(batch);
            runtime::RuntimeFrameOpportunity opportunity{};
            (void)host->composition().serviceFrameOpportunity(true, opportunity);
            if (phase == canvas::input::PointerPhase::kUp) {
                host->beginPointerUpCaptureRelease();
                ReleaseCapture();
                host->endPointerUpCaptureRelease();
            }
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(window, &paint);
        auto* host = reinterpret_cast<WindowsCanonicalSurfaceHost*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (host != nullptr) {
            std::string ignored;
            (void)host->paintCanonical(dc, &ignored);
        }
        EndPaint(window, &paint);
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
    if (!composition_.open()) { if (error) *error = "Runtime composition open failed"; return false; }
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
    SetWindowLongPtrW(impl_->window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    ShowWindow(impl_->window, SW_SHOWNOACTIVATE);
    render::SurfaceMetrics metrics{static_cast<float>(width), static_cast<float>(height),
                                   width, height, 1.0F, 1.0F};
    surface_ = new WindowsSurfaceAdapter(render::SurfaceSnapshot{
        render::ViewId{1}, render::SurfaceGeneration{1},
        render::MetricsGeneration{1}, metrics});
    observation_.surfaceGeneration = 1;
    observation_.metricsGeneration = 1;
    if (!composition_.bindSurface(width, height) || !composition_.registerSource(1U)) { if (error) *error = "Runtime composition surface bind failed"; return false; }
    const auto bound = composition_.surfaceSnapshot();
    observation_.surfaceGeneration = bound.surfaceGeneration;
    observation_.metricsGeneration = bound.metricsGeneration;
    (void)composition_.setPlatformPresentationDeferred(true);
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
    if (composition_.surfaceSnapshot().width != width ||
        composition_.surfaceSnapshot().height != height) {
        return resizeForWindow(width, height, error);
    }
    return true;
}

bool WindowsCanonicalSurfaceHost::resizeForWindow(std::uint32_t width,
                                                  std::uint32_t height,
                                                  std::string* error) {
    if (impl_ == nullptr || surface_ == nullptr || error == nullptr ||
        width == 0U || height == 0U) {
        if (error != nullptr) *error = "Windows surface is not resizeable";
        return false;
    }
    if (impl_->width == width && impl_->height == height &&
        composition_.surfaceSnapshot().width == width &&
        composition_.surfaceSnapshot().height == height) return true;
    if (!composition_.resizeSurface(width, height)) {
        *error = "Runtime composition resize did not produce a fresh surface";
        return false;
    }
    const auto live = composition_.surfaceSnapshot();
    const auto current = surface_->lifecycle().current();
    const auto replacement = render::SurfaceSnapshot{
        current.viewId, render::SurfaceGeneration{live.surfaceGeneration},
        render::MetricsGeneration{live.metricsGeneration},
        render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                               width, height, 1.0F, 1.0F}};
    if (surface_->bind(replacement) != WindowsSurfaceDisposition::kRebound) {
        *error = "Windows surface resize did not produce a fresh generation";
        return false;
    }
    impl_->width = width;
    impl_->height = height;
    ++observation_.resizeEvents;
    observation_.surfaceGeneration = live.surfaceGeneration;
    observation_.metricsGeneration = live.metricsGeneration;
    runtime::RuntimeFrameOpportunity opportunity{};
    if (!composition_.serviceFrameOpportunity(true, opportunity) ||
        !opportunity.canonicalRequested) {
        *error = "Runtime composition resize did not request a canonical redraw";
        return false;
    }
    InvalidateRect(impl_->window, nullptr, FALSE);
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

bool WindowsCanonicalSurfaceHost::paintCanonical(HDC dc, std::string* error) {
    if (dc == nullptr || error == nullptr) {
        if (error != nullptr) *error = "Windows paint target is unavailable";
        return false;
    }
    if (!composition_.canonicalProviderAvailable()) { *error = "Runtime canonical provider is unavailable"; return false; }
    const auto surfaceInfo = composition_.surfaceSnapshot();
    interaction::PreviewPresentationCapture previewCapture{};
    std::uint64_t previewPresentCount = 0;
    const bool hadPreview = composition_.previewActive();
    const bool havePreview = hadPreview &&
        ((composition_.capturePreviewPresentation(previewCapture) &&
          composition_.renderPreviewPresentation(previewCapture, &previewPresentCount)) ||
         composition_.previewProviderAvailable());
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(surfaceInfo.width) *
                                   surfaceInfo.height * 4U);
    if (!composition_.readCanonicalRgba(rgba)) {
        *error = "Runtime canonical readback failed";
        return false;
    }
    if (havePreview) {
        if (composition_.previewProviderAvailable()) {
            std::vector<std::uint8_t> overlay(rgba.size());
            if (composition_.readPreviewRgba(overlay)) {
                for (std::size_t i = 0; i + 3U < rgba.size(); i += 4U) {
                    const float alpha = static_cast<float>(overlay[i + 3U]) / 255.0F;
                    if (alpha <= 0.0F) continue;
                    for (std::size_t c = 0; c < 3U; ++c) {
                        rgba[i + c] = static_cast<std::uint8_t>(
                            static_cast<float>(overlay[i + c]) * alpha +
                            static_cast<float>(rgba[i + c]) * (1.0F - alpha));
                    }
                }
            }
        }
    }
    for (std::size_t i = 0; i + 3U < rgba.size(); i += 4U) std::swap(rgba[i], rgba[i + 2U]);
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = static_cast<LONG>(surfaceInfo.width);
    bitmap.bmiHeader.biHeight = -static_cast<LONG>(surfaceInfo.height);
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    bitmap.bmiHeader.biCompression = BI_RGB;
    const int width = static_cast<int>(surfaceInfo.width);
    const int height = static_cast<int>(surfaceInfo.height);
    if (StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height,
                      rgba.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
        *error = "Windows canonical DIB presentation failed";
        return false;
    }
    if (const auto frame = composition_.lastSubmittedCanonicalFrame(); frame.has_value() &&
        frame->frameId.value() != lastPresentedFrame_) {
        if (!composition_.receivePlatformPresented({
                frame->viewId, frame->frameId, frame->surfaceGeneration,
                frame->metricsGeneration, render::PresentOutcome::kPresented,
                render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) {
            *error = "Windows canonical feedback rejected";
            return false;
        }
        lastPresentedFrame_ = frame->frameId.value();
    }
    if (havePreview && previewCapture.contentRevision != 0U &&
        composition_.previewActive() &&
        !composition_.acknowledgePreviewPresentation(
            previewCapture, previewPresentCount)) {
        *error = "Windows preview acknowledgement rejected";
        return false;
    }
    if (hadPreview && !composition_.previewActive()) {
        std::vector<std::uint8_t> canonical(rgba.size());
        if (!composition_.readCanonicalRgba(canonical)) {
            *error = "Windows canonical redraw after preview retirement failed";
            return false;
        }
        for (std::size_t i = 0; i + 3U < canonical.size(); i += 4U) {
            std::swap(canonical[i], canonical[i + 2U]);
        }
        if (StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height,
                          canonical.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY) == GDI_ERROR) {
            *error = "Windows canonical redraw after preview retirement failed";
            return false;
        }
    }
    return true;
}

bool WindowsCanonicalSurfaceHost::releasingCaptureForPointerUp() const noexcept {
    return impl_ != nullptr && impl_->releasingCaptureForPointerUp;
}

void WindowsCanonicalSurfaceHost::beginPointerUpCaptureRelease() noexcept {
    if (impl_ != nullptr) impl_->releasingCaptureForPointerUp = true;
}

void WindowsCanonicalSurfaceHost::endPointerUpCaptureRelease() noexcept {
    if (impl_ != nullptr) impl_->releasingCaptureForPointerUp = false;
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
bool WindowsCanonicalSurfaceHost::resizeForWindow(std::uint32_t,
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
bool WindowsCanonicalSurfaceHost::paintCanonical(void*, std::string* error) {
    if (error != nullptr) *error = "Windows host is only available on Windows";
    return false;
}

} // namespace canvas::windows_demo

#endif
