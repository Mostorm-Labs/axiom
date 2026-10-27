#include "canvas/interaction/region_selection_session.hpp"
#include "canvas/interaction/region_selection_geometry.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

using namespace canvas;
using namespace canvas::interaction;
namespace {
input::PointerSample sample(input::PointerPhase phase, float x, float y, std::uint64_t seq) {
    input::PointerSample s; s.key={1,2,3};s.phase=phase;s.x=x;s.y=y;s.sequence=seq;return s;
}
void session_latches_down_and_only_finishes_confirmed_geometry() {
    RegionSelectionSession session;
    auto down=sample(input::PointerPhase::kDown,0,0,1);down.modifiers=input::PointerModifiers::kShift;
    const RegionSelectionContext context{5,6,7,8,9,{}};
    assert(session.begin(down,RegionSelectionKind::kRectangle,context));
    auto move=sample(input::PointerPhase::kMove,100,50,2);
    assert(session.update(move));assert(session.active());
    auto prediction=sample(input::PointerPhase::kMove,999,999,3);prediction.predicted=true;
    assert(session.update(prediction));
    assert(session.outline().size()==4 && session.outline()[2].x==100);
    auto up=sample(input::PointerPhase::kUp,100,50,4);
    const auto result=session.finish(up);
    assert(result.status==RegionFinishStatus::kReady && !session.active());
    assert(result.combine==SelectionCombineMode::kAdd && result.context.selectionRevision==5);
    assert(result.points.size()==4 && result.points[2].y==50);
}
void freeform_closure_cancel_and_generation_fences() {
    RegionSelectionSession session;
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,1),RegionSelectionKind::kFreeform,{}));
    assert(session.update(sample(input::PointerPhase::kMove,100,0,2)));
    auto stale=sample(input::PointerPhase::kUp,100,100,3);stale.key.generation=2;
    assert(session.finish(stale).status==RegionFinishStatus::kIgnored && session.active());
    assert(session.update(sample(input::PointerPhase::kMove,100,100,3)));
    const auto result=session.finish(sample(input::PointerPhase::kUp,0,100,4));
    assert(result.status==RegionFinishStatus::kReady && result.points.size()==4);
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,5),RegionSelectionKind::kFreeform,{}));
    session.cancel();assert(!session.active() && session.outline().empty());
    assert(session.finish(sample(input::PointerPhase::kUp,100,100,6)).status==RegionFinishStatus::kIgnored);
}
void pending_click_microgesture_nonfinite_and_collinear_are_distinct() {
    RegionSelectionSession session;
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,1),RegionSelectionKind::kPendingRectangle,{}));
    assert(session.finish(sample(input::PointerPhase::kUp,2,2,2)).status==RegionFinishStatus::kClick);
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,3),RegionSelectionKind::kRectangle,{}));
    assert(session.finish(sample(input::PointerPhase::kUp,2,2,4)).status==RegionFinishStatus::kInvalid);
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,5),RegionSelectionKind::kFreeform,{}));
    assert(session.update(sample(input::PointerPhase::kMove,50,50,6)));
    assert(session.finish(sample(input::PointerPhase::kUp,100,100,7)).status==RegionFinishStatus::kInvalid);
    auto nan=sample(input::PointerPhase::kDown,std::numeric_limits<float>::quiet_NaN(),0,8);
    assert(!session.begin(nan,RegionSelectionKind::kRectangle,{}));
}
void logical_pixel_threshold_and_confirmed_path_bound_are_enforced() {
    RegionSelectionSession session;
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,1),RegionSelectionKind::kPendingRectangle,{}));
    assert(session.update(sample(input::PointerPhase::kMove,3.99F,0,2)));assert(!session.dragged());
    assert(session.update(sample(input::PointerPhase::kMove,4,0,3)));assert(session.dragged());session.cancel();
    assert(session.begin(sample(input::PointerPhase::kDown,0,0,1),RegionSelectionKind::kFreeform,{}));
    for(std::uint64_t i=1;i<4096;++i)assert(session.update(sample(input::PointerPhase::kMove,static_cast<float>(i),1,i+1)));
    assert(session.outline().size()==4096);
    assert(!session.update(sample(input::PointerPhase::kMove,4096,1,4097)));
    assert(!session.active()&&session.outline().empty());
}
semantic::ObjectRecord shape(unsigned kind,semantic::Transform2D t={}) {
    semantic::ObjectRecord r;r.kind=semantic::ObjectKind::kShape;r.kind_version=2;
    r.transform=t;r.content=semantic::ShapeContent{kind,100,100};return r;
}
void exact_shape_intersections_reject_aabb_false_positives() {
    const std::vector<RegionPoint> corner{{0,0},{5,0},{5,5},{0,5}};
    assert(intersectSelectionRegion(shape(1),corner)==RegionIntersection::kHit);
    assert(intersectSelectionRegion(shape(2),corner)==RegionIntersection::kMiss);
    assert(intersectSelectionRegion(shape(3),corner)==RegionIntersection::kMiss);
    const std::vector<RegionPoint> center{{49,49},{51,49},{51,51},{49,51}};
    assert(intersectSelectionRegion(shape(2),center)==RegionIntersection::kHit);
    const std::vector<RegionPoint> encloses{{-5,-5},{105,-5},{105,105},{-5,105}};
    assert(intersectSelectionRegion(shape(3),encloses)==RegionIntersection::kHit);
    const std::vector<RegionPoint> crosses{{-5,49},{105,49},{105,51},{-5,51}};
    assert(intersectSelectionRegion(shape(2),crosses)==RegionIntersection::kHit);
    // A 45-degree rotated square occupies a diamond; this is an empty AABB corner.
    const double a=std::sqrt(0.5);
    const auto rotated=shape(1,{a,a,-a,a,0,0});
    const std::vector<RegionPoint> empty{{65,2},{69,2},{69,6},{65,6}};
    assert(intersectSelectionRegion(rotated,empty)==RegionIntersection::kMiss);
}
void concave_and_even_odd_self_intersection_are_not_bounding_boxes() {
    const std::vector<RegionPoint> concave{{0,0},{100,0},{100,20},{20,20},{20,100},{0,100}};
    auto small=shape(1);std::get<semantic::ShapeContent>(small.content)={1,5,5};small.transform.tx=60;small.transform.ty=60;
    assert(intersectSelectionRegion(small,concave)==RegionIntersection::kMiss);
    small.transform.tx=5;assert(intersectSelectionRegion(small,concave)==RegionIntersection::kHit);
    const std::vector<RegionPoint> bowtie{{0,0},{100,100},{0,100},{100,0}};
    small.transform.tx=48;small.transform.ty=10;
    assert(intersectSelectionRegion(small,bowtie)==RegionIntersection::kHit);
    small.transform.tx=5;small.transform.ty=48;
    assert(intersectSelectionRegion(small,bowtie)==RegionIntersection::kMiss);
    const std::vector<RegionPoint> repeated{{0,0},{0,0},{100,0},{100,100},{0,100},{0,0}};
    assert(intersectSelectionRegion(small,repeated)==RegionIntersection::kHit);
    const std::vector<RegionPoint> invalid{{0,0},{5,5},{10,10}};
    assert(intersectSelectionRegion(small,invalid)==RegionIntersection::kInvalid);
    // Batch2 now owns VectorStroke geometry: mismatched Shape content is invalid.
    small.kind=semantic::ObjectKind::kVectorStroke;
    assert(intersectSelectionRegion(small,repeated)==RegionIntersection::kInvalid);
}
void existing_image_and_sticky_frames_use_affine_rectangular_geometry() {
    const std::vector<RegionPoint> corner{{0,0},{5,0},{5,5},{0,5}};
    semantic::ObjectRecord image;image.kind=semantic::ObjectKind::kImage;
    semantic::ImageContent content;content.width=100;content.height=100;image.content=content;
    assert(intersectSelectionRegion(image,corner)==RegionIntersection::kHit);
    image.transform.tx=200;assert(intersectSelectionRegion(image,corner)==RegionIntersection::kMiss);
    semantic::ObjectRecord sticky;sticky.kind=semantic::ObjectKind::kSticky;
    sticky.content=semantic::StickyContent{100,100};
    assert(intersectSelectionRegion(sticky,corner)==RegionIntersection::kHit);
    sticky.transform={0,1,-1,0,200,0};
    const std::vector<RegionPoint> rotatedInside{{150,50},{155,50},{155,55},{150,55}};
    assert(intersectSelectionRegion(sticky,rotatedInside)==RegionIntersection::kHit);
    assert(intersectSelectionRegion(sticky,corner)==RegionIntersection::kMiss);
}
}
int main() {
    session_latches_down_and_only_finishes_confirmed_geometry();
    freeform_closure_cancel_and_generation_fences();
    pending_click_microgesture_nonfinite_and_collinear_are_distinct();
    logical_pixel_threshold_and_confirmed_path_bound_are_enforced();
    exact_shape_intersections_reject_aabb_false_positives();
    concave_and_even_odd_self_intersection_are_not_bounding_boxes();
    existing_image_and_sticky_frames_use_affine_rectangular_geometry();
}
