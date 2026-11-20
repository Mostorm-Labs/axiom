#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include <objc/message.h>
#include <objc/runtime.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

extern "C" id MTLCreateSystemDefaultDevice(void);
namespace canvas::render {
struct HidpiPreviewQualification {
    static auto budget(SkiaRenderer& r) { return r.resourceBudget_; }
    static bool cache(const SkiaRenderer& r) { return bool(r.compactCache_) || bool(r.legacyCompactCache_); }
    static bool pending(const SkiaRenderer& r) { return r.compactReleasePending(); }
};
}
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
public:
    static auto& preview(interaction::CanvasEditingRuntime& r) { return r.previewRenderer_; }
    static auto& canonical(interaction::CanvasEditingRuntime& r) { return r.skiaRenderer_; }
};
}
using namespace canvas;
using namespace canvas::render;
using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
using json = nlohmann::json;
namespace {
void require(bool v, const char* m) { if (!v) throw std::runtime_error(m); }
id message(id target, const char* name) {
    return reinterpret_cast<id(*)(id, SEL)>(objc_msgSend)(target, sel_registerName(name));
}
struct Completion final : GpuCompletionService {
    sk_sp<GrDirectContext> context;
    std::shared_ptr<GpuCompletionState> flags = std::make_shared<GpuCompletionState>();
    Completion() {
        auto device = MTLCreateSystemDefaultDevice();
        auto queue = message(device, "newCommandQueue");
        GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device), sk_cfp<GrMTLHandle>(queue)};
        context = GrDirectContexts::MakeMetal(backend);
        require(bool(context), "Metal context unavailable");
    }
    ~Completion() override { context->flushAndSubmit(GrSyncCpu::kYes); flags->abandoned = true; }
    void service() noexcept override { context->checkAsyncWorkCompletion(); }
    auto state() const noexcept -> std::shared_ptr<const GpuCompletionState> override { return flags; }
    void drain() { context->flushAndSubmit(GrSyncCpu::kYes); service(); }
};
class Target final : public SkiaSurfaceProvider {
    sk_sp<SkSurface> surface_;
    std::uint64_t generation_ = 1, presents_ = 0;
    bool visible_ = false;
public:
    std::shared_ptr<Completion> completion = std::make_shared<Completion>();
    bool unavailable = false;
    bool lost = false, logicallyLost = false;
    std::function<void()> beforeResize;
    std::function<void()> beforeDestroy;
    ~Target() override { if (beforeDestroy) beforeDestroy(); }
    RenderTargetInfo describe() const noexcept override {
        RenderTargetCapabilities caps{};
        caps.gpuAccelerated = caps.supportsPresent = true;
        return {"p36-metal", RenderTargetKind::kGpuOffscreen, RenderTargetBackend::kMetal,
            RenderTargetFormat::kRgba8888,
            {320, 200, unsigned(surface_->width()), unsigned(surface_->height()),
             float(surface_->width()) / 320, 1}, caps};
    }
    auto gpuCompletionService() const noexcept -> std::shared_ptr<GpuCompletionService> override { return completion; }
    SkiaSurfaceAcquireResult acquire() noexcept override {
        if (lost || logicallyLost) return SkiaSurfaceAcquireResult::rejected(SkiaSurfaceAcquireCode::kLost, "lost target");
        if (unavailable) return SkiaSurfaceAcquireResult::rejected(SkiaSurfaceAcquireCode::kUnavailable, "withheld target");
        return SkiaSurfaceAcquireResult::acquired({surface_.get(), generation_});
    }
    void release() noexcept override {}
    BackendSubmissionResult present() noexcept override {
        completion->context->flushAndSubmit(GrSyncCpu::kNo); ++presents_;
        return BackendSubmissionResult::accepted();
    }
    BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override {
        if (beforeResize) beforeResize();
        surface_ = SkSurfaces::RenderTarget(completion->context.get(), skgpu::Budgeted::kNo, SkImageInfo::MakeN32Premul(w, h));
        return surface_ ? BackendSubmissionResult::accepted() : BackendSubmissionResult::rejected("target allocation");
    }
    BackendSubmissionResult lose() noexcept override { logicallyLost=true; return BackendSubmissionResult::accepted(); }
    void physicalLoss() {
        lost=true; completion->flags->physicalLost=true; completion->flags->abandoned=true;
        completion->context->abandonContext();
    }
    BackendSubmissionResult advanceGeneration() noexcept override { ++generation_; logicallyLost=false; return BackendSubmissionResult::accepted(); }
    BackendSubmissionResult readbackRgba(std::span<std::uint8_t>) noexcept override { return BackendSubmissionResult::rejected("not used"); }
    std::uint64_t readbackCount() const noexcept override { return 0; }
    std::uint64_t cpuCopyCount() const noexcept override { return 0; }
    std::uint64_t presentCount() const noexcept override { return presents_; }
    std::uint64_t generation() const noexcept override { return generation_; }
    void setOverlayVisible(bool v) noexcept override { visible_ = v; }
    bool overlayVisible() const noexcept override { return visible_; }
};
struct Host {
    runtime::RuntimeCompositionCore core;
    Target *canonical, *preview;
    unsigned dpr;
    std::uint64_t sequence = 0;
    Host(const char* profile, unsigned scale) : dpr(scale) {
        require(core.open(), "core open");
        auto c = std::make_unique<Target>(), p = std::make_unique<Target>();
        canonical = c.get(); preview = p.get();
        require(canonical->completion->context != preview->completion->context, "fixture contexts must differ");
        require(c->resize(320*dpr, 200*dpr).code == BackendSubmissionCode::kAccepted &&
                p->resize(320*dpr, 200*dpr).code == BackendSubmissionCode::kAccepted, "fixture targets");
        require(core.registerSurfaceProvider("p36-canonical", std::move(c)) &&
                core.registerPreviewSurfaceProvider("p36-preview", std::move(p)) &&
                core.bindSurface({320, 200, 320*dpr, 200*dpr, float(dpr), 1}) &&
                core.registerSource(71) && core.setPlatformPresentationDeferred(true), "fixture bind");
        require(core.qualificationRuntime().selectBrushProfile(profile, 3), "brush selection");
    }
    auto& renderer() { return Access::preview(core.qualificationRuntime()); }
    auto& actual() { return core.qualificationRuntime(); }
    bool pointer(input::PointerPhase phase, unsigned contact = 1, unsigned n = 0) {
        input::PlatformPointerSample p{}; p.source=71; p.pointer=contact; p.sequence=++sequence;
        p.timestampNs=sequence*16666667; p.x=30+float(n)*1.5F; p.y=85+20*std::sin(n*.1);
        p.pressure=.5; p.phase=phase; return core.submit({{p}});
    }
    runtime::RuntimeFrameOpportunity frame(bool acknowledge = true) {
        runtime::RuntimeFrameOpportunity f;
        require(core.serviceFrameOpportunity(false, f), "actual frame opportunity");
        if (acknowledge) {
            preview->completion->drain(); canonical->completion->drain();
            if (f.previewWork) require(core.finishFrameOpportunity({f.previewWork, true}), "preview receipt");
            if (f.canonicalWork) require(core.receivePlatformPresented({*f.canonicalWork,
                PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified, {}}), "canonical receipt");
        }
        return f;
    }
    void draw(unsigned contact = 1) {
        require(pointer(input::PointerPhase::kDown, contact), "actual down");
        for (unsigned n=1; n<=100; ++n) {
            require(pointer(input::PointerPhase::kMove, contact, n), "actual move");
            if (n%10==0) frame();
        }
        auto budget = HidpiPreviewQualification::budget(renderer());
        require(budget && budget->used(resources::ResourceClass::kRaster)>0 &&
                budget->used(resources::ResourceClass::kScratch)>0, "actual deferred renderer never allocated preview cache");
    }
    void drain() {
        for (unsigned i=0; i<8; ++i) { preview->completion->drain(); canonical->completion->drain(); frame(); }
    }
    void empty(const char* message) {
        auto b=HidpiPreviewQualification::budget(renderer());
        require(!HidpiPreviewQualification::cache(renderer()), message);
        require(!b || (b->used(resources::ResourceClass::kRaster)==0 && b->used(resources::ResourceClass::kScratch)==0), message);
    }
};
void cancel(const char* profile, unsigned dpr, json& rows) {
    Host h(profile, dpr); h.draw();
    const auto before = h.core.readback().semanticGeneration;
    auto budget = HidpiPreviewQualification::budget(h.renderer());
    require(h.pointer(input::PointerPhase::kCancel), "actual cancel");
    h.drain();
    const auto retained = budget->used(resources::ResourceClass::kRaster);
    const auto scratch = budget->used(resources::ResourceClass::kScratch);
    json row{{"case","deferred-cancel"},{"profile",profile},{"dpr",dpr},
        {"stableLeaseBytesAfterIdle",retained},{"scratchLeaseBytesAfterIdle",scratch},
        {"actualPreviewRenderer",true},{"noNewInputAfterCancel",true}};
    rows.push_back(row); std::cout << row.dump() << std::endl;
    require(!h.core.previewActive() && !h.preview->overlayVisible(), "cancel leaves preview visible");
    require(h.core.readback().semanticGeneration==before, "cancel mutates document");
    require(retained==0 && scratch==0, "HD-L1 actual deferred renderer retains inactive stable/scratch leases");
}
void handoff(const char* profile, unsigned dpr, bool canonicalFirst, json& rows) {
    Host h(profile,dpr);h.draw();
    require(h.pointer(input::PointerPhase::kMove,1,101),"last move");
    auto previous=h.frame(false);require(previous.previewWork.has_value(),"last preview work");
    require(h.pointer(input::PointerPhase::kUp,1,101),"actual Up");
    require(h.core.previewActive() && HidpiPreviewQualification::cache(h.renderer()),"Up retired before qualified receipt");
    // Freeze canonical work while the independently submitted preview waits.
    runtime::RuntimeFrameOpportunity canonical;
    for(unsigned i=0;i<128&&!canonical.canonicalWork;++i)canonical=h.frame(false);
    require(canonical.canonicalWork.has_value(),"canonical work missing");
    auto captured=canonical;require(captured.previewWork.has_value(),"sealed preview work");
    require(!h.core.finishFrameOpportunity({previous.previewWork,true}),"pre-seal preview qualifier accepted");
    h.preview->completion->drain();h.canonical->completion->drain();
    const auto ackPreview=[&]{require(h.core.finishFrameOpportunity({captured.previewWork,true}),"last preview qualifier");};
    const auto ackCanonical=[&]{require(h.core.receivePlatformPresented({*canonical.canonicalWork,
        PresentOutcome::kPresented,PresentEvidenceKind::kPlatformQualified,{}}),"canonical qualifier");};
    if(canonicalFirst){
        ackCanonical();
        // A covered brush display can already retire on the qualified
        // canonical receipt. Its late preview receipt must then be stale.
        if(h.core.previewActive())ackPreview();
        else require(!h.core.finishFrameOpportunity({captured.previewWork,true}),"retired late preview accepted");
    }
    else{ackPreview();require(h.core.previewActive(),"preview alone qualified canonical");ackCanonical();}
    h.drain();h.empty("qualified handoff retains preview pixels");
    require(!h.core.previewActive()&&h.actual().semanticObjectCount()==1,"qualified handoff state");
    require(!h.core.finishFrameOpportunity({captured.previewWork,true}),"late preview qualifier accepted");
    rows.push_back({{"case",canonicalFirst?"canonical-first":"preview-first"},{"profile",profile},{"dpr",dpr},{"retired",true}});
}
void unavailable(json& rows) {
    Host h("crayon-wax-v1",2);h.draw();auto b=HidpiPreviewQualification::budget(h.renderer());
    h.preview->unavailable=true;require(h.pointer(input::PointerPhase::kCancel),"withheld cancel");
    require(h.core.resourceRetirementPending()&&HidpiPreviewQualification::cache(h.renderer()),"unavailable retire lost release demand");
    require(h.renderer().compactWork().retainedBytes>0&&h.renderer().compactWork().temporaryBytes>0,"queued invalidation reports fake zero resident bytes");
    h.frame();require(h.core.resourceRetirementPending(),"idle dropped withheld demand");
    h.preview->unavailable=false;h.drain();h.empty("idle release does not recover without input");
    rows.push_back({{"case","acquire-unavailable-idle-recovery"},{"released",true}});
}
void contacts(json& rows) {
    Host h("chalk-compact-v1",2);
    // Use the existing Runtime Brush entry for simultaneous sessions, avoiding
    // AutoIntent's two-touch viewport arbitration. Drawing still uses the
    // actual composition deferred capture/render/receipt path.
    auto package=ink::BrushPackageCatalog{}.loadDefault("chalk-compact-v1",3).package;
    for(unsigned id:{1U,2U}){
        require(h.actual().beginBrushSession(id,package,42+id),"multicontact begin");
        for(unsigned i=1;i<=90;++i)require(h.actual().appendBrushSample(id,20+i*2,60+id*25,.5,i,false,false),"multicontact append");
        h.frame();
    }
    auto capture=interaction::PreviewPresentationCapture{};
    require(h.actual().appendBrushSample(2,205,110,.5,91,false,false),"survivor tail");
    require(h.actual().capturePreviewPresentation(capture),"capture survivor");
    std::weak_ptr<const ink::BrushPreviewDelta::Chunk> survivor;
    for(auto& c:capture.geometry.contours)if(c.session==2)survivor=c.compactChunks->previous;
    require(!survivor.expired(),"survivor prefix missing");
    capture={};const auto before=h.renderer().compactWork().generatedKnots;
    require(h.actual().cancelBrushSession(1),"cancel one contact");h.frame();
    require(h.core.previewActive()&&!survivor.expired(),"retirement destroyed live contact");
    require(h.renderer().compactWork().generatedKnots-before<100,"retirement rebuilt live stable prefix");
    require(h.actual().cancelBrushSession(2),"cancel final contact");h.drain();h.empty("multicontact final cache");
    require(survivor.expired(),"retired prefix owner retained");
    rows.push_back({{"case","two-contacts-retire-only-owned-prefix"},{"survivorReused",true},{"prefixReleased",true}});
}
void transitions(json& rows) {
    Host h("crayon-wax-v1",2);h.draw();bool checked=false;
    h.preview->beforeResize=[&]{require(!HidpiPreviewQualification::cache(h.renderer()),"old compact owner survives provider resize");checked=true;};
    require(h.core.resizeSurface({320,200,1280,800,4,1}),"active resize");require(checked,"resize hook not reached");
    h.preview->beforeResize={};h.frame();require(HidpiPreviewQualification::cache(h.renderer()),"resized active preview not rebuilt");
    require(h.pointer(input::PointerPhase::kCancel),"resize cancel");h.drain();h.empty("resize final release");
    rows.push_back({{"case","active-resize-old-context-release"},{"releasedBeforeResize",checked}});

    Host replaced("chalk-compact-v1",2);replaced.draw();
    auto old=replaced.preview->completion;bool destroyed=false;
    require(replaced.core.loseSurface(),"replacement lose");replaced.empty("logical loss keeps old cache");
    auto c=std::make_unique<Target>(),p=std::make_unique<Target>();auto* cr=c.get();auto* pr=p.get();
    c->resize(640,400);p->resize(640,400);
    replaced.preview->beforeDestroy=[&]{require(!HidpiPreviewQualification::cache(replaced.renderer()),"cache crosses context destruction");destroyed=true;};
    require(replaced.core.registerSurfaceProvider("replacement-c",std::move(c))&&replaced.core.registerPreviewSurfaceProvider("replacement-p",std::move(p))&&
        replaced.core.rebindSurface({320,200,640,400,2,1}),"new context pair");
    replaced.canonical=cr;replaced.preview=pr;replaced.frame();require(destroyed,"old provider not destroyed");
    require(replaced.pointer(input::PointerPhase::kCancel),"replacement cancel");replaced.drain();replaced.empty("replacement final release");
    rows.push_back({{"case","provider-replacement-independent-contexts"},{"oldOwnerReleased",destroyed}});

    Host loss("chalk-compact-v1",2);loss.draw();auto lostBudget=HidpiPreviewQualification::budget(loss.renderer());
    loss.preview->physicalLoss();require(loss.core.loseSurface(),"physical loss");
    require(!HidpiPreviewQualification::cache(loss.renderer()),"lost context retains owner");
    loss.actual().serviceResourceRetirements();require(lostBudget->used(resources::ResourceClass::kRaster)==0&&lostBudget->used(resources::ResourceClass::kScratch)==0,"physical-loss leases");
    rows.push_back({{"case","physical-loss"},{"released",true}});

    Host logical("crayon-wax-v1",2);logical.draw();auto originalContext=logical.preview->completion;
    logical.preview->unavailable=true;require(logical.core.loseSurface(),"withheld logical lose");
    require(HidpiPreviewQualification::cache(logical.renderer()),"withheld logical lose unexpectedly released");
    logical.preview->unavailable=false;
    require(logical.core.rebindSurface({320,200,640,400,2,1}),"logical lost acquire recovery deadlock");
    require(!HidpiPreviewQualification::cache(logical.renderer())&&logical.preview->completion==originalContext,
        "logical rebind failed old context release");
    logical.frame();require(HidpiPreviewQualification::cache(logical.renderer()),"logical rebind did not restore active preview");
    require(logical.pointer(input::PointerPhase::kCancel),"logical recovery cancel");logical.drain();logical.empty("logical recovery final release");
    rows.push_back({{"case","logical-loss-unavailable-rebind-same-context"},{"recoveredWithoutNewInput",true}});
}
void closeAndQuota(json& rows) {
    Host h("crayon-wax-v1",2);h.draw();auto b=HidpiPreviewQualification::budget(h.renderer());
    h.preview->unavailable=true;auto options=RenderCacheOptions{};options.maxBytes=8U*1048576;
    h.renderer().setRenderCacheOptions(options);
    require(HidpiPreviewQualification::budget(h.renderer())==b&&HidpiPreviewQualification::pending(h.renderer()),"budget replacement orphans charged cache");
    h.preview->unavailable=false;h.actual().serviceResourceRetirements();
    h.renderer().setRenderCacheOptions(options);require(HidpiPreviewQualification::budget(h.renderer())!=b,"budget replacement cannot finish release");
    require(h.pointer(input::PointerPhase::kCancel),"quota cancel");h.drain();h.empty("quota final cache");
    rows.push_back({{"case","set-options-with-unavailable-target"},{"noOrphanedBudget",true}});
    for(bool unavailable:{false,true}){
        Host closed("chalk-compact-v1",2);closed.draw();auto budget=HidpiPreviewQualification::budget(closed.renderer());
        closed.preview->unavailable=unavailable;require(closed.core.close(),"core close");
        closed.preview->completion->drain();closed.actual().serviceResourceRetirements();
        require(!HidpiPreviewQualification::cache(closed.renderer()),"closed runtime retains compact owner");
        require(budget->used(resources::ResourceClass::kRaster)==0&&budget->used(resources::ResourceClass::kScratch)==0,"closed pixel leases");
        rows.push_back({{"case","close"},{"acquireUnavailable",unavailable},{"ownerReleased",true}});
    }
}
}
int main(int argc, char** argv) try {
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    json rows=json::array();
    for(const auto profile:{"crayon-wax-v1","chalk-compact-v1"})for(unsigned dpr:{2U,4U}){
        cancel(profile,dpr,rows);handoff(profile,dpr,false,rows);handoff(profile,dpr,true,rows);
    }
    unavailable(rows);contacts(rows);transitions(rows);closeAndQuota(rows);
    if (argc>1) std::ofstream(argv[1]) << json{{"rows",rows}}.dump(2) << '\n';
    message(pool,"drain"); return 0;
} catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
