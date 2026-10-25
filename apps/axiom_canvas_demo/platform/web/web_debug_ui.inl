// Optional platform sibling. All controls borrow the composition-owned facade.
#include "canvas/debug_ui/controller.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "imgui.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"

namespace {
class WebDebugUi final : public canvas::runtime::PlatformDiagnostics {
    static constexpr int kPanelWidth = 410;
    static constexpr int kPanelHeight = 650;
    canvas::runtime::RuntimeComposition& composition_;
    std::unique_ptr<canvas::debug_ui::DebugController> controller_;
    std::unique_ptr<canvas::render::WebGlSurfaceProvider> provider_;
    ImGuiContext* context_ = nullptr;
    sk_sp<SkImage> font_;
    bool visible_ = false;

  public:
    explicit WebDebugUi(canvas::runtime::RuntimeComposition& composition)
        : composition_(composition) {}
    ~WebDebugUi() {
        if (context_) {
            ImGui::SetCurrentContext(context_);
            ImGui::DestroyContext(context_);
        }
    }
    bool initialize() {
        provider_ = std::make_unique<canvas::render::WebGlSurfaceProvider>(
            canvas::render::WebGlSurfaceConfig{"#debugUi", kPanelWidth, kPanelHeight});
        if (!provider_->ready())
            return false;
        canvas::debug_ui::DebugControllerContext controls{};
        controls.runtime = &composition_.controlFacade();
        controls.platformDiagnostics = this;
        controller_ = std::make_unique<canvas::debug_ui::DebugController>(std::move(controls));
        IMGUI_CHECKVERSION();
        context_ = ImGui::CreateContext();
        if (!context_)
            return false;
        ImGui::SetCurrentContext(context_);
        ImGui::StyleColorsDark();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(kPanelWidth, kPanelHeight);
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
        if (!pixels)
            return false;
        const auto info = SkImageInfo::MakeA8(width, height);
        font_ = SkImages::RasterFromPixmapCopy(SkPixmap(info, pixels, info.minRowBytes()));
        return font_ != nullptr;
    }
    bool render() {
        if (!visible_)
            return true;
        ImGui::SetCurrentContext(context_);
        auto& io = ImGui::GetIO();
        io.DeltaTime = 1.0F / 60.0F;
        ImGui::NewFrame();
        (void)controller_->buildImGuiFrame();
        ImGui::Render();
        const auto acquired = provider_->acquire();
        if (acquired.code != canvas::render::SkiaSurfaceAcquireCode::kAcquired)
            return false;
        const bool rendered = canvas::debug_ui::ImGuiSkiaRenderer{}.render(
            ImGui::GetDrawData(), acquired.frame.surface, font_.get());
        provider_->release();
        return rendered &&
               provider_->present().code == canvas::render::BackendSubmissionCode::kAccepted;
    }
    bool input(int phase, float x, float y, int button) {
        if (phase == 4) {
            visible_ = !visible_;
            return render();
        }
        if (phase == 5)
            return render();
        if (!visible_)
            return false;
        ImGui::SetCurrentContext(context_);
        auto& io = ImGui::GetIO();
        if (phase == 9) {
            io.AddKeyEvent(ImGuiMod_Ctrl, (button & 1) != 0);
            io.AddKeyEvent(ImGuiMod_Shift, (button & 2) != 0);
            io.AddKeyEvent(ImGuiMod_Alt, (button & 4) != 0);
            io.AddKeyEvent(ImGuiMod_Super, (button & 8) != 0);
        } else if (phase == 6 || phase == 7) {
            io.AddKeyEvent(static_cast<ImGuiKey>(button), phase == 6);
        } else if (phase == 8) {
            io.AddMouseWheelEvent(x, y);
        } else {
            io.AddMousePosEvent(x, y);
            if (phase == 0 || phase == 2 || phase == 3)
                io.AddMouseButtonEvent(button >= 0 && button < 5 ? button : 0, phase == 0);
        }
        return render();
    }
    bool text(const char* value) {
        if (!visible_ || !value)
            return false;
        ImGui::SetCurrentContext(context_);
        ImGui::GetIO().AddInputCharactersUTF8(value);
        return render();
    }
    canvas::runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override {
        canvas::runtime::PlatformDiagnosticsSnapshot out{};
        const auto s = composition_.surfaceSnapshot();
        out.canonicalSurfaceGeneration = s.surfaceGeneration;
        out.previewSurfaceGeneration = composition_.previewSurfaceGeneration();
        out.width = s.width;
        out.height = s.height;
        out.devicePixelRatio = s.devicePixelRatio;
        out.presentCount = composition_.canonicalPresentCount();
        out.surfaceAvailable = s.available;
        out.canonicalSurfaceMode = canvas::runtime::SurfaceMode::kGpuDefault;
        return out;
    }
};
} // namespace
