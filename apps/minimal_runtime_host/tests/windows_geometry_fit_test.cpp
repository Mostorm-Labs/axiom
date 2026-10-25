#include "minimal_runtime_facade.hpp"
#include "windows_d3d12_surface_provider.hpp"
#include "host_font_resources.hpp"
#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string_view>

namespace {
using namespace canvas;
struct HostFixture {
    minimal_runtime_host::MinimalRuntimeHost host;
    minimal_runtime_host::MinimalRuntimeFacade facade{host};
    std::unique_ptr<debug_ui::DebugController> controller;
    std::uint64_t sequence=0,request=0;
    bool initialize(HWND window) {
        auto canonical=std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(window,true);
        auto preview=std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(window,false);
        if(canonical->resize(800,600).code!=render::BackendSubmissionCode::kAccepted ||
           preview->resize(800,600).code!=render::BackendSubmissionCode::kAccepted) return false;
        auto& c=host.composition();
        assert(c.registerSurfaceProvider("windows-d3d12-canonical",std::move(canonical)));
        assert(c.registerPreviewSurfaceProvider("windows-d3d12-preview",std::move(preview)));
        assert(c.bindSurface(800,600) && c.setPlatformPresentationDeferred(true));
        assert(host.registerSource(1));
        std::array<wchar_t,32768> module{};
        const auto length=GetModuleFileNameW(nullptr,module.data(),static_cast<DWORD>(module.size()));
        assert(length>0 && length<module.size());
        const auto root=std::filesystem::path(module.data()).parent_path();
        assert(minimal_runtime_host::bindHostTextFonts(c,root/L"axiom-font.otf",root/L"fonts"));
        debug_ui::DebugControllerContext context{};
        context.runtime=&facade;context.platformDiagnostics=&facade;
        controller=std::make_unique<debug_ui::DebugController>(context);
        controller->uiState().setWorkspace(debug_ui::WorkspaceId::kControl);
        return true;
    }
    void control(runtime::CanvasControlPayload payload) {
        runtime::CanvasControlRequest command{};
        command.clientId=0xFC05D3D;command.requestId=++request;
        command.target=facade.mountedCanvasTargets().front();command.payload=payload;
        assert(facade.submitCanvasControl(command).state==runtime::ProductControlState::kQueued);
        assert(host.composition().ownerPump());
        assert(facade.canvasControlReceipt({command.clientId,command.requestId})->state==runtime::CanvasControlReceiptState::kApplied);
    }
    void tool(runtime::CanvasToolKind value) {
        runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kSelectTool;p.tool=value;control(p);
    }
    void preset(runtime::CanvasCreationPreset value) {
        runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;p.creationPreset=value;control(p);
    }
    void pointer(input::PointerPhase phase,float x,float y) {
        input::PlatformPointerBatch batch;++sequence;
        batch.samples.push_back({1,1,sequence,sequence*1000000,x,y,0,0,0,{}, {},
            input::SampleProvenance::kConfirmedCurrent,phase});
        assert(host.submit(batch));
    }
    void tap(float x,float y) {pointer(input::PointerPhase::kDown,x,y);pointer(input::PointerPhase::kUp,x,y);}
    void frame() {
        auto& c=host.composition();
        for(unsigned retry=0;retry<32;++retry) {
            runtime::RuntimeFrameOpportunity work{};
            assert(c.serviceFrameOpportunity(true,work) && "Windows D3D12 rejected the production frame");
            if(!work.canonicalWork) {Sleep(1);continue;}
            // Same accepted-work feedback used by the native host. This
            // fixture proves submission, not physical scanout/PHYS acceptance.
            assert(c.receivePlatformPresented({*work.canonicalWork,render::PresentOutcome::kPresented,
                render::PresentEvidenceKind::kPlatformQualified,{}}));
            if(work.previewWork) assert(c.finishFrameOpportunity({work.previewWork,true}));
            return;
        }
        assert(false && "Windows D3D12 frame did not progress within retry budget");
    }
    void uiFrame() {ImGui::NewFrame();(void)controller->buildImGuiFrame();ImGui::Render();}
    void fitWidget() {
        for(unsigned i=0;i<3;++i)uiFrame();
        ImGuiWindow* content=nullptr;
        for(auto* window:ImGui::GetCurrentContext()->Windows)
            if(std::string_view(window->Name).find("/##content_")!=std::string_view::npos)content=window;
        assert(content);
        ImGui::NewFrame();auto& gui=*ImGui::GetCurrentContext();
        gui.NavActivateId=gui.NavActivateDownId=content->GetID("Fit Content");
        gui.NavInputSource=ImGuiInputSource_Keyboard;
        assert(controller->buildImGuiFrame());ImGui::Render();frame();
    }
};
void fractionalSticky(HostFixture& h) {
    h.preset(runtime::CanvasCreationPreset::kSticky);
    h.pointer(input::PointerPhase::kDown,80.125F,100.25F);
    h.pointer(input::PointerPhase::kMove,104.875F,124.875F);
    h.pointer(input::PointerPhase::kUp,104.875F,124.875F);
    assert(h.host.composition().semanticObjectCount()==1);
    h.frame();
    h.tool(runtime::CanvasToolKind::kSelect);h.tap(92.5F,112.5F);
    const auto sticky=h.facade.readSelectedSticky();
    assert(sticky.available && std::abs(sticky.content.width-24.75)<0.001 &&
        sticky.content.width==sticky.content.height);
    h.fitWidget();h.frame();
}
void fitAfterAttachment(HostFixture& h) {
    h.tool(runtime::CanvasToolKind::kInk);
    h.pointer(input::PointerPhase::kDown,600,100);h.pointer(input::PointerPhase::kMove,700,180);
    h.pointer(input::PointerPhase::kUp,700,180);h.frame();
    h.preset(runtime::CanvasCreationPreset::kRectangle);
    h.pointer(input::PointerPhase::kDown,600,300);h.pointer(input::PointerPhase::kUp,700,400);h.frame();
    h.tool(runtime::CanvasToolKind::kSelect);h.tap(650,350);h.tap(728,350);h.frame();
    assert(h.host.composition().semanticObjectCount()==4);
    h.fitWidget();
    const auto fit=h.facade.readRuntimeState().camera;
    for(const auto point:std::array<std::array<float,2>,4>{{{600,100},{700,180},{600,300},{700,400}}}) {
        const auto x=point[0]*fit.scale+fit.translationX,y=point[1]*fit.scale+fit.translationY;
        assert(x>=30 && x<=770 && y>=30 && y<=570 && "Windows Fit lost earlier content after attachment creation");
    }
    h.tool(runtime::CanvasToolKind::kInk);h.frame();
    const auto after=h.facade.readRuntimeState().camera;
    assert(after.scale==fit.scale && after.translationX==fit.translationX && after.translationY==fit.translationY);
}
} // namespace
int main(int argc,char** argv) {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    WNDCLASSW klass{};klass.hInstance=GetModuleHandleW(nullptr);klass.lpfnWndProc=DefWindowProcW;
    klass.lpszClassName=L"AxiomGeometryFitRegression";assert(RegisterClassW(&klass));
    const auto window=CreateWindowExW(0,klass.lpszClassName,L"Geometry regression",WS_OVERLAPPEDWINDOW,
        0,0,820,640,nullptr,nullptr,klass.hInstance,nullptr);assert(window);
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,1400};io.DeltaTime=1.0F/60;
    unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    int result=0;
    {
        HostFixture host;
        if(!host.initialize(window)) {std::fputs("BLOCKED_ENVIRONMENT: Windows D3D12 unavailable\n",stderr);result=77;}
        else {
            if(argc>1 && std::string_view(argv[1])=="sticky")fractionalSticky(host);
            else fitAfterAttachment(host);
            assert(host.host.composition().close());
            std::puts("PASS: Windows D3D12 / real fonts / Debug UI; synthetic pointer facts, no physical input claim");
        }
    }
    ImGui::DestroyContext();assert(DestroyWindow(window));assert(UnregisterClassW(klass.lpszClassName,klass.hInstance));
    return result;
}
