
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)) { std::cerr << "CHECK FAILED " << #x << " line " << __LINE__ << std::endl; std::exit(1); } } while(false)
#include "../common/creation_session.hpp"
#include "../common/ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"

#include "canvas/interaction/viewport_constraints.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/render_view_runtime.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <limits>
#include <iostream>
#include <vector>
#include <string_view>

namespace {

using canvas::foundation::ObjectId;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::ink_playground::CreationPreset;
using canvas::ink_playground::CreationSession;
using canvas::ink_playground::CreationStyle;
using canvas::input::PointerKey;
using canvas::interaction::SnapCandidate;
using canvas::interaction::SnapFeatureKind;
using canvas::interaction::SnapPolicy;
using canvas::interaction::SnapSource;
using canvas::interaction::SnapTarget;
using canvas::interaction::SnapResolver;

struct ReferenceCandidate final {
  SnapCandidate candidate{};
};

float featureValue(const WorldRect& rect, SnapFeatureKind feature, bool horizontal) {
  const float minimum = horizontal ? rect.left : rect.top;
  const float maximum = horizontal ? rect.right : rect.bottom;
  if (feature == SnapFeatureKind::kCenter) return (minimum + maximum) * 0.5F;
  return feature == SnapFeatureKind::kMinEdge ? minimum : maximum;
}

bool less(const SnapCandidate& left, const SnapCandidate& right) {
  const float ld = std::abs(left.correction);
  const float rd = std::abs(right.correction);
  if (ld != rd) return ld < rd;
  if (left.targetObjectId != right.targetObjectId)
    return left.targetObjectId < right.targetObjectId;
  if (left.targetFeature != right.targetFeature)
    return static_cast<unsigned>(left.targetFeature) < static_cast<unsigned>(right.targetFeature);
  return static_cast<unsigned>(left.sourceFeature) < static_cast<unsigned>(right.sourceFeature);
}

std::optional<ReferenceCandidate> referenceAxis(
    const WorldRect& source, const std::vector<SnapTarget>& targets, bool horizontal,
    float dpr, std::uint8_t features, float engage) {
  const std::array sourceValues{
      std::pair{SnapFeatureKind::kMinEdge, horizontal ? source.left : source.top},
      std::pair{SnapFeatureKind::kCenter, (horizontal ? source.left + source.right
                                                       : source.top + source.bottom) * 0.5F},
      std::pair{SnapFeatureKind::kMaxEdge, horizontal ? source.right : source.bottom}};
  std::optional<ReferenceCandidate> best;
  (void)dpr;
  const float threshold = engage;
  for (const auto& target : targets) {
    if (target.selected || target.transformed || !target.viewBounds.isFiniteAndOrdered()) continue;
    for (const auto targetFeature : {SnapFeatureKind::kMinEdge, SnapFeatureKind::kCenter,
                                     SnapFeatureKind::kMaxEdge}) {
      for (const auto& [sourceFeature, value] : sourceValues) {
        if ((features & (1U << static_cast<unsigned>(sourceFeature))) == 0U) continue;
        const float correction = featureValue(target.viewBounds, targetFeature, horizontal) - value;
        SnapCandidate candidate{target.objectId, targetFeature, sourceFeature, correction};
        if (std::abs(correction) > threshold) continue;
        if (!best || less(candidate, best->candidate)) best = ReferenceCandidate{candidate};
      }
    }
  }
  return best;
}

WorldPoint mapLogical(WorldPoint p,float zoom,float rotation,WorldPoint center={128,128}) {
  const auto c=std::cos(rotation),si=std::sin(rotation);
  return {128+zoom*(c*(p.x-center.x)+si*(p.y-center.y)),
          128+zoom*(-si*(p.x-center.x)+c*(p.y-center.y))};
}
WorldRect conservativeViewport(float zoom,float rotation) {
  WorldRect bounds{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),
      -std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
  for(auto p:{WorldPoint{0,0},WorldPoint{256,0},WorldPoint{0,256},WorldPoint{256,256}}) {
    const float x=(p.x-128)/zoom,y=(p.y-128)/zoom;
    const auto c=std::cos(rotation),si=std::sin(rotation);
    const WorldPoint world{128+c*x-si*y,128+si*x+c*y};
    bounds.left=std::min(bounds.left,world.x);bounds.right=std::max(bounds.right,world.x);
    bounds.top=std::min(bounds.top,world.y);bounds.bottom=std::max(bounds.bottom,world.y);
  }
  return bounds;
}
canvas::semantic::ObjectRecord authoredShape(std::uint64_t id,WorldRect bounds,bool visible=true,bool locked=false) {
  canvas::semantic::ObjectRecord object{};object.id=ObjectId::fromUint64(id);
  object.kind=canvas::semantic::ObjectKind::kShape;object.kind_version=1;
  object.placement.order_key=canvas::semantic::OrderKey{{1,static_cast<std::uint8_t>(id%254+1)}};
  object.transform.tx=bounds.left;object.transform.ty=bounds.top;
  object.content=canvas::semantic::ShapeContent{1,bounds.right-bounds.left,bounds.bottom-bounds.top};
  if(!visible)object.properties.entries.push_back({1,false});
  if(locked)object.properties.entries.push_back({2,true});
  return object;
}

void creation_session_indexed_reference_camera_matrix() {
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  unsigned compared=0;
  for(bool reverse:{false,true}) for(auto policy:{SnapPolicy{6,10},SnapPolicy{3,5}})
    for(float zoom:{0.5F,1.0F,2.0F})for(float dpr:{1.0F,2.0F,3.0F})for(float rotation:{0.0F,0.37F}) {
      canvas::ink_playground::InkPlaygroundHost host;assert(host.bindSurface(256,256));
      // Independent literals: far perpendicular X/Y references, tie, hidden,
      // outside every finite viewport, and a locked passive Shape reference.
      auto x=authoredShape(7,{100,180,140,220},true,true);
      auto tie=authoredShape(9,{100,180,140,220});
      auto y=authoredShape(11,{180,100,220,140});
      auto hidden=authoredShape(3,{97,180,137,220},false);
      auto offscreen=authoredShape(13,{100,900,140,940});
      std::vector<canvas::semantic::ObjectRecord> fixtures{x,tie,y,hidden,offscreen};
      if(reverse)std::reverse(fixtures.begin(),fixtures.end());
      assert(Access::applyRemote(host,canvas::semantic::InsertObjectsOp{fixtures}));
      const auto viewport=conservativeViewport(zoom,rotation);
      const auto indexed=Access::query(host,viewport);assert(indexed);
      std::vector<ObjectId> linearIds;
      std::vector<SnapTarget> referenceTargets,indexedTargets;
      // Explicit literal bounds are independent of Scene bounds calculation.
      for(const auto& item:std::array<std::pair<ObjectId,WorldRect>,4>{{
          {x.id,{100,180,140,220}},{tie.id,{100,180,140,220}},
          {y.id,{180,100,220,140}},{offscreen.id,{100,900,140,940}}}}) {
        if(item.second.intersects(viewport)) {
          linearIds.push_back(item.first);
          referenceTargets.push_back({item.first,{item.second.left*zoom,item.second.top*zoom,
              item.second.right*zoom,item.second.bottom*zoom},false,false});
        }
      }
      auto actualIds=indexed.value().backToFront;std::sort(actualIds.begin(),actualIds.end());
      std::sort(linearIds.begin(),linearIds.end());assert(actualIds==linearIds);
      const auto scene=Access::sceneRecords(host);
      for(const auto id:indexed.value().backToFront) {
        const auto record=std::find_if(scene.begin(),scene.end(),[&](const auto& r){return r.objectId==id;});
        assert(record!=scene.end());indexedTargets.push_back({id,record->worldBounds,false,false});
      }
      for(auto preset:{CreationPreset::kRectangle,CreationPreset::kEllipse,CreationPreset::kTriangle,
          CreationPreset::kLine,CreationPreset::kArrow}) {
        const WorldPoint start{20,20},end{100-2.0F/zoom,100-2.0F/zoom};
        CreationSession session;session.setSnapPolicy(policy);session.setSnapScale(zoom);
        session.setSnapTargets(indexedTargets);const PointerKey key{21,42,1};
        assert(session.begin(key,preset,CreationStyle{},start,viewport,123));
        assert(session.move(key,end));
        const WorldRect source{start.x*zoom,start.y*zoom,end.x*zoom,end.y*zoom};
        const auto expectedX=referenceAxis(source,referenceTargets,true,dpr,6,policy.engageLogicalPx);
        const auto expectedY=referenceAxis(source,referenceTargets,false,dpr,6,policy.engageLogicalPx);
        WorldPoint expectedEnd=end;
        const auto adjust=[&](const auto& candidate,float& value) {
          if(candidate)value+=(candidate->candidate.sourceFeature==SnapFeatureKind::kCenter?2:1)*
              candidate->candidate.correction/zoom;
        };
        adjust(expectedX,expectedEnd.x);adjust(expectedY,expectedEnd.y);
        const auto preview=session.preview();assert(preview.valid);
        assert(std::abs(preview.end.x-expectedEnd.x)<1e-4 && std::abs(preview.end.y-expectedEnd.y)<1e-4);
        assert(preview.guides.size()==static_cast<unsigned>(expectedX.has_value())+static_cast<unsigned>(expectedY.has_value()));
        for(const auto& guide:preview.guides) {
          const bool xAxis=guide.axis==canvas::interaction::SnapGuideAxis::kVertical;
          const auto& expected=xAxis?expectedX:expectedY;assert(expected);
          const auto ref=std::find_if(referenceTargets.begin(),referenceTargets.end(),[&](const auto& target){
            return target.objectId==expected->candidate.targetObjectId;});assert(ref!=referenceTargets.end());
          assert(std::abs(guide.position-featureValue(ref->viewBounds,expected->candidate.targetFeature,xAxis)/zoom)<1e-4);
          const float sourceMin=xAxis?start.y:start.x,sourceMax=xAxis?end.y:end.x;
          const float targetMin=(xAxis?ref->viewBounds.top:ref->viewBounds.left)/zoom;
          const float targetMax=(xAxis?ref->viewBounds.bottom:ref->viewBounds.right)/zoom;
          assert(std::abs(guide.spanStart-std::max(std::min(sourceMin,targetMin),xAxis?viewport.top:viewport.left))<1e-4);
          assert(std::abs(guide.spanEnd-std::min(std::max(sourceMax,targetMax),xAxis?viewport.bottom:viewport.right))<1e-4);
        }
        const auto committed=session.finish(key);assert(committed && committed->record==preview.record);
        if(const auto* shape=std::get_if<canvas::semantic::ShapeContent>(&committed->record.content)) {
          assert(std::abs(shape->width-(expectedEnd.x-start.x))<1e-4);
          assert(std::abs(shape->height-(expectedEnd.y-start.y))<1e-4);
        } else {
          const auto connector=std::get<canvas::semantic::ConnectorContent>(committed->record.content);
          const auto point=std::get<canvas::semantic::FreePointEndpoint>(connector.end.value).point;
          assert(std::abs(point.x-expectedEnd.x)<1e-4 && std::abs(point.y-expectedEnd.y)<1e-4);
        }
        // Nonzero rotation remains a world-axis constraint. Its logical length
        // is zoom*world length; DPR only selects physical raster density.
        const auto beginView=mapLogical(start,zoom,rotation),endView=mapLogical(expectedEnd,zoom,rotation);
        canvas::render::FrameState frame{canvas::render::ViewId{1},
            canvas::render::CameraState{{128,128},zoom,rotation,canvas::render::CameraGeneration{1}},
            viewport,canvas::render::SurfaceMetrics{256,256,static_cast<unsigned>(256*dpr),static_cast<unsigned>(256*dpr),dpr,1},
            host.semanticGeneration(),canvas::foundation::SceneRevision{host.sceneRevision()},
            canvas::render::SurfaceGeneration{host.surface().generation},canvas::render::MetricsGeneration{1},canvas::render::FrameId{1}};
        canvas::render::VisibilityResult visible{host.semanticGeneration(),
            canvas::foundation::SceneRevision{host.sceneRevision()},viewport,
            indexed.value().diagnostics.candidatesExamined,
            indexed.value().backToFront.size(),indexed.value().backToFront};
        const auto draw=canvas::render::DirectReferenceSource::build(frame,visible,Access::runtimeScene(host));
        assert(draw);
        const auto& affine=draw.value().worldToView;
        const auto project=[&](WorldPoint point) {
          return WorldPoint{static_cast<float>(affine.a*point.x+affine.c*point.y+affine.tx),
                            static_cast<float>(affine.b*point.x+affine.d*point.y+affine.ty)};
        };
        const auto actualStart=project(start),actualEnd=project(expectedEnd);
        assert(std::abs(actualStart.x-beginView.x)<1e-4 && std::abs(actualStart.y-beginView.y)<1e-4);
        assert(std::abs(actualEnd.x-endView.x)<1e-4 && std::abs(actualEnd.y-endView.y)<1e-4);
        canvas::render::RenderViewRuntime view(frame);
        view.editingOverlay().setCreationPreview(preview.record,{});
        assert(view.editingOverlay().creationPreview()==preview.record);
        const auto delta=mapLogical({expectedEnd.x+1,expectedEnd.y},zoom,rotation);
        assert(std::abs(std::hypot(delta.x-endView.x,delta.y-endView.y)-zoom)<1e-4);
        assert(std::isfinite(beginView.x)&&std::isfinite(endView.y));
        ++compared;
      }
    }
  std::cout<<"PASS E04 indexed viewport/linear target ids, five presets, preview/guide/final matrix cases="<<compared<<"\n";
}

void logical_threshold_is_dpr_invariant() {
  for (const auto policy : {SnapPolicy{6,10}, SnapPolicy{3,5}}) {
    for (const auto dpr : {1.0F,2.0F,3.0F}) {
      SnapResolver resolver;
      resolver.setPolicy(policy);
      const std::vector<SnapTarget> targets{{ObjectId::fromUint64(7),{100,200,140,240}}};
      const float enter = policy.engageLogicalPx;
      auto actual = resolver.resolve({{20,20,100-enter+0.25F,70},4,4},targets,dpr);
      assert(actual.x && "logical enter must not shrink with DPR");
      actual = resolver.resolve({{20,20,100-policy.releaseLogicalPx+0.25F,70},4,4},targets,dpr);
      assert(actual.x && "logical release must not shrink with DPR");
      actual = resolver.resolve({{20,20,100-policy.releaseLogicalPx-0.25F,70},4,4},targets,dpr);
      assert(!actual.x);
    }
  }
}

void changed_target_at_up_cannot_commit_stale_snap() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256,256));
  canvas::semantic::ObjectRecord target{};
  target.id=ObjectId::fromUint64(7);
  target.kind=canvas::semantic::ObjectKind::kShape; target.kind_version=1;
  target.placement.order_key=canvas::semantic::OrderKey{{0x20}};
  target.transform.tx=100;target.transform.ty=180;
  target.content=canvas::semantic::ShapeContent{1,40,40};
  assert(Access::applyRemote(host,canvas::semantic::InsertObjectsOp{{target}}));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  const PointerKey key{11,42,1};
  assert(host.creationPointer(key,canvas::input::PointerPhase::kDown,20,20));
  assert(host.creationPointer(key,canvas::input::PointerPhase::kMove,97,80));
  assert(host.creationPreview().snapped);
  assert(Access::applyRemote(host,canvas::semantic::DeleteObjectsOp{{target.id}}));
  const auto count=host.submittedOperationCount();
  const auto generation=host.semanticGeneration();
  (void)host.creationPointer(key,canvas::input::PointerPhase::kUp,97,80);
  assert(host.submittedOperationCount()==count && host.semanticGeneration()==generation &&
         "Up must reject if snap targets changed after last preview");
}

void indexed_scene_query_matches_linear_visible_shape_reference() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  std::vector<canvas::semantic::ObjectRecord> records;
  for (std::uint64_t i = 0; i != 5U; ++i) {
    canvas::semantic::ObjectRecord record{};
    record.id = ObjectId::fromUint64(100U + i);
    record.kind = canvas::semantic::ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement.order_key = canvas::semantic::OrderKey{{1U, static_cast<std::uint8_t>(i + 1U)}};
    record.transform.tx = static_cast<double>(i * 80U);
    record.transform.ty = static_cast<double>(i * 20U);
    record.content = canvas::semantic::ShapeContent{1U, 24.0, 24.0};
    if (i == 3U) record.properties.entries.push_back({1U, false});
    records.push_back(record);
  }
  assert(Access::applyRemote(host, canvas::semantic::InsertObjectsOp{records}));
  const WorldRect query{0.0F, 0.0F, 160.0F, 120.0F};
  const auto indexed = Access::query(host, query);
  assert(indexed);
  std::vector<ObjectId> expected;
  for (const auto& record : Access::sceneRecords(host)) {
    if (record.kind == canvas::SceneObjectKind::kShape &&
        (static_cast<std::uint32_t>(record.flags) &
         static_cast<std::uint32_t>(canvas::SceneRecordFlags::kVisible)) != 0U &&
        record.worldBounds.intersects(query)) {
      expected.push_back(record.objectId);
    }
  }
  auto actual = indexed.value().backToFront;
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  assert(actual == expected);
  assert(indexed.value().diagnostics.candidatesExamined >= actual.size());
}

void invalid_targets_and_two_view_preview_are_isolated() {
  const PointerKey key{11,42,1};
  CreationSession a,b;
  const float nan=std::numeric_limits<float>::quiet_NaN();
  a.setSnapTargets({{ObjectId::fromUint64(7),{nan,20,140,80}},
                    {ObjectId::fromUint64(8),{140,20,80,80}},
                    {ObjectId::fromUint64(9),{80,20,140,80},true,false},
                    {ObjectId::fromUint64(10),{80,20,140,80},false,true}});
  b.setSnapTargets({{ObjectId::fromUint64(12),{80,20,140,80}}});
  assert(a.begin(key,CreationPreset::kTriangle,{}, {20,20},{0,0,256,256},100));
  assert(b.begin(key,CreationPreset::kArrow,{}, {20,20},{0,0,256,256},200));
  assert(a.move(key,{77,80}) && b.move(key,{77,80}));
  assert(!a.preview().snapped && b.preview().snapped);
  const auto before=b.preview().record;
  assert(a.cancel(key));
  assert(b.active() && b.preview().record==before);
  assert(b.finish(key)->record==before);
}

void moved_target_refresh_and_camera_fence() {
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  for(bool cameraChange:{false,true}) {
    canvas::ink_playground::InkPlaygroundHost host;assert(host.bindSurface(256,256));
    auto target=authoredShape(7,{100,180,140,220});
    assert(Access::applyRemote(host,canvas::semantic::InsertObjectsOp{{target}}));
    assert(host.selectCreationPreset(CreationPreset::kRectangle));
    const PointerKey key{11,42,1};
    assert(host.creationPointer(key,canvas::input::PointerPhase::kDown,20,20));
    assert(host.creationPointer(key,canvas::input::PointerPhase::kMove,97,80));
    assert(host.creationPreview().snapped);
    if(cameraChange) assert(host.setViewportZoomAt(2,128,128));
    else {
      auto transform=target.transform;transform.tx=150;
      assert(Access::applyRemote(host,canvas::semantic::SetTransformsOp{{{target.id,transform}}}));
    }
    const auto generation=host.semanticGeneration();const auto count=host.submittedOperationCount();
    assert(!host.creationPointer(key,canvas::input::PointerPhase::kUp,97,80));
    assert(host.semanticGeneration()==generation && host.submittedOperationCount()==count);
  }
  canvas::ink_playground::InkPlaygroundHost host;assert(host.bindSurface(256,256));
  auto target=authoredShape(7,{100,180,140,220});
  assert(Access::applyRemote(host,canvas::semantic::InsertObjectsOp{{target}}));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  const PointerKey key{11,42,1};
  assert(host.creationPointer(key,canvas::input::PointerPhase::kDown,20,20));
  assert(host.creationPointer(key,canvas::input::PointerPhase::kMove,97,80));
  auto transform=target.transform;transform.tx=150;
  assert(Access::applyRemote(host,canvas::semantic::SetTransformsOp{{{target.id,transform}}}));
  assert(host.creationPointer(key,canvas::input::PointerPhase::kMove,147,80));
  const auto preview=host.creationPreview();assert(preview.snapped && preview.end.x==150);
  assert(host.creationPointer(key,canvas::input::PointerPhase::kUp,147,80));
  const auto objects=Access::objects(host);
  const auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& r){return r.id==preview.record.id;});
  assert(found!=objects.end());
  auto expected=preview.record;
  // Frozen PropertyBag normalization omits explicit opacity=1. This
  // literal reference does not invoke the production normalizer.
  std::erase_if(expected.properties.entries,[](const auto& entry) {
    const auto* value=std::get_if<float>(&entry.value);
    return entry.field_id==3 && value && *value==1;
  });
  assert(*found==expected);
}

void creation_preview_and_final_commit_are_identical() {
  CreationSession session;
  const PointerKey key{11U, 42U, 1U};
  session.setSnapPolicy({6.0F, 10.0F});
  session.setSnapTargets({{ObjectId::fromUint64(7U), {80.0F, 20.0F, 140.0F, 80.0F}, false, false}});
  assert(session.begin(key, CreationPreset::kRectangle, CreationStyle{}, {20.0F, 20.0F},
                       {0.0F, 0.0F, 256.0F, 256.0F}, 99U));
  assert(session.move(key, {74.0F, 80.0F}));
  const auto preview = session.preview();
  assert(preview.active && preview.valid && preview.snapped);
  const auto commit = session.finish(key);
  assert(commit.has_value());
  assert(commit->record == preview.record);
}

void hundred_move_preview_has_zero_canonical_side_effects() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  const auto generation = host.semanticGeneration();
  const auto operations = host.submittedOperationCount();
  const auto scene=canvas::ink_playground::InkPlaygroundHistoryTestAccess::runtimeRecords(host);
  const auto damage=canvas::ink_playground::InkPlaygroundHistoryTestAccess::invalidation(host);
  const PointerKey key{11U, 42U, 1U};
  assert(host.creationPointer(key, canvas::input::PointerPhase::kDown, 20.0F, 20.0F));
  for (std::uint64_t i = 0; i < 100U; ++i) {
    assert(host.creationPointer(key, canvas::input::PointerPhase::kMove,
                               60.0F + static_cast<float>(i), 80.0F));
    assert(host.semanticGeneration() == generation);
    assert(host.submittedOperationCount() == operations);
    assert(host.semanticObjectCount() == 0U);
    assert(canvas::ink_playground::InkPlaygroundHistoryTestAccess::runtimeRecords(host)==scene);
    const auto currentDamage=canvas::ink_playground::InkPlaygroundHistoryTestAccess::invalidation(host);
    assert(currentDamage.generation==damage.generation && currentDamage.rects.size()==damage.rects.size());
    assert(!host.operationOutcome().applied);
  }
  assert(host.creationPointer(key, canvas::input::PointerPhase::kCancel, 160.0F, 80.0F));
  assert(host.semanticGeneration() == generation);
  assert(host.submittedOperationCount() == operations);
}

}  // namespace

int main(int argc,char** argv) {
  if(argc>1 && std::string_view(argv[1])=="stale"){changed_target_at_up_cannot_commit_stale_snap();return 0;}
  std::cerr<<"TRACE logical_threshold_is_dpr_invariant\n"<<std::flush;
  logical_threshold_is_dpr_invariant();
  std::cerr<<"TRACE indexed_scene_query_matches_linear_visible_shape_reference\n"<<std::flush;
  indexed_scene_query_matches_linear_visible_shape_reference();
  std::cerr<<"TRACE creation_session_indexed_reference_camera_matrix\n"<<std::flush;
  creation_session_indexed_reference_camera_matrix();
  std::cerr<<"TRACE creation_preview_and_final_commit_are_identical\n"<<std::flush;
  creation_preview_and_final_commit_are_identical();
  std::cerr<<"TRACE hundred_move_preview_has_zero_canonical_side_effects\n"<<std::flush;
  hundred_move_preview_has_zero_canonical_side_effects();
  std::cerr<<"TRACE changed_target_at_up_cannot_commit_stale_snap\n"<<std::flush;
  changed_target_at_up_cannot_commit_stale_snap();
  std::cerr<<"TRACE invalid_targets_and_two_view_preview_are_isolated\n"<<std::flush;
  invalid_targets_and_two_view_preview_are_isolated();
  std::cerr<<"TRACE moved_target_refresh_and_camera_fence\n"<<std::flush;
  moved_target_refresh_and_camera_fence();
  std::cout<<"PASS E04 invalid/selected/transient targets excluded; two sessions isolated; moved target refresh and camera/target Up fences\n";
  return 0;
}
