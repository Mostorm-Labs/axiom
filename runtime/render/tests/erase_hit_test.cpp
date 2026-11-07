#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <limits>
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
    require(render::intersectEraseGeometry(r,box(110,95,10,10))==Hit::kMiss,"tangent-only erase removed no area");
    require(render::intersectEraseGeometry(r,box(109.99,99.99,1,.02))==Hit::kHit,"small positive intersection was culled");
    r.eraseMasks={{ObjectId::fromUint64(2),center}};
    require(render::intersectEraseGeometry(r,center)==Hit::kMiss,"repeated erase must be no-op");
    require(render::intersectEraseGeometry(r,box(99,92,9,16))==Hit::kHit,"unerased side was lost");
    r.eraseMasks.push_back({ObjectId::fromUint64(3),box(80,80,40,40)});
    require(render::intersectEraseGeometry(r,center)==Hit::kMiss,"fully erased source still hit");
    r=dot();r.eraseMasks={{ObjectId::fromUint64(2),box(80,80,20,40)}};
    require(render::intersectEraseGeometry(r,center)==Hit::kHit,"partially erased source missed visible side");
    r.eraseMasks.push_back({ObjectId::fromUint64(3),box(100,80,20,40)});
    require(render::intersectEraseGeometry(r,center)==Hit::kMiss,"combined historical masks left a false hit");
    r.eraseMasks.pop_back();
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
    // The production repeat corpus has thousands of contour components.
    // The query remains an exact no-op, including a non-identical query fully
    // covered by the union of historical masks. No history mutation is needed.
    auto dense=dot();auto content=std::get<semantic::CompactStrokeContent>(dense.content.get());content.stroke.knots.clear();
    for(unsigned i=0;i<4800;++i)content.stroke.knots.push_back({100.+65*std::sin(i*.075),100.+50*std::cos(i*.11),static_cast<std::uint16_t>(2000+(i*173)%62000)});
    dense.content=content;semantic::SweptCircleMask sweep;
    for(unsigned i=1;i<121;++i){const semantic::Vec2 a{90.+35*std::sin((i-1)*.12),100.+35*std::cos((i-1)*.17)},b{90.+35*std::sin(i*.12),100.+35*std::cos(i*.17)};
        sweep.segments.push_back({{a,16},{b,16},{a.x+(b.x-a.x)/3,a.y+(b.y-a.y)/3},{a.x+2*(b.x-a.x)/3,a.y+2*(b.y-a.y)/3}});}
    dense.eraseMasks={{ObjectId::fromUint64(9),sweep}};
    require(render::intersectEraseGeometry(dense,sweep)==Hit::kMiss,"dense identical swept erase was not a no-op");
    dense.eraseMasks={{ObjectId::fromUint64(10),box(0,0,100,200)},{ObjectId::fromUint64(11),box(100,0,100,200)}};
    require(render::intersectEraseGeometry(dense,box(90,90,20,20))==Hit::kMiss,"dense combined masks left a hit");
    // An invalid optional proof must preserve the original result. A mask
    // outside the body cannot turn an unrelated invalid history into a hit.
    r=dot();r.eraseMasks={{ObjectId::fromUint64(12),box(std::numeric_limits<double>::quiet_NaN(),0,20,20)}};
    require(render::intersectEraseGeometry(r,outside)==Hit::kMiss,"failed fast proof changed disjoint miss");
    require(render::intersectEraseGeometry(r,center)==Hit::kInvalid,"failed fast proof bypassed old-mask validation");
    std::cout<<"exact erase intersection/no-op/old-mask/object-local/selection parity PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
