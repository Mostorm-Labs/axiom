#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "partial_erase_geometry.hpp"
#include "cpu_pending_erase_state.hpp"
#include <algorithm>
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <cstdio>
#endif

namespace canvas::interaction {
std::optional<std::vector<semantic::EraseMaskAddItem>> CanvasEditingRuntime::partialEraseTargets(
    std::span<const foundation::WorldPoint> trace, bool exactHit) const {
    std::vector<semantic::EraseMaskAddItem> result;
    if (trace.empty() || !sceneCoordinator_) return result;
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::exact_prepare,true,semanticGeneration_.current().value(),sceneCoordinator_->runtimeScene().generation().value(),sceneRevision(),cameraGeneration(),surface_.generation,surface_.metricsGeneration,trace.size(),exactHit);
    const double radius = eraserDiameterLogicalPx_ * .5;
    double left=trace.front().x,right=left,top=trace.front().y,bottom=top;
    for(const auto& p:trace){left=std::min(left,double(p.x));right=std::max(right,double(p.x));top=std::min(top,double(p.y));bottom=std::max(bottom,double(p.y));}
    if(!runtimeSceneHost_)return std::nullopt;
    const auto candidates=runtimeSceneHost_->query(canvas::SceneQuery{{float(left-radius),float(top-radius),float(right+radius),float(bottom+radius)}});
    if(!candidates)return std::nullopt;
    const auto cohort=skiaRenderer_.exactQueryCohort(exactHit);
    for(const auto id:candidates.value().backToFront) {
        const auto* found=sceneCoordinator_->runtimeScene().find(id);if(!found)return std::nullopt;
        const auto& record=*found;
        if(record.kind!=semantic::ObjectKind::kVectorStroke && record.kind!=semantic::ObjectKind::kDabStroke)continue;
        if(!textWritable(record.objectId))continue;
        const auto& b=record.worldBounds;
        if(b.right<left-radius||b.left>right+radius||b.bottom<top-radius||b.top>bottom+radius)continue;
        G5_SCOPE(g5MaskConstruct,g5::Tag::wa_mask_construct,g5::Purpose::none);
        auto mask=partialEraseGeometry(trace,record.transform,radius);
        G5_SCOPE_END(g5MaskConstruct);
        UR2D_COUNT(masksBuilt);
        if(!mask)return std::nullopt;
        if(exactHit) {
            G5_SCOPE(g5Path,g5::Tag::pathops,g5::Purpose::none);
            const auto hit=skiaRenderer_.intersectEraseGeometry(record,*mask);
            UR2D_COUNT(exactCalls);
            G5_SCOPE_END(g5Path);
            if(hit==render::SelectionGeometryHit::kInvalid){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
                std::fprintf(stderr,"AXIOM_ERASE_REJECT stage=exact_geometry trace=%zu old_masks=%zu\n",trace.size(),record.eraseMasks.size());
#endif
                return std::nullopt;
            }
            if(hit==render::SelectionGeometryHit::kMiss)continue;
        }
        // Textured materials have no exact opaque-coverage hit oracle. Their
        // bounded broad-phase is conservative; Skia clips the identical mask
        // against actual pigment coverage, never against other object kinds.
        result.push_back({record.objectId,{{{},std::move(*mask)}}});
    }
    return result;
}
bool CanvasEditingRuntime::applyPartialErasePreview(render::ReferenceDrawList& list) const {
    if(!partialEraseFeedbackActive())return true;
    // Feedback may reuse transparent sources for translated ink too. Scope
    // this to the active gesture: exact canonical composition remains unchanged.
    for(auto& entry:list.entries)
        if((entry.record.kind==semantic::ObjectKind::kVectorStroke || entry.record.kind==semantic::ObjectKind::kDabStroke) && textWritable(entry.record.objectId))
            entry.presentationEraseActive=true;
    const auto appendTrace=[&](std::span<const foundation::WorldPoint> trace,double radius){
        // Pure presentation mask generation uses the captured world trace;
        // no new PathOps or semantic target decision is made here.
        for(auto& entry:list.entries){
            if(!entry.presentationEraseActive)continue;
            auto mask=partialEraseGeometry(trace,entry.record.transform,radius);
            if(!mask)return false;
            entry.presentationEraseMasks.push_back({{},std::move(*mask)});
        }
        return true;
    };
    if(cpuPending_){
        std::vector<const CpuPendingEraseState::Intent*> ordered;
        for(const auto& p:cpuPending_->slots)if(p&&p->serial)ordered.push_back(p.get());
        std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->serial<b->serial;});
        for(const auto* p:ordered)if(!appendTrace(p->trace,p->radius))return false;
    }
    for(const auto& [pointer,trace]:eraserTraces_) {
        (void)pointer;
        // The renderer clips this disposable mask against the actual source.
        // A precise CPU PathOps intersection here repeats contour/mask work on
        // every frame without changing pixels. Keep it at the canonical commit
        // boundary, where no-op/history semantics require exact target filtering.
        if(cpuPending_){
            const auto found=std::find_if(cpuPending_->slots.begin(),cpuPending_->slots.end(),[&](const auto& p){return p&&p->session==pointer&&!p->serial;});
            if(found!=cpuPending_->slots.end()){if(!appendTrace(trace,(*found)->radius))return false;continue;}
        }
        auto masks=partialEraseTargets(trace,false);if(!masks)return false;
        for(const auto& item:*masks)for(auto& entry:list.entries)if(entry.record.objectId==item.object_id)
            entry.presentationEraseMasks.insert(entry.presentationEraseMasks.end(),item.masks.begin(),item.masks.end());
    }
    list.canonicalBytes.clear();list.digest.clear();return true;
}
}
