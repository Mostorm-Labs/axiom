#include "canvas/foundation/internal/frame_timing.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPicture.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkRuntimeEffect.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "ink_coverage_sksl.hpp"
#include "ink_material_sksl.hpp"
#include "ink_material_v2_sksl.hpp"

namespace canvas::render::internal {
namespace {
const sk_sp<SkRuntimeEffect>& coverageEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkCoverageSksl), kInkCoverageSkslSize));
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
const sk_sp<SkRuntimeEffect>& materialV2Effect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(reinterpret_cast<const char*>(kInkMaterialV2Sksl),kInkMaterialV2SkslSize));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());
        return result.effect;
    }();return effect;
}
}

void drawCompactCoverageToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
                                  std::span<const foundation::InkKnot> knots){
    drawCompactCoverageRangeToSkCanvas(canvas,config,knots,0,knots.size());
}
void drawCompactCoverageRangeToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::size_t begin,std::size_t end){
    if(knots.empty())return;
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
    SkRuntimeShaderBuilder builder(config.materialVersion==2?materialV2Effect():materialEffect());
    builder.child("coverage")=image.makeShader(SkSamplingOptions(SkFilterMode::kLinear),local);
    builder.uniform("paint")=SkV4{float(config.color[0]),float(config.color[1]),float(config.color[2]),float(config.color[3]*config.opacity)};
    builder.uniform("family")=float(config.family);
    builder.uniform("density")=float(config.density);builder.uniform("textureScale")=float(config.textureScale);
    builder.uniform("deposition")=float(config.deposition);builder.uniform("wetEdge")=float(config.wetEdge);
    builder.uniform("seed")=SkV2{float(seed&65535U),float((seed>>16)&65535U)};
    SkPaint paint;paint.setShader(builder.makeShader());canvas.drawPaint(paint);
}

void drawCompactStrokeToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
                                std::span<const foundation::InkKnot> knots,std::uint64_t seed,
                                CompactRasterWork* work,const SkPicture* coverage,const CompactDeviceGrid* grid){
    if(knots.empty())return;
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
        AXIOM_FRAME_TIMING_SCOPE(cpuTiming,cpu_coverage);
        AXIOM_FRAME_TIMING_ADD(coverage_tiles,1);
        AXIOM_FRAME_TIMING_ADD(coverage_pixels,std::uint64_t(w)*h);
        auto tile=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w,h));
        if(!tile)throw std::runtime_error("compact coverage tile allocation failed");
        if(work){++work->allocatedTiles;work->allocatedPixels+=std::uint64_t(w)*h;}
        auto* target=tile->getCanvas();target->clear(SK_ColorBLACK);
        target->translate(float(-x),float(-y));target->concat(matrix);
        if(coverage)target->drawPicture(coverage);
        else drawCompactCoverageToSkCanvas(*target,config,knots);
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
}
