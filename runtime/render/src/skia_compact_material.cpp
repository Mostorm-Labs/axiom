#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPicture.h"
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
const sk_sp<SkRuntimeEffect>& vectorMaterialEffect(){
    static const auto effect=[] {
        auto result=SkRuntimeEffect::MakeForShader(SkString(R"sksl(
            uniform shader coverage; uniform float4 paint;
            half4 main(float2 p){half a=half(coverage.eval(p).r*paint.a);return half4(half3(paint.rgb)*a,a);}
        )sksl"));
        if(!result.effect)throw std::runtime_error(result.errorText.c_str());return result.effect;
    }();return effect;
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
void drawCrayonCoverage(SkCanvas& canvas,const foundation::InkBrushConfig& config,
    std::span<const foundation::InkKnot> knots,std::size_t begin,std::size_t end,double distance,double total,std::uint64_t seed){
    if(knots.empty()||begin>=knots.size())return;
    if(distance<0)distance=ink::compactLength(knots.first(begin));
    SkRuntimeShaderBuilder builder(crayonCoverageEffect());
    if(total<0)total=ink::compactLength(knots);
    builder.uniform("totalDistance")=float(total);builder.uniform("taper")=SkV2{float(config.startTaper),float(config.endTaper)};
    builder.uniform("textureScale")=float(config.textureScale);builder.uniform("deposition")=float(config.deposition);
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

void drawCompactContourCoverageToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,const ink::CompactContour& contour){
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode(SkBlendMode::kLighten);
    paint.setColor(SK_ColorWHITE);
    // Each adjacent pair is one overlapping ribbon. The compiler carries the
    // preceding patch across chunk boundaries, so every pair is rasterized
    // exactly once regardless of input batching. A pending last patch must not
    // be drawn separately: AA edges accumulate when drawn again with its pair.
    for(std::size_t first=0;first+1<contour.patches.size();++first){
        const auto end=first+2;
        SkPathBuilder path;const auto& start=contour.patches[first];path.moveTo(start.leftStart.x,start.leftStart.y);
        for(std::size_t i=first;i<end;++i){const auto& p=contour.patches[i];path.quadTo(p.leftControl.x,p.leftControl.y,p.leftEnd.x,p.leftEnd.y);}
        const auto& last=contour.patches[end-1];path.lineTo(last.rightEnd.x,last.rightEnd.y);
        for(std::size_t i=end;i-->first;){const auto& p=contour.patches[i];path.quadTo(p.rightControl.x,p.rightControl.y,p.rightStart.x,p.rightStart.y);}
        path.close();canvas.drawPath(path.detach(),paint);
    }
    for(const auto& cap:contour.caps){canvas.save();canvas.translate(cap.x,cap.y);
        if(config.family==foundation::InkFamily::kMarker){canvas.rotate(config.markerAngle*57.29577951308232);canvas.scale(1,config.markerFlatness);}
        canvas.drawCircle(0,0,cap.radius,paint);canvas.restore();}
}
void drawCompactCoverageToSkCanvas(SkCanvas& canvas,const foundation::InkBrushConfig& config,
                                  std::span<const foundation::InkKnot> knots,double startDistance,double totalDistance,std::uint64_t seed){
    if(config.algorithmVersion==5){drawCrayonCoverage(canvas,config,knots,0,knots.size(),startDistance,totalDistance,seed);return;}
    if(config.algorithmVersion==4){drawCompactContourCoverageToSkCanvas(canvas,config,ink::compileCompactContour(config,knots));return;}
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
    const bool startCap=startDistance>1e-6||config.startCap||config.startTaper>0;
    const bool endCap=startDistance+ink::compactLength(knots)<totalDistance-1e-6||config.endCap||config.endTaper>0;
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
    if(config.algorithmVersion==5){drawCrayonCoverage(canvas,config,knots,begin,end,beginDistance,totalDistance,seed);return;}
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
        const auto first=begin?begin-1:0;const auto last=std::min(end,knots.size());
        if(beginDistance<0)beginDistance=ink::compactLength(knots.first(first+1));
        if(totalDistance<0)totalDistance=ink::compactLength(knots);
        drawCompactCoverageToSkCanvas(canvas,config,knots.subspan(first,last-first),beginDistance,totalDistance);return;
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
    SkRuntimeShaderBuilder builder(config.materialVersion==3?crayonMaterialEffect():config.algorithmVersion==4?vectorMaterialEffect():config.materialVersion==2?materialV2Effect():materialEffect());
    builder.child("coverage")=image.makeShader(SkSamplingOptions(SkFilterMode::kLinear),local);
    builder.uniform("paint")=SkV4{float(config.color[0]),float(config.color[1]),float(config.color[2]),float(config.color[3]*config.opacity)};
    if(config.algorithmVersion==4||config.materialVersion==3){SkPaint paint;paint.setShader(builder.makeShader());canvas.drawPaint(paint);return;}
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
        auto tile=config.algorithmVersion==5?canvas.makeSurface(SkImageInfo::MakeN32Premul(w,h)):
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w,h));
        if(!tile)throw std::runtime_error("compact coverage tile allocation failed");
        if(work){++work->allocatedTiles;work->allocatedPixels+=std::uint64_t(w)*h;}
        auto* target=tile->getCanvas();target->clear(SK_ColorBLACK);
        target->translate(float(-x),float(-y));target->concat(matrix);
        if(coverage)target->drawPicture(coverage);
        else drawCompactCoverageToSkCanvas(*target,config,knots,0,-1,seed);
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
