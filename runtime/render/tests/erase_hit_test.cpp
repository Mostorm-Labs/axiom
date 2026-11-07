#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
using Hit=render::SelectionGeometryHit;
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
semantic::EraseMaskGeometry box(double x,double y,double w,double h){
    semantic::FilledPathMask mask;mask.path.commands={semantic::MoveTo{{x,y}},semantic::LineTo{{x+w,y}},
        semantic::LineTo{{x+w,y+h}},semantic::LineTo{{x,y+h}},semantic::ClosePath{}};return mask;
}
RuntimeSceneRecord dot(){
    RuntimeSceneRecord r;r.objectId=ObjectId::fromUint64(1);r.kind=semantic::ObjectKind::kVectorStroke;
    semantic::CompactStrokeContent c;c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;
    c.stroke.config.size=20;c.stroke.config.thinning=0;c.stroke.seed=42;c.stroke.knots={{100,100,32768}};r.content=c;return r;
}
}
int main(){try{
    auto r=dot();const auto center=box(95,95,10,10),outside=box(150,150,5,5);
    require(render::intersectEraseGeometry(r,center)==Hit::kHit,"visible dot intersection");
    require(render::intersectEraseGeometry(r,outside)==Hit::kMiss,"outside dot intersection");
    r.eraseMasks={{ObjectId::fromUint64(2),center}};
    require(render::intersectEraseGeometry(r,center)==Hit::kMiss,"repeated erase must be no-op");
    require(render::intersectEraseGeometry(r,box(99,92,9,16))==Hit::kHit,"unerased side was lost");
    r.eraseMasks.push_back({ObjectId::fromUint64(3),box(80,80,40,40)});
    require(render::intersectEraseGeometry(r,center)==Hit::kMiss,"fully erased source still hit");
    r=dot();r.eraseMasks={{ObjectId::fromUint64(2),box(80,80,20,40)}};
    require(render::intersectEraseGeometry(r,center)==Hit::kHit,"partially erased source missed visible side");
    r.transform={1,.2,.3,1.1,80,-30};
    require(render::intersectEraseGeometry(r,center)==Hit::kHit,"object-local hit changed with world affine");
    require(render::intersectEraseGeometry(r,box(90,94,8,12))==Hit::kMiss,"old local mask was ignored");
    // The selection caller keeps its full visible-geometry path. A region
    // wholly in the removed side remains unselectable after erase locality.
    const foundation::WorldPoint removed[]{{90,94},{98,94},{98,106},{90,106}};
    r.transform={};
    require(render::intersectVisibleSelectionGeometry(r,removed)==Hit::kMiss,"selection lost historical mask subtraction");
    auto bad=dot();bad.content=semantic::ShapeContent{1,20,20};
    require(render::intersectEraseGeometry(bad,center)==Hit::kInvalid,"invalid source bypassed validation");
    std::cout<<"exact erase intersection/no-op/old-mask/object-local/selection parity PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
