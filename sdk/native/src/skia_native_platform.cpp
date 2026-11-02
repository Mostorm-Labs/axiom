#include "skia_native_platform.hpp"
#include "host_font_resources.hpp"
#include <canvas/runtime/internal/runtime_composition_core.hpp>
#include <canvas/runtime/internal/runtime_platform_access.hpp>
#include <chrono>
#include <optional>
#include <cmath>

namespace canvas::sdk {
namespace {
class Platform final:public NativeSessionPlatform {
 runtime::RuntimeComposition* owner_=nullptr;
 NativeView initial_;
 NativeSurfaceFactory factory_;
 std::filesystem::path assets_;
 render::SkiaSurfaceProvider* canonical_=nullptr;
 render::SkiaSurfaceProvider* preview_=nullptr;
 std::optional<runtime::CanonicalWork> confirmed_;
 std::uint64_t canonicalCount_=0,previewCount_=0;
 runtime::RuntimeCompositionCore& core(){return runtime::RuntimePlatformAccess::core(*owner_);}
public:
 Platform(NativeView view,NativeSurfaceFactory factory,std::filesystem::path assets):initial_(view),factory_(std::move(factory)),assets_(std::move(assets)){}
 Json open(runtime::RuntimeComposition& owner,const Json&)override{owner_=&owner;return {{"ok",attach(initial_)},{"code","SURFACE_REJECTED"}};}
 bool attach(const NativeView& view)override{
  if(!owner_)return false;
  try{
   const auto state=core().state();
   if(state!=runtime::RuntimeCompositionState::kBound&&state!=runtime::RuntimeCompositionState::kLost)return false;
   auto pair=factory_(view);if(!pair.canonical||!pair.preview||pair.canonical.get()==pair.preview.get())return false;
   auto* canonical=pair.canonical.get();auto* preview=pair.preview.get();
   if(!core().registerSurfaceProvider(pair.canonicalProfile,std::move(pair.canonical))||!core().registerPreviewSurfaceProvider(pair.previewProfile,std::move(pair.preview)))return false;
   const render::SurfaceMetrics metrics{view.width,view.height,static_cast<std::uint32_t>(std::round(view.width*view.dpr)),static_cast<std::uint32_t>(std::round(view.height*view.dpr)),view.dpr,1};
   const bool bound=state==runtime::RuntimeCompositionState::kBound?core().bindSurface(metrics):core().rebindSurface(metrics);
   if(!bound||!core().setPlatformPresentationDeferred(true))return false;
   canonical_=canonical;preview_=preview;initial_=view;confirmed_.reset();canonicalCount_=previewCount_=0;return true;
  }catch(...){return false;}
 }
 bool detach()override{
  if(!owner_)return false;
  if(core().state()!=runtime::RuntimeCompositionState::kLost&&!core().loseSurface())return false;
  canonical_=preview_=nullptr;confirmed_.reset();return true;
 }
 bool resize(float w,float h,float dpr)override{
  auto view=initial_;view.width=w;view.height=h;view.dpr=dpr;
  return detach()&&attach(view);
 }
 bool lose()override{return detach();}
 bool loadFonts()override{return minimal_runtime_host::bindHostTextFonts(core(),assets_/"axiom-font.otf",assets_/"fonts");}
 Json frame(bool force)override{
  if(!owner_||!canonical_||!preview_)return {{"ok",false},{"code","NOT_ATTACHED"}};
  runtime::RuntimeFrameOpportunity work{};
  if(!core().serviceFrameOpportunity(force,work))return {{"ok",false},{"code","FRAME_REJECTED"}};
  // Providers return from present only after their actual native presentation
  // command completes. Echo exact owner work; never substitute a frame number.
  if(work.previewWork){const auto count=preview_->presentCount();if(count<=previewCount_)return {{"ok",false},{"code","PREVIEW_NOT_PRESENTED"}};
   if(!core().finishFrameOpportunity({work.previewWork,true}))return {{"ok",false},{"code","STALE_PREVIEW"}};previewCount_=count;}
  if(work.canonicalWork&&(!confirmed_||confirmed_->frame!=work.canonicalWork->frame)){
   const auto count=canonical_->presentCount();if(count<=canonicalCount_)return {{"ok",false},{"code","CANONICAL_NOT_PRESENTED"}};
   if(!core().receivePlatformPresented({*work.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,std::nullopt}))return {{"ok",false},{"code","STALE_CANONICAL"}};
   confirmed_=work.canonicalWork;canonicalCount_=count;
  }
  return {{"ok",true},{"pending",core().canonicalPresentationDirty()||(core().previewActive()&&core().previewRenderState().dirty)}};
 }
 bool shutdown()override{return !owner_||core().state()==runtime::RuntimeCompositionState::kLost||detach();}
 void release()noexcept override{canonical_=preview_=nullptr;owner_=nullptr;}
 std::uint64_t nowNs()const noexcept override{return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
};
}
std::unique_ptr<NativeSessionPlatform> makeSkiaNativePlatform(const NativeView& view,NativeSurfaceFactory factory,const std::filesystem::path& assets){return std::make_unique<Platform>(view,std::move(factory),assets);}
}
