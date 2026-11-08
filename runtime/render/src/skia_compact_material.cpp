#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "canvas/foundation/internal/frame_timing.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "compact_raster_internal.hpp"
#include "compact_contour_skia_path.hpp"
#include "skia_prepared_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkBBHFactory.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkRuntimeEffect.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "ink_coverage_sksl.hpp"
#include "ink_coverage_v3_sksl.hpp"
#include "ink_material_sksl.hpp"
#include "ink_material_v2_sksl.hpp"
#include "crayon_coverage_v3_sksl.hpp"
#include "crayon_material_v3_sksl.hpp"
#include "drywet_coverage_v3_sksl.hpp"
#include "drywet_material_v3_sksl.hpp"

namespace canvas::render::internal {
namespace {
const sk_sp<SkRuntimeEffect>& coverageEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkCoverageSksl), kInkCoverageSkslSize));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());
        return result.effect;
    }();return effect;
}
const sk_sp<SkRuntimeEffect>& coverageV3Effect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkCoverageV3Sksl),kInkCoverageV3SkslSize));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());
        return result.effect;
    }();return effect;
}
const sk_sp<SkRuntimeEffect>& materialEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkMaterialSksl), kInkMaterialSkslSize));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());
        return result.effect;
    }();return effect;
}
const sk_sp<SkRuntimeEffect>& vectorMaterialEffect(bool alphaCoverage=false){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(R"sksl(
            uniform shader coverage; uniform float4 paint;
            half4 main(float2 p){half a=half(coverage.eval(p).r*paint.a);return half4(half3(paint.rgb)*a,a);}
        )sksl"));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());return result.effect;
    }();
    if(!alphaCoverage)return effect;
    static const auto alpha=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(R"sksl(
            uniform shader coverage; uniform float4 paint;
            half4 main(float2 p){half a=half(coverage.eval(p).a*paint.a);return half4(half3(paint.rgb)*a,a);}
        )sksl"));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());return result.effect;
    }();return alpha;
}
const sk_sp<SkRuntimeEffect>& crayonCoverageEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kCrayon_coverage_v3),kCrayon_coverage_v3Size));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());return result.effect;
    }();return effect;
}
const sk_sp<SkRuntimeEffect>& crayonMaterialEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kCrayon_material_v3),kCrayon_material_v3Size));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());return result.effect;
    }();return effect;
}
const sk_sp<SkRuntimeEffect>& dryWetEffect(bool material){
    static const auto coverage=[] {
        auto r=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kDryWet_coverage_v3),kDryWet_coverage_v3Size));
        if(!r.effect)throw std::runtime_error(r.errorText.c_str());return r.effect;
    }();
    static const auto paint=[] {
        auto r=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kDryWet_material_v3),kDryWet_material_v3Size));
        if(!r.effect)throw std::runtime_error(r.errorText.c_str());return r.effect;
    }();
    return material?paint:coverage;
}
void drawPressureMaterialCoverage(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::size_t begin,std::size_t end,double distance,double total,std::uint64_t seed){
    if(knots.empty()||begin>=knots.size())return;
    if(distance<0)distance=ink::compactLength(knots.first(begin));
    SkRuntimeShaderBuilder builder(config.algorithmVersion==6?dryWetEffect(false):crayonCoverageEffect());
    if(total<0)total=ink::compactLength(knots);
    builder.uniform("totalDistance")=float(total);builder.uniform("taper")=SkV2{float(config.startTaper),float(config.endTaper)};
    builder.uniform("textureScale")=float(config.textureScale);
    if(config.algorithmVersion==6){builder.uniform("family")=float(config.family);builder.uniform("density")=float(config.density);}
    else builder.uniform("deposition")=float(config.deposition);
    builder.uniform("seed")=SkV2{float(seed&65535U),float((seed>>16)&65535U)};
    SkPaint paint;paint.setBlendMode(SkBlendMode::kLighten);
    const auto clip=canvas.getLocalClipBounds();
    for(auto i=begin;i<std::min(end,knots.size());++i){
        if(i==0&&knots.size()>1)continue;
        const auto& a=knots[i?i-1:0];const auto& b=knots[i];
        const auto ra=ink::compactRadius(config,a.pressure),rb=ink::compactRadius(config,b.pressure);
        const auto radius=std::max(ra,rb);
        const auto bounds=SkRect::MakeLTRB(float(std::min(a.x,b.x)-radius),float(std::min(a.y,b.y)-radius),
                                         float(std::max(a.x,b.x)+radius),float(std::max(a.y,b.y)+radius));
        builder.uniform("a")=SkV2{float(a.x),float(a.y)};builder.uniform("b")=SkV2{float(b.x),float(b.y)};
        builder.uniform("radii")=SkV2{float(ra),float(rb)};
        builder.uniform("pressures")=SkV2{float(a.pressure/65535.),float(b.pressure/65535.)};
        builder.uniform("startDistance")=float(distance);
        builder.uniform("clipCaps")=SkV2{float(i<=1&&distance==0&&!config.startCap),float(i+1==knots.size()&&distance+std::hypot(b.x-a.x,b.y-a.y)>=total-1e-5&&!config.endCap)};
        distance+=std::hypot(b.x-a.x,b.y-a.y);
        if(!SkRect::Intersects(bounds,clip))continue;
        paint.setShader(builder.makeShader());canvas.drawRect(bounds,paint);
    }
}
const sk_sp<SkRuntimeEffect>& materialV2Effect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkMaterialV2Sksl),kInkMaterialV2SkslSize));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());
        return result.effect;
    }();return effect;
}
}

namespace {
void drawContourCoverage(SkCanvas& canvas,const foundation::InkBrushConfig& config,const ink::CompactContour& contour,SkBlendMode blend){
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode(blend);
    paint.setColor(SK_ColorWHITE);
    // Each adjacent pair is one overlapping ribbon. The compiler carries the
    // preceding patch across chunk boundaries, so every pair is rasterized
    // exactly once regardless of input batching. A pending last patch must not
    // be drawn separately: AA edges accumulate when drawn again with its pair.
    for(std::size_t first=0;first+1<contour.patches.size();++first){
        canvas.drawPath(compactContourRibbon(std::span(contour.patches).subspan(first,2)),paint);
    }
    for(const auto& cap:contour.caps){canvas.save();canvas.translate(static_cast<SkScalar>(cap.x),static_cast<SkScalar>(cap.y));
        if(config.family==foundation::InkFamily::kMarker){canvas.rotate(static_cast<SkScalar>(config.markerAngle*57.29577951308232));canvas.scale(1,static_cast<SkScalar>(config.markerFlatness));}
        canvas.drawCircle(0,0,static_cast<SkScalar>(cap.radius),paint);canvas.restore();}
}
}
void drawCompactContourCoverageToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,const ink::CompactContour& contour,bool gpuWhiteCoverage){
    drawContourCoverage(canvas,config,contour,
        (gpuWhiteCoverage||canvas.recordingContext()||canvas.imageInfo().colorType()==kAlpha_8_SkColorType)?SkBlendMode::kSrcOver:SkBlendMode::kLighten);
}
void drawCompactCoverageToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
                                  std::span<const foundation::InkKnot> knots,double startDistance,double totalDistance,std::uint64_t seed,
                                  bool allowStartCap,bool allowEndCap,bool gpuWhiteCoverage){
    if(config.algorithmVersion==5||config.algorithmVersion==6){drawPressureMaterialCoverage(canvas,config,knots,0,knots.size(),startDistance,totalDistance,seed);return;}
    if(config.algorithmVersion==4){drawCompactContourCoverageToSkCanvas(canvas,config,ink::compileCompactContour(config,knots),gpuWhiteCoverage);return;}
    if(config.algorithmVersion!=3){drawCompactCoverageRangeToSkCanvas(canvas,config,knots,0,knots.size());return;}
    if(knots.empty())return;
    const auto points=ink::compactCoverage(config,knots,startDistance,totalDistance);
    if(totalDistance<0)totalDistance=startDistance+ink::compactLength(knots);
    SkMatrix inverse;if(!canvas.getTotalMatrix().invert(&inverse))return;
    const auto pixelWidth=std::max(std::hypot(inverse.getScaleX(),inverse.getSkewY()),std::hypot(inverse.getSkewX(),inverse.getScaleY()));
    SkRuntimeShaderBuilder builder(coverageV3Effect());
    builder.uniform("aaWidth")=pixelWidth;
    const bool marker=config.family==foundation::InkFamily::kMarker;
    const auto angle=marker?config.markerAngle:0.;
    builder.uniform("axis")=SkV2{float(std::cos(angle)),float(std::sin(angle))};
    builder.uniform("flatness")=float(marker?config.markerFlatness:1.);
    SkPaint paint;paint.setBlendMode(SkBlendMode::kLighten);
    const auto clip=canvas.getLocalClipBounds();
    const bool startCap=allowStartCap&&(startDistance>1e-6||config.startCap||config.startTaper>0);
    const bool endCap=allowEndCap&&(startDistance+ink::compactLength(knots)<totalDistance-1e-6||config.endCap||config.endTaper>0);
    for(std::size_t i=0;i<points.size();++i){
        const auto& a=points[i?i-1:0];const auto& b=points[i];
        const auto radius=float(std::max(a.radius,b.radius));if(radius<=0)continue;
        const auto support=radius+pixelWidth;
        auto bounds=SkRect::MakeLTRB(float(std::min(a.x,b.x))-support,float(std::min(a.y,b.y))-support,
                                   float(std::max(a.x,b.x))+support,float(std::max(a.y,b.y))+support);
        if(!SkRect::Intersects(bounds,clip))continue;
        builder.uniform("a")=SkV2{float(a.x),float(a.y)};builder.uniform("b")=SkV2{float(b.x),float(b.y)};
        builder.uniform("radii")=SkV2{float(a.radius),float(b.radius)};
        // Omit the initial standalone disk for a butt cap.
        if(i==0&&!startCap&&points.size()>1)continue;
        builder.uniform("clipCaps")=SkV2{float(i==1&&!startCap),float(i+1==points.size()&&!endCap)};
        paint.setShader(builder.makeShader());canvas.drawRect(bounds,paint);
    }
}
void drawCompactCoverageRangeToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::size_t begin,std::size_t end,double beginDistance,double totalDistance,std::uint64_t seed){
    if(config.algorithmVersion==5||config.algorithmVersion==6){drawPressureMaterialCoverage(canvas,config,knots,begin,end,beginDistance,totalDistance,seed);return;}
    if(knots.empty()||begin>=knots.size()||end<=begin)return;
    if(config.algorithmVersion==4){
        // Compatibility entry point. Stateful navigation uses a retained
        // compiler so prefix traversal is not repeated between render slices.
        ink::CompactContourCompiler compiler(config);ink::CompactContour discarded,emitted;
        if(totalDistance<0)totalDistance=ink::compactLength(knots);
        if(begin)compiler.append(knots.first(begin),totalDistance,discarded);
        const auto last=std::min(end,knots.size());
        compiler.append(knots.subspan(begin,last-begin),totalDistance,emitted);
        if(last==knots.size())compiler.finish(totalDistance,emitted);
        drawCompactContourCoverageToSkCanvas(canvas,config,emitted);return;
    }
    if(config.algorithmVersion==3){
        // Retain one predecessor and two knots after the requested end. The
        // first successor completes the final segment and the second keeps
        // the final segment's finite brush support inside the chunk bounds.
        const auto first=begin?begin-1:0;const auto last=std::min(end+2,knots.size());
        if(beginDistance<0)beginDistance=ink::compactLength(knots.first(first+1));
        if(totalDistance<0)totalDistance=ink::compactLength(knots);
        drawCompactCoverageToSkCanvas(canvas,config,knots.subspan(first,last-first),beginDistance,totalDistance,seed,
                                      begin==0,end==knots.size());return;
    }
    SkRuntimeShaderBuilder builder(coverageEffect());
    const bool marker=config.family==foundation::InkFamily::kMarker;
    const auto angle=marker?config.markerAngle:0.;
    builder.uniform("axis")=SkV2{float(std::cos(angle)),float(std::sin(angle))};
    builder.uniform("flatness")=float(marker?config.markerFlatness:1.);
    SkPaint paint;paint.setBlendMode(SkBlendMode::kLighten);
    const auto clip=canvas.getLocalClipBounds();
    for(std::size_t i=begin;i<std::min(end,knots.size());++i){
        const auto& a=knots[i?i-1:0];const auto& b=knots[i];
        const auto ra=ink::compactRadius(config,a.pressure),rb=ink::compactRadius(config,b.pressure);
        const auto r=float(std::max(ra,rb));
        auto bounds=SkRect::MakeLTRB(float(std::min(a.x,b.x))-r,float(std::min(a.y,b.y))-r,
                                     float(std::max(a.x,b.x))+r,float(std::max(a.y,b.y))+r);
        if(!SkRect::Intersects(bounds,clip))continue;
        builder.uniform("a")=SkV2{float(a.x),float(a.y)};
        builder.uniform("b")=SkV2{float(b.x),float(b.y)};
        builder.uniform("radii")=SkV2{float(ra),float(rb)};
        paint.setShader(builder.makeShader());canvas.drawRect(bounds,paint);
    }
}

void compositeCompactCoverageToSkCanvas(SkCanvas& canvas,const SkImage& image,
                                       const foundation::InkBrushConfig& config,
                                       std::uint64_t seed,int deviceX,int deviceY){
    SkMatrix local;
    if(!canvas.getTotalMatrix().invert(&local))return;
    local.preTranslate(float(deviceX),float(deviceY));
    SkRuntimeShaderBuilder builder(config.materialVersion==4?dryWetEffect(true):config.materialVersion==3?crayonMaterialEffect():config.algorithmVersion==4?vectorMaterialEffect(image.colorType()==kAlpha_8_SkColorType):config.materialVersion==2?materialV2Effect():materialEffect());
    builder.child("coverage")=image.makeShader(SkSamplingOptions(SkFilterMode::kLinear),local);
    builder.uniform("paint")=SkV4{float(config.color[0]),float(config.color[1]),float(config.color[2]),float(config.color[3]*config.opacity)};
    if(config.materialVersion==4){
        builder.uniform("family")=float(config.family);builder.uniform("textureScale")=float(config.textureScale);
        builder.uniform("wetEdge")=float(config.wetEdge);builder.uniform("seed")=SkV2{float(seed&65535U),float((seed>>16)&65535U)};
        SkPaint paint;paint.setShader(builder.makeShader());canvas.drawPaint(paint);return;
    }
    if(config.algorithmVersion==4||config.materialVersion==3){SkPaint paint;paint.setShader(builder.makeShader());canvas.drawPaint(paint);return;}
    builder.uniform("family")=float(config.family);
    builder.uniform("density")=float(config.density);builder.uniform("textureScale")=float(config.textureScale);
    builder.uniform("deposition")=float(config.deposition);builder.uniform("wetEdge")=float(config.wetEdge);
    builder.uniform("seed")=SkV2{float(seed&65535U),float((seed>>16)&65535U)};
    SkPaint paint;paint.setShader(builder.makeShader());canvas.drawPaint(paint);
}

void drawCompactStrokeToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
                                std::span<const foundation::InkKnot> knots,std::uint64_t seed,
                                CompactRasterWork* work,const SkPicture* coverage,const CompactDeviceGrid* grid,bool compatibleCoverage,CompactCoverageTargets* targets,
                                const CompactStrokeRange* range,const PreparedNavigationGeometry* prepared,bool yieldOnAllocation){
    G5_SCOPE(g5Coverage,g5::Tag::coverage,g5::Purpose::none);

    if(knots.empty())return;
    std::vector<foundation::InkKnot> selected;
    double rangeStartDistance = 0.0;
    double rangeTotalDistance = -1.0;
    bool rangeStartCap = true;
    bool rangeEndCap = true;
    if (range) {
        if (range->begin >= range->end || range->end > knots.size()) return;
        const auto first = range->begin ? range->begin - 1 : 0;
        const auto last = std::min(range->end + 2, knots.size());
        selected.assign(knots.begin() + static_cast<std::ptrdiff_t>(first),
                        knots.begin() + static_cast<std::ptrdiff_t>(last));
        knots = selected;
        // startDistance is already anchored to the retained predecessor knot
        // (the same convention used by drawCompactCoverageRangeToSkCanvas).
        rangeStartDistance = range->startDistance;
        rangeTotalDistance = range->totalDistance;
        rangeStartCap = range->allowStartCap;
        rangeEndCap = range->allowEndCap;
        // The cooperative GPU job owns full-history cursor state. Keep the
        // proven exact range path on the bounded CPU coverage/material tiles.
        compatibleCoverage = false;
    }
    auto* execution=RenderExecutionScope::current();
    std::optional<ExecutionDecision> decision;
    if(execution&&config.algorithmVersion==4){
        decision=execution->select(canvas,config.algorithmVersion,range!=nullptr);
        if(decision->route==ExecutionRoute::kRejected||decision->route==ExecutionRoute::kPending)return;
        compatibleCoverage=decision->route==ExecutionRoute::kGpu;
    }
    if(compatibleCoverage&&targets&&config.algorithmVersion==3&&canvas.recordingContext()){
        CompactStrokeRasterJob job(canvas,config,knots,grid,true,targets->budget);
        // Legacy synchronous callers have no resumable owner. Preserve their
        // explicit allocation failure instead of spinning on a denied token.
        while(!job.step(canvas,config,knots,seed,*targets,true,work))
            if(job.resourcePending()){
                if(yieldOnAllocation&&work){work->resourcePending=true;return;}
                throw std::bad_alloc();
            }
        return;
    }
    // V4 keeps the pinned contour; only per-pixel AA moves to Skia's GPU.
    // CPU Direct remains an independent geometry/interior oracle. V3 keeps
    // its exact hybrid policy and V5/V6 retain procedural coverage.
    compatibleCoverage=(compatibleCoverage&&canvas.recordingContext()&&config.algorithmVersion==4)||config.algorithmVersion==5||config.algorithmVersion==6;
    // Tiles are device-clipped; no allocation scales with a long stroke AABB.
    const auto deviceClip=canvas.getDeviceClipBounds();
    const auto clip=grid?SkIRect::MakeLTRB(grid->left,grid->top,grid->right,grid->bottom):deviceClip;const auto matrix=canvas.getTotalMatrix();
    // Coverage has finite support within the centreline's maximum radius.
    // Keep the original clip-anchored grid and shader coordinates so culling
    // empty tiles cannot alter material noise, sampling, or seams.
    SkRect support=SkRect::MakeEmpty();
    for(const auto& knot:knots){
        const auto radius=ink::compactRadius(config,knot.pressure);
        const auto bounds=SkRect::MakeLTRB(float(knot.x-radius),float(knot.y-radius),
                                          float(knot.x+radius),float(knot.y+radius));
        support.join(bounds);
    }
    support=matrix.mapRect(support);
    support.outset(2,2); // Conservative device-pixel margin for image filtering.
    const bool canCull=!matrix.hasPerspective()&&support.isFinite();
    for(int y=clip.top();y<clip.bottom();y+=256)for(int x=clip.left();x<clip.right();x+=256){
        const auto w=std::min(256,clip.right()-x),h=std::min(256,clip.bottom()-y);
        if(canCull&&!SkRect::Intersects(support,SkRect::MakeXYWH(float(x),float(y),float(w),float(h))))continue;
        if(!SkIRect::Intersects(SkIRect::MakeXYWH(x,y,w,h),deviceClip))continue;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
        foundation::internal::frame_timing::Scope cpuTiming{compatibleCoverage&&canvas.recordingContext()
            ?foundation::internal::frame_timing::Stage::gpu_coverage_submit
            :foundation::internal::frame_timing::Stage::cpu_coverage};
#endif
        AXIOM_FRAME_TIMING_ADD(coverage_tiles,1);
        AXIOM_FRAME_TIMING_ADD(coverage_pixels,std::uint64_t(w)*h);
        CompactCoverageTargets localTargets;
        auto* allocationOwner=targets?targets:(execution&&execution->targets()?execution->targets():&localTargets);
        auto tile=decision?allocationOwner->acquire(canvas,w,h,*decision,work).surface
                          :allocationOwner->acquire(canvas,w,h,compatibleCoverage,work);
        G5_TARGET(tile,compatibleCoverage);
        if(!tile){
            if(decision){if(work)work->resourcePending=!execution->rejected();return;}
            if(yieldOnAllocation&&work){work->resourcePending=true;return;}
            if(targets)throw std::bad_alloc();throw std::runtime_error("compact coverage tile allocation failed");
        }
        if(tile&&tile->getCanvas()->recordingContext())AXIOM_FRAME_TIMING_ADD(gpu_coverage_tiles,1);
        else {AXIOM_FRAME_TIMING_ADD(cpu_coverage_tiles,1);
            if(!canvas.recordingContext())AXIOM_FRAME_TIMING_ADD(coverage_no_gpu_context,1);
            else if(!compatibleCoverage)AXIOM_FRAME_TIMING_ADD(coverage_cpu_policy,1);
            else AXIOM_FRAME_TIMING_ADD(coverage_target_fallback,1);}
        if(work){if(tile&&tile->getCanvas()->recordingContext())++work->gpuTiles;else ++work->cpuTiles;}

        if(execution)execution->coverage(*tile,true);
        auto* target=tile->getCanvas();target->resetMatrix();target->clear(SK_ColorBLACK);
        target->translate(float(-x),float(-y));target->concat(matrix);
        if(prepared&&config.algorithmVersion==4)prepared->drawContour(*target);
        else if(coverage)target->drawPicture(coverage);
        else drawCompactCoverageToSkCanvas(*target,config,knots,
                                            range ? rangeStartDistance : 0.0,
                                            range ? rangeTotalDistance : -1.0,
                                            seed,rangeStartCap,rangeEndCap);
        AXIOM_FRAME_TIMING_END(cpuTiming);
        AXIOM_FRAME_TIMING_SCOPE(materialTiming,material_submit);
        canvas.save();canvas.resetMatrix();canvas.clipRect(SkRect::MakeXYWH(float(x),float(y),float(w),float(h)));
        // The tile remains unchanged for exact coverage/filter parity. Only
        // shade its finite support instead of running the material shader
        // over the transparent remainder of this tile.
        SkIRect shaded=SkIRect::MakeXYWH(x,y,w,h);
        if(canCull){
            SkIRect rounded;support.roundOut(&rounded);
            if(!shaded.intersect(rounded)){canvas.restore();continue;}
            canvas.clipIRect(shaded);
        }
        if(work)work->compositedPixels+=std::uint64_t(shaded.width())*shaded.height();
        canvas.setMatrix(matrix);compositeCompactCoverageToSkCanvas(canvas,*tile->makeImageSnapshot(),config,seed,x,y);canvas.restore();
    }
}

sk_sp<SkSurface> CompactCoverageTargets::acquire(SkCanvas& canvas,int w,int h,bool compatible,CompactRasterWork* work,bool scalar){
    // Explicit legacy wrapper. A compatible request on Raster is CPU truth,
    // never a retained GPU key. Other compatibility fallbacks remain declared.
    ExecutionDecision d;d.route=compatible&&canvas.recordingContext()?ExecutionRoute::kGpu:ExecutionRoute::kCpu;
    d.context=d.route==ExecutionRoute::kGpu?canvas.recordingContext():nullptr;
    d.allowCpuFallback=true;
    return acquire(canvas,w,h,d,work,scalar).surface;
}
CompactCoverageTargets::Allocation CompactCoverageTargets::acquire(SkCanvas& canvas,int w,int h,
    const ExecutionDecision& decision,CompactRasterWork* work,bool scalar){
    auto* owner=RenderExecutionScope::current();
    const auto denied=[&](ExecutionReason reason,bool terminal){
        if(owner)owner->unavailable(decision,reason,terminal);
        if(work)work->resourcePending=!terminal;
        return Allocation{nullptr,terminal?ExecutionRoute::kRejected:ExecutionRoute::kPending,reason};
    };
    if(decision.route==ExecutionRoute::kRejected||decision.route==ExecutionRoute::kPending){if(work)work->resourcePending=decision.route==ExecutionRoute::kPending;return {nullptr,decision.route,decision.reason};}
    if(decision.ownerScoped&&(!owner||!owner->accepts(decision)))return denied(ExecutionReason::kStaleBasis,true);
    if(w<=0||h<=0||std::uint64_t(w)*h>std::numeric_limits<std::size_t>::max()/4)
        return denied(ExecutionReason::kImpossibleCapacity,true);
    const auto bytes=std::size_t(w)*h*4;
    if((decision.ownerScoped||decision.maximumTargetBytes)&&bytes>decision.maximumTargetBytes)return denied(ExecutionReason::kImpossibleCapacity,true);
    const bool gpu=decision.route==ExecutionRoute::kGpu;
    if(gpu&&(!decision.context||canvas.recordingContext()!=decision.context))return denied(ExecutionReason::kWrongContext,true);
    const auto lifetimeMatches=[&](const ExecutionBasis& b){const auto& a=decision.basis;
        return a.target==b.target&&a.view==b.view&&a.surface==b.surface&&a.metrics==b.metrics&&a.resource==b.resource;};
    for(auto& slot:slots_)if(slot.surface&&slot.scalar==(!gpu&&scalar)&&slot.context==(gpu?decision.context:nullptr)&&
        lifetimeMatches(slot.basis)&&slot.surface->width()==w&&slot.surface->height()==h){
        slot.used=++tick_;if(work)++work->targetReuses;
        if(owner)owner->allocation(decision,slot.surface.get(),true);
        return {slot.surface,gpu?ExecutionRoute::kGpu:ExecutionRoute::kCpu,decision.reason};
    }
    auto& slot=*std::min_element(slots_.begin(),slots_.end(),[](const auto& a,const auto& b){return a.used<b.used;});
    slot=Slot{};
    auto token=budget?budget->reserve(resources::ResourceClass::kScratch,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!token)return denied(ExecutionReason::kAllocationDenied,false);
    resources::ResourceLease lease(budget,token.value_or(0));
    const auto info=SkImageInfo::MakeN32Premul(w,h);
    auto target=gpu?canvas.makeSurface(info):nullptr;
    if(!target&&(!gpu||decision.allowCpuFallback))target=SkSurfaces::Raster(scalar?SkImageInfo::MakeA8(w,h):info);
    if(!target)return denied(ExecutionReason::kAllocationDenied,false);
    if(owner)owner->allocation(decision,target.get(),false);
    const auto actualContext=target->recordingContext();
    if(gpu&&actualContext!=decision.context&&!decision.allowCpuFallback)
        return {nullptr,ExecutionRoute::kRejected,ExecutionReason::kTargetMismatch};
    slot.surface=target;slot.lease=std::move(lease);slot.context=actualContext;
    slot.basis=decision.basis;slot.scalar=target->imageInfo().colorType()==kAlpha_8_SkColorType;slot.used=++tick_;
    if(work){++work->allocatedTiles;work->allocatedPixels+=bytes/4;}
    return {target,actualContext?ExecutionRoute::kGpu:ExecutionRoute::kCpu,
        gpu&&!actualContext?ExecutionReason::kTargetMismatch:decision.reason};
}
CompactStrokeRasterJob::CompactStrokeRasterJob(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,const CompactDeviceGrid* grid,bool compatible,std::shared_ptr<resources::ResourceBudgetManager> budget,
    std::size_t maxRecordedBytes,std::function<void(std::size_t)> reclaim,bool canonicalReplay)
    :matrix_(canvas.getTotalMatrix()),deviceClip_(canvas.getDeviceClipBounds()),canonicalReplay_(canonicalReplay),reclaim_(std::move(reclaim)){
    clip_=grid?SkIRect::MakeLTRB(grid->left,grid->top,grid->right,grid->bottom):deviceClip_;
    x_=clip_.left();y_=clip_.top();done_=knots.empty()||clip_.isEmpty();
    support_=SkRect::MakeEmpty();
    for(const auto& k:knots){const auto radius=ink::compactRadius(config,k.pressure);
        support_.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));}
    support_=matrix_.mapRect(support_);support_.outset(2,2);
    canCull_=!matrix_.hasPerspective()&&support_.isFinite();
    totalDistance_=ink::compactLength(knots);
    // Record only the pinned v4 helper, incrementally while its first device
    // tile is compiled. Later tiles replay identical commands on their original
    // CPU grids. No full-history preparation is added to a frame opportunity.
    if(config.algorithmVersion==4&&budget&&!done_&&!matrix_.hasPerspective()){
        SkMatrix inverse;const auto count=knots.size()/32+(knots.size()%32!=0);
        const auto groups=count/recordsPerGroup+(count%recordsPerGroup!=0);
        const auto groupBytes=groups*sizeof(RecordGroup);
        if(matrix_.invert(&inverse)&&groupBytes<=maxRecordedBytes&&count<=(maxRecordedBytes-groupBytes)/(sizeof(ContourRecord)+512)){
            recordBounds_=inverse.mapRect(SkRect::Make(deviceClip_));
            const auto margin=2*std::max(std::hypot(inverse.getScaleX(),inverse.getSkewY()),
                                       std::hypot(inverse.getSkewX(),inverse.getScaleY()));
            recordBounds_.outset(float(margin),float(margin));
            const auto bytes=count*(sizeof(ContourRecord)+512)+groupBytes;
            if(recordBounds_.isFinite()&&std::isfinite(margin)){
                auto token=budget->reserve(resources::ResourceClass::kScratch,bytes,true);
                if(token){
                    recordBudget_=budget;recordLimit_=maxRecordedBytes;recordBytes_=bytes;recordStorageBytes_=bytes;picturesEnabled_=true;
                    recordStorageLease_=resources::ResourceLease(budget,*token);
                    try{contourRecords_.reserve(count);recordGroups_.reserve(groups);}catch(const std::bad_alloc&){abandonRecords();}
                }
            }
        }
    }
    // A deliberately inscribed subset of saturated V3 coverage. This is a
    // work-culling region, never substitute brush geometry. The rest always
    // executes the pinned CPU evaluator, including caps and every AA edge.
    if(compatible&&canvas.recordingContext()&&config.algorithmVersion==3&&!matrix_.hasPerspective()&&knots.size()<=1024&&deviceClip_.height()<=8192){
        // Conservative transient path/region bound; denial keeps CPU Direct.
        const auto estimate=std::size_t(65536)+knots.size()*4096+
            std::size_t(std::max(0,deviceClip_.height()))*std::min<std::size_t>(knots.size(),128)*32;
        auto token=budget?budget->reserve(resources::ResourceClass::kScratch,estimate,true):std::optional<std::uint64_t>{};
        if(budget&&!token)return;
        resources::ResourceLease lease(budget,token.value_or(0));
        SkMatrix inverse;
        if(!matrix_.invert(&inverse))return;
        const double pixel=std::max(std::hypot(inverse.getScaleX(),inverse.getSkewY()),
                                    std::hypot(inverse.getSkewX(),inverse.getScaleY()));
        // Region scan conversion is a culling hint only within this bounded
        // numeric range. Large world origins/extreme zoom keep exact CPU.
        const double scale=std::max(std::hypot(matrix_.getScaleX(),matrix_.getSkewY()),
                                    std::hypot(matrix_.getSkewX(),matrix_.getScaleY()));
        if(scale<1./64||scale>64||!std::isfinite(pixel))return;
        for(const auto& knot:knots)if(std::max(std::abs(knot.x),std::abs(knot.y))>65536)return;
        const double flat=config.family==foundation::InkFamily::kMarker?config.markerFlatness:1.;
        const double angle=config.family==foundation::InkFamily::kMarker?config.markerAngle:0.;
        const auto points=ink::compactCoverage(config,knots,0,totalDistance_);
        if(points.size()>4096)return;
        SkPathBuilder path;
        const auto mapped=[&](double x,double y){return SkPoint{float(x*std::cos(angle)-y*flat*std::sin(angle)),
            float(x*std::sin(angle)+y*flat*std::cos(angle))};};
        // AA transition, region scan conversion and whole-pixel guard. The
        // polygon is inscribed (no Bezier circle overshoot at large radii).
        for(std::size_t i=1;i<points.size();++i){
            if((i==1&&!config.startCap&&config.startTaper==0)||
               (i+1==points.size()&&!config.endCap&&config.endTaper==0))continue;
            const auto& a=points[i-1];const auto& b=points[i];
            const double radius=std::min(a.radius,b.radius)-pixel*(2./flat+2.)-.03125;
            if(radius<=0)continue;
            for(const auto& point:{a,b}){
                for(unsigned j=0;j<16;++j){const double t=j*6.283185307179586/16.;
                    const auto p=mapped(std::cos(t)*radius,std::sin(t)*radius);
                    if(j==0)path.moveTo(float(point.x)+p.x(),float(point.y)+p.y());
                    else path.lineTo(float(point.x)+p.x(),float(point.y)+p.y());}
                path.close();
            }
            // Segment vector in nib-local ellipse coordinates.
            const double dx=(b.x-a.x)*std::cos(angle)+(b.y-a.y)*std::sin(angle);
            const double dy=(-(b.x-a.x)*std::sin(angle)+(b.y-a.y)*std::cos(angle))/flat;
            const double length=std::hypot(dx,dy);if(length<1e-6)continue;
            const auto n=mapped(-dy/length*radius,dx/length*radius);
            // Clockwise, same winding as endpoint polygons.
            path.moveTo(float(a.x)-n.x(),float(a.y)-n.y());path.lineTo(float(b.x)-n.x(),float(b.y)-n.y());
            path.lineTo(float(b.x)+n.x(),float(b.y)+n.y());path.lineTo(float(a.x)+n.x(),float(a.y)+n.y());path.close();
        }
        path.transform(matrix_);interior_.setPath(path.detach(),SkRegion(deviceClip_));
        if(interior_.computeRegionComplexity()>8192){interior_.setEmpty();return;}
        interiorLease_=std::move(lease);
    }
}
void CompactStrokeRasterJob::abandonRecords() noexcept {
    std::vector<ContourRecord>{}.swap(contourRecords_);std::vector<RecordGroup>{}.swap(recordGroups_);recordStorageLease_.reset();
    recordBudget_.reset();recordBytes_=0;recordStorageBytes_=0;recordLimit_=0;recordComplete_=false;picturesEnabled_=false;
}
void CompactStrokeRasterJob::appendRecord(ContourRecord record){
    const auto index=contourRecords_.size();
    if(index%recordsPerGroup==0)recordGroups_.push_back({SkRect::MakeEmpty(),index});
    auto& group=recordGroups_.back();
    if(!record.empty)group.bounds.join(record.bounds);
    group.end=index+1;contourRecords_.push_back(std::move(record));
}
void CompactStrokeRasterJob::abandonPictures() noexcept {
    for(auto& record:contourRecords_){record.picture.reset();record.lease.reset();}
    recordBytes_=recordStorageBytes_;picturesEnabled_=false;
}
void CompactStrokeRasterJob::recordContour(const foundation::InkBrushConfig& config,const ink::CompactContour& contour,
    ink::CompactContourCompiler checkpoint){
    if(!recordBudget_)return;
    AXIOM_FRAME_TIMING_SCOPE(recordTiming,contour_record);
    if(contour.patches.size()<2&&contour.caps.empty()){appendRecord({});return;}
    ContourRecord record;record.bounds=compactContourBounds(contour);record.checkpoint=std::move(checkpoint);record.empty=false;
    // Job-owned checkpoints remain bounded scratch under the hard limit.
    // Canonical job-owned replay can use bounded free hard headroom while its
    // mandatory pixel targets occupy the soft budget. It never evicts navigation
    // to admit pictures. Other callers retain the legacy soft-admitted policy.
    if(picturesEnabled_){
        const auto count=contour.patches.size()+contour.caps.size();
        const auto remaining=recordLimit_-recordBytes_;
        if(remaining<4096||count>(remaining-4096)/4096)abandonPictures();
        else{
            const auto estimate=4096+count*4096;
            auto token=recordBudget_->reserve(resources::ResourceClass::kRecorded,estimate,canonicalReplay_);
            if(!token&&!canonicalReplay_&&reclaim_){reclaim_(estimate);token=recordBudget_->reserve(resources::ResourceClass::kRecorded,estimate,false);}
            if(!token)abandonPictures();
            else{
                resources::ResourceLease lease(recordBudget_,*token);
                try{
                    auto hierarchy=SkRTreeFactory{}();
                    SkPictureRecorder recorder;auto* target=recorder.beginRecording(recordBounds_,hierarchy);
                    drawContourCoverage(*target,config,contour,contourBlend_);
                    auto picture=recorder.finishRecordingAsPicture();
                    // Conservatively include BBH storage even on SDK builds
                    // whose approximate picture footprint already includes it.
                    const auto bytes=picture?picture->approximateBytesUsed()+(hierarchy?hierarchy->bytesUsed():0):0;
                    if(!picture||bytes>estimate)abandonPictures();
                    else{
                        lease.reset();token=recordBudget_->reserve(resources::ResourceClass::kRecorded,bytes,canonicalReplay_);
                        if(!token)abandonPictures();
                        else{record.picture=std::move(picture);record.lease=resources::ResourceLease(recordBudget_,*token);recordBytes_+=bytes;}
                    }
                }catch(const std::bad_alloc&){abandonPictures();}
            }
        }
    }
    appendRecord(std::move(record));
}
bool CompactStrokeRasterJob::step(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::uint64_t seed,CompactCoverageTargets& targets,
    bool compatible,CompactRasterWork* work){
    G5_SCOPE(g5Coverage,g5::Tag::coverage,g5::Purpose::none);

    resourcePending_=false;
    if(done_)return true;
    if(!tile_){
        // Empty tiles do no shader work; preserve the Direct grid anchor.
        while(y_<clip_.bottom()){
            const auto rect=SkIRect::MakeXYWH(x_,y_,std::min(256,clip_.right()-x_),std::min(256,clip_.bottom()-y_));
            if(SkIRect::Intersects(rect,deviceClip_)&&(!canCull_||SkRect::Intersects(support_,SkRect::Make(rect))))break;
            x_+=256;if(x_>=clip_.right()){x_=clip_.left();y_+=256;}
        }
        if(y_>=clip_.bottom()){done_=true;return true;}
        const auto w=std::min(256,clip_.right()-x_),h=std::min(256,clip_.bottom()-y_);
        tileInterior_=interior_;tileInterior_.op(SkIRect::MakeXYWH(x_,y_,w,h),SkRegion::kIntersect_Op);
        hybrid_=compatible&&canvas.recordingContext()&&config.algorithmVersion==3&&!tileInterior_.isEmpty();
        tileInterior_.translate(-x_,-y_);
        // V4 reuses the same contour/checkpoints on a GPU N32 target. No
        // CPU edge correction. Legacy continuous-gradient coverage keeps its
        // separately qualified hybrid/CPU policy.
        auto* execution=RenderExecutionScope::current();
        if(execution&&config.algorithmVersion==4){
            const auto decision=execution->select(canvas,config.algorithmVersion,false);
            compatible=decision.route==ExecutionRoute::kGpu;
            tile_=targets.acquire(canvas,w,h,decision,work,!compatible&&config.materialVersion<3).surface;
        }else tile_=targets.acquire(canvas,w,h,hybrid_||(compatible&&((config.algorithmVersion==4&&canvas.recordingContext())||config.algorithmVersion==5||config.algorithmVersion==6)),work,config.algorithmVersion==4&&config.materialVersion<3);
        if(!tile_){resourcePending_=true;return false;}
        if(execution)execution->coverage(*tile_,false);
        hybrid_=hybrid_&&tile_->getCanvas()->recordingContext();
        if(tile_->getCanvas()->recordingContext())AXIOM_FRAME_TIMING_ADD(gpu_coverage_tiles,1);
        else {AXIOM_FRAME_TIMING_ADD(cpu_coverage_tiles,1);
            if(!canvas.recordingContext())AXIOM_FRAME_TIMING_ADD(coverage_no_gpu_context,1);
            else if(!compatible)AXIOM_FRAME_TIMING_ADD(coverage_cpu_policy,1);
            else if(config.algorithmVersion==4)AXIOM_FRAME_TIMING_ADD(coverage_target_fallback,1);}
        if(work){if(tile_->getCanvas()->recordingContext())++work->gpuTiles;else ++work->cpuTiles;
            if(tile_->imageInfo().colorType()==kAlpha_8_SkColorType)++work->scalarCoverageTiles;}
        if(tile_->imageInfo().colorType()==kAlpha_8_SkColorType)AXIOM_FRAME_TIMING_ADD(scalar_coverage_tiles,1);
        AXIOM_FRAME_TIMING_ADD(coverage_tiles,1);AXIOM_FRAME_TIMING_ADD(coverage_pixels,std::uint64_t(w)*h);
        const auto blend=tile_->imageInfo().colorType()==kAlpha_8_SkColorType||
            (config.algorithmVersion==4&&tile_->getCanvas()->recordingContext())?SkBlendMode::kSrcOver:SkBlendMode::kLighten;
        // A target-format fallback must not replay pictures recorded with a
        // different coverage encoding. The exact checkpoints remain valid.
        if(recordComplete_&&blend!=contourBlend_)abandonPictures();
        contourBlend_=blend;
        G5_TARGET(tile_,compatible);
        auto* target=tile_->getCanvas();target->resetMatrix();target->clear(tile_->imageInfo().colorType()==kAlpha_8_SkColorType?SK_ColorTRANSPARENT:SK_ColorBLACK);
        target->translate(float(-x_),float(-y_));target->concat(matrix_);
        if(hybrid_){
            correction_=targets.acquire(canvas,w,h,false,work);
            if(!correction_){tile_.reset();resourcePending_=true;return false;}
            auto* cpu=correction_->getCanvas();cpu->resetMatrix();cpu->clear(SK_ColorBLACK);
            // Saturation encodes .045 to the exact pinned N32 value (11/255).
            AXIOM_FRAME_TIMING_SCOPE(interiorTiming,gpu_coverage_submit);
            target->resetMatrix();SkPaint paint;paint.setColor4f({11.F/255,11.F/255,11.F/255,1},nullptr);
            paint.setBlendMode(SkBlendMode::kSrc);target->drawPaint(paint);
            std::uint64_t area=0;
            for(SkRegion::Iterator it(tileInterior_);!it.done();it.next())area+=std::uint64_t(it.rect().width())*it.rect().height();
            const auto remainder=std::uint64_t(w)*h-area;
            if(work){work->gpuInteriorPixels+=area;work->cpuEdgePixels+=remainder;}
            AXIOM_FRAME_TIMING_ADD(gpu_interior_pixels,area);
            AXIOM_FRAME_TIMING_ADD(cpu_edge_pixels,remainder);
        }
        knot_=0;distance_=0;
        if(config.algorithmVersion==4&&!recordComplete_)contour_.emplace(config);else contour_.reset();
    }
    if(recordComplete_){
        AXIOM_FRAME_TIMING_SCOPE(selectionTiming,coverage_select);
        const auto clip=compactContourCullBounds(*tile_->getCanvas());
        std::size_t checks=0;
        while(knot_<knots.size()&&checks<selectionChecksPerStep){
            const auto index=knot_/32;
            if(index%recordsPerGroup==0){
                ++checks;if(work)++work->contourRecordGroups;AXIOM_FRAME_TIMING_ADD(record_groups,1);
                const auto& group=recordGroups_[index/recordsPerGroup];
                if(group.bounds.isEmpty()||(clip&&group.bounds.isFinite()&&!SkRect::Intersects(*clip,group.bounds))){
                    const auto skipped=group.end-index;
                    if(work)work->contourRecordsRejected+=skipped;AXIOM_FRAME_TIMING_ADD(record_rejected,skipped);
                    knot_=std::min(group.end*32,knots.size());continue;
                }
                if(checks==selectionChecksPerStep)break;
            }
            ++checks;if(work)++work->contourRecordCandidates;AXIOM_FRAME_TIMING_ADD(record_candidates,1);
            const auto& record=contourRecords_[index];
            if(!record.empty&&(!clip||!record.bounds.isFinite()||record.bounds.isEmpty()||SkRect::Intersects(*clip,record.bounds)))break;
            if(work)++work->contourRecordsRejected;AXIOM_FRAME_TIMING_ADD(record_rejected,1);
            knot_=std::min((index+1)*32,knots.size());
        }
        if(knot_<knots.size()&&checks==selectionChecksPerStep){
            if(work)++work->contourSelectionYields;AXIOM_FRAME_TIMING_ADD(record_selection_yields,1);return false;
        }
    }
    if(knot_<knots.size()){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
        foundation::internal::frame_timing::Scope coverageTiming{!hybrid_&&tile_->getCanvas()->recordingContext()
            ?foundation::internal::frame_timing::Stage::gpu_coverage_submit
            :foundation::internal::frame_timing::Stage::cpu_coverage};
#endif
        const auto end=std::min(knot_+32,knots.size());
        auto* target=(hybrid_?correction_:tile_)->getCanvas();
        target->save();
        if(hybrid_){
            target->resetMatrix();target->clipRegion(tileInterior_,SkClipOp::kDifference);
            target->translate(float(-x_),float(-y_));target->concat(matrix_);
        }
        if(recordComplete_){
            AXIOM_FRAME_TIMING_SCOPE(replayTiming,contour_replay);
            const auto& record=contourRecords_[knot_/32];
            if(record.picture){
                target->drawPicture(record.picture);if(work)++work->contourRecordReplays;AXIOM_FRAME_TIMING_ADD(record_replays,1);
            }else if(record.checkpoint){
                auto compiler=*record.checkpoint;ink::CompactContour emitted;
                {AXIOM_FRAME_TIMING_SCOPE(compileTiming,contour_compile);
                    compiler.append(knots.subspan(knot_,end-knot_),totalDistance_,emitted);
                    if(end==knots.size())compiler.finish(totalDistance_,emitted);}
                {AXIOM_FRAME_TIMING_SCOPE(drawTiming,contour_draw);
                    drawContourCoverage(*target,config,emitted,contourBlend_);}
                if(work){++work->contourCheckpointReplays;work->checkpointReplayKnots+=end-knot_;work->compiledContourKnots+=end-knot_;}
                AXIOM_FRAME_TIMING_ADD(checkpoint_replays,1);AXIOM_FRAME_TIMING_ADD(checkpoint_replay_knots,end-knot_);
                AXIOM_FRAME_TIMING_ADD(compiled_contour_knots,end-knot_);
            }
        }else if(contour_){
            if(work)work->compiledContourKnots+=end-knot_;
            AXIOM_FRAME_TIMING_ADD(compiled_contour_knots,end-knot_);
            auto checkpoint=recordBudget_?std::optional<ink::CompactContourCompiler>(*contour_):std::nullopt;
            ink::CompactContour emitted;
            {AXIOM_FRAME_TIMING_SCOPE(compileTiming,contour_compile);
                contour_->append(knots.subspan(knot_,end-knot_),totalDistance_,emitted);
                if(end==knots.size())contour_->finish(totalDistance_,emitted);}
            {AXIOM_FRAME_TIMING_SCOPE(drawTiming,contour_draw);
                drawContourCoverage(*target,config,emitted,contourBlend_);}
            if(checkpoint)recordContour(config,emitted,std::move(*checkpoint));
        }else drawCompactCoverageRangeToSkCanvas(*target,config,knots,knot_,end,distance_,totalDistance_,seed);
        target->restore();
        if(config.algorithmVersion!=4)for(std::size_t i=std::max(std::size_t(1),knot_);i<end;++i)
            distance_+=std::hypot(knots[i].x-knots[i-1].x,knots[i].y-knots[i-1].y);
        knot_=end;
    }
    if(knot_<knots.size())return false;
    if(recordBudget_)recordComplete_=true;
    if(hybrid_&&correction_){
        AXIOM_FRAME_TIMING_SCOPE(edgeUploadTiming,material_submit);
        auto image=correction_->makeImageSnapshot();if(!image){resourcePending_=true;return false;}
        auto* target=tile_->getCanvas();target->save();target->resetMatrix();
        target->clipRegion(tileInterior_,SkClipOp::kDifference);
        SkPaint paint;paint.setBlendMode(SkBlendMode::kSrc);
        target->drawImage(image,0,0,SkSamplingOptions(),&paint);target->restore();
        correction_.reset();
    }
    {
        AXIOM_FRAME_TIMING_SCOPE(materialTiming,material_submit);
        auto image=tile_->makeImageSnapshot();if(!image){resourcePending_=true;return false;}
        canvas.save();canvas.resetMatrix();
        auto shaded=SkIRect::MakeXYWH(x_,y_,tile_->width(),tile_->height());
        if(canCull_){SkIRect rounded;support_.roundOut(&rounded);if(!shaded.intersect(rounded))shaded=SkIRect::MakeEmpty();}
        canvas.clipIRect(shaded);canvas.setMatrix(matrix_);
        if(work)work->compositedPixels+=std::uint64_t(shaded.width())*shaded.height();
        compositeCompactCoverageToSkCanvas(canvas,*image,config,seed,x_,y_);canvas.restore();
    }
    tile_.reset();x_+=256;if(x_>=clip_.right()){x_=clip_.left();y_+=256;}
    done_=y_>=clip_.bottom();return done_;
}

}
