from pathlib import Path
w=Path.cwd()
p=w/'runtime/render/src/compact_raster_internal.hpp';s=p.read_text().replace(' private:\n    std::shared_ptr<const PreparedNavigationGeometry> prepared_;','''#if defined(AXIOM_ENABLE_FRAME_TIMING)
    void diagnosticCursor(std::uint64_t& primitive,std::uint64_t& knot,int& x,int& y,bool& done)const noexcept {
        primitive=preparedPrimitive_;knot=knot_;x=x_;y=y_;done=done_;
    }
#endif
 private:
    std::shared_ptr<const PreparedNavigationGeometry> prepared_;''',1);p.write_text(s)
p=w/'runtime/render/src/skia_render_cache_internal.hpp';s=p.read_text().replace('    void resetPreparation();','''    void resetPreparation(const char* reason="unspecified");
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    std::uint64_t snrResetSequence_=0;
    void diagnosticProgress(const char* phase,const char* reason,int outcome,const PreparationAllowance* allowance=nullptr)const noexcept;
#endif''',1);p.write_text(s)
p=w/'runtime/render/src/skia_render_cache.cpp';s=p.read_text()
# Replace each current reset invocation with stable callsite label, no behavior.
lines=s.splitlines(keepends=True)
for i,line in enumerate(lines):
 if'resetPreparation();'in line:lines[i]=line.replace('resetPreparation();',f'resetPreparation("cache-site-{i+1}");')
s=''.join(lines)
s=s.replace('void SkiaRenderCache::resetPreparation(){','''void SkiaRenderCache::resetPreparation([[maybe_unused]] const char* reason){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    diagnosticProgress("reset-before",reason,-1);++snrResetSequence_;
#endif''',1)
start=s.index('SkiaRenderCache::Preparation SkiaRenderCache::prepare(\n');end=s.index('SkiaRenderCache::InkDraw SkiaRenderCache::drawDamagedInk',start)
a=s[start:end]
a=a.replace('    G5_SCOPE(g5Prepare,g5::Tag::prepare,g5::Purpose::none);','''    G5_SCOPE(g5Prepare,g5::Tag::prepare,g5::Purpose::none);
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    diagnosticProgress("prepare-before","entry",-1,shared);
    struct ProgressEnd {std::function<void()> run;~ProgressEnd(){run();}} snrEnd{[&]{diagnosticProgress("prepare-after","return",-1,shared);}};
#endif''',1)
a=a.replace('    const auto yield=[&]{flushPreparation();', '''    const auto yield=[&]{
#if defined(AXIOM_ENABLE_FRAME_TIMING)
        diagnosticProgress("prepare-yield","allowance-or-layer",int(Preparation::kPending),&allowance);
#endif
        flushPreparation();''',1)
s=s[:start]+a+s[end:]
s+='''
#if defined(AXIOM_ENABLE_FRAME_TIMING)
namespace canvas::render::internal {
void SkiaRenderCache::diagnosticProgress(const char* phase,const char* reason,int outcome,const PreparationAllowance* allowance)const noexcept {
    std::uint64_t primitive=0,knot=0;int x=0,y=0;bool done=false;
    if(pending_&&pending_->raster)pending_->raster->diagnosticCursor(primitive,knot,x,y,done);
    std::size_t pendingBytes=0;if(budget)for(unsigned i=0;i<unsigned(resources::ResourceClass::kCount);++i)pendingBytes+=budget->snapshot(static_cast<resources::ResourceClass>(i)).pendingRelease;
    std::fprintf(stderr,"AXIOM_SNR_PROGRESS {\\"phase\\":\\"%s\\",\\"reason\\":\\"%s\\",\\"outcome\\":%d,\\"cache\\":\\"%llu\\",\\"job\\":\\"%llu\\",\\"frame\\":\\"%llu\\",\\"semantic\\":\\"%llu\\",\\"scene\\":\\"%llu\\",\\"camera\\":\\"%llu\\",\\"surface\\":\\"%llu\\",\\"metrics\\":\\"%llu\\",\\"provider\\":\\"%llu\\",\\"target\\":\\"%llu\\",\\"resource\\":\\"%llu\\",\\"target_epoch\\":\\"%llu\\",\\"cursor\\":%zu,\\"pending_raster\\":\\"%llu\\",\\"primitive\\":%llu,\\"knot\\":%llu,\\"tile_x\\":%d,\\"tile_y\\":%d,\\"raster_done\\":%s,\\"admissions\\":%zu,\\"residents\\":%zu,\\"staged\\":%s,\\"stream\\":%s,\\"complete\\":%s,\\"steps\\":%llu,\\"coverage_steps\\":%llu,\\"body_builds\\":%llu,\\"body_tiles\\":%llu,\\"mask_updates\\":%llu,\\"raster_groups\\":%llu,\\"used_bytes\\":%zu,\\"pending_bytes\\":%zu,\\"gpu_epochs\\":%zu,\\"source_stamp\\":\\"%llu\\",\\"transform\\":[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]}\\n",
      phase,reason,outcome,(unsigned long long)reinterpret_cast<std::uintptr_t>(this),(unsigned long long)snrResetSequence_,
      (unsigned long long)(frame_?frame_->frameId.value():0),(unsigned long long)(frame_?frame_->sceneGeneration.value():0),(unsigned long long)(frame_?frame_->sceneReadToken.value():0),(unsigned long long)(frame_?frame_->camera.generation.value():0),(unsigned long long)(frame_?frame_->surfaceGeneration.value():0),(unsigned long long)(frame_?frame_->metricsGeneration.value():0),
      (unsigned long long)reinterpret_cast<std::uintptr_t>(target_),(unsigned long long)reinterpret_cast<std::uintptr_t>(surface_),(unsigned long long)resourceEpoch_,(unsigned long long)targetEpoch_,preparationCursor_,(unsigned long long)reinterpret_cast<std::uintptr_t>(pending_?pending_->raster.get():nullptr),(unsigned long long)primitive,(unsigned long long)knot,x,y,done?"true":"false",admissions_.size(),entries_.size(),stagedBodies_?"true":"false",streamFrame_?"true":"false",compositeReady_?"true":"false",(unsigned long long)(allowance?allowance->steps:0),(unsigned long long)work.preparationCoverageSteps,(unsigned long long)work.preparedBodyBuilds,(unsigned long long)work.bodyTileBuilds,(unsigned long long)work.maskTileUpdates,(unsigned long long)work.rasterizedGroups,budget?budget->used():0,pendingBytes,budget?budget->pendingGpuEpochCount():0,(unsigned long long)tick_,transform_.a,transform_.b,transform_.c,transform_.d,transform_.tx,transform_.ty);
}
}
#endif
'''
p.write_text(s)
p=w/'sdk/web/bridge.cpp';s=p.read_text().replace('#include <', '#include <',1)
# Existing stdio is available through foundation diagnostic header.
s='#include "canvas/foundation/internal/frame_timing.hpp"\n'+s
s=s.replace('        if (canonical_ <= 0 || preview_ <= 0)','''#if defined(AXIOM_ENABLE_FRAME_TIMING)
        std::fprintf(stderr,"AXIOM_SNR_LIFECYCLE {\\"event\\":\\"context-register\\",\\"owner\\":\\"%llu\\",\\"canonical\\":%d,\\"preview\\":%d}\\n",(unsigned long long)reinterpret_cast<std::uintptr_t>(this),canonical_,preview_);
#endif
        if (canonical_ <= 0 || preview_ <= 0)''',1)
s=s.replace('        return {{"ok", result != platform::WebFrameResult::kRejected},','''#if defined(AXIOM_ENABLE_FRAME_TIMING)
        std::fprintf(stderr,"AXIOM_SNR_LIFECYCLE {\\"event\\":\\"frame-outcome\\",\\"owner\\":\\"%llu\\",\\"was_pending\\":%s,\\"pending\\":%s,\\"result\\":%u,\\"presented\\":\\"%llu\\"}\\n",(unsigned long long)reinterpret_cast<std::uintptr_t>(this),wasPending?"true":"false",adapter_->frameStatus().pending?"true":"false",unsigned(result),(unsigned long long)adapter_->presentedCount());
#endif
        return {{"ok", result != platform::WebFrameResult::kRejected},''',1)
s=s.replace('        preview_ = canonical_ = 0;','''#if defined(AXIOM_ENABLE_FRAME_TIMING)
        std::fprintf(stderr,"AXIOM_SNR_LIFECYCLE {\\"event\\":\\"context-registry-destroy\\",\\"owner\\":\\"%llu\\",\\"canonical\\":%d,\\"preview\\":%d}\\n",(unsigned long long)reinterpret_cast<std::uintptr_t>(this),canonical_,preview_);
#endif
        preview_ = canonical_ = 0;''',1);p.write_text(s)
