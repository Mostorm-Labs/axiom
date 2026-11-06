#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::interaction {
void CanvasEditingRuntime::cancelPlacementCreation() noexcept {
  if (placementCreation_) {
    placementCreation_.reset();
    transientOverlayDirty_=false;
    transientOverlayHandoffPending_=true;
    canonicalPresentationDirty_=true;
  }
  placementPointers_.clear();
}

bool CanvasEditingRuntime::placementPointer(const input::PointerKey& key,
    input::PointerPhase phase,float x,float y) noexcept {
  if (!key.valid()) return false;
  if (phase==input::PointerPhase::kDown) {
    placementPointers_.insert(key);
    if (placementPointers_.size()!=1) {
      placementCreation_.reset();transientOverlayDirty_=false;
      transientOverlayHandoffPending_=true;canonicalPresentationDirty_=true;
      return true;
    }
    if (!canonicalWriteAdmission() || !historySceneReady()) return false;
    const auto point=viewToContent(x,y);
    PlacementCreation draft;draft.key=key;draft.generation=semanticGeneration_.current().value();
    draft.camera=cameraGeneration();draft.metrics=surface_.metricsGeneration;
    draft.selection=selection_.revision();draft.scope=selection_.groupScope();
    draft.preview.active=true;draft.preview.valid=true;draft.preview.preset=creationPreset_;
    draft.preview.start={point.first,point.second};draft.preview.end=draft.preview.start;
    if(creationPreset_==CreationPreset::kTable) {
      draft.preview.record=validCreationHover(true)&&creationHover_->source.isZero()
          ?creationHover_->preview.primary:makeCreationPrototype(CreationPreset::kTable);
      if(const auto* table=std::get_if<semantic::TableContent>(&draft.preview.record.content)) {
        for(const auto& row:table->rows)draft.rows.push_back(row.id);
        for(const auto& column:table->columns)draft.columns.push_back(column.id);
      }
    } else {
      draft.preview.record.id=allocateTextIdentity();draft.preview.record.kind=semantic::ObjectKind::kSection;
      draft.preview.record.kind_version=1;materializeStructuralAppearance(draft.preview.record);
    }
    (void)clearCreationHover();
    if(draft.preview.record.id.isZero())return false;
    placementCreation_=std::move(draft);
  }
  const auto terminal=phase==input::PointerPhase::kUp||phase==input::PointerPhase::kCancel;
  if (terminal) placementPointers_.erase(key);
  if (!placementCreation_ || placementCreation_->key!=key) return true;
  auto& draft=*placementCreation_;
  if (phase==input::PointerPhase::kCancel ||
      draft.generation!=semanticGeneration_.current().value() || draft.camera!=cameraGeneration() ||
      draft.metrics!=surface_.metricsGeneration || draft.selection!=selection_.revision() ||
      draft.scope!=selection_.groupScope()) {
    cancelPlacementCreation();return true;
  }
  const auto point=viewToContent(x,y);
  const float dx=point.first-draft.preview.start.x,dy=point.second-draft.preview.start.y;
  if (std::max(std::abs(dx),std::abs(dy))*viewportGesture().scale>=4)draft.dragged=true;
  auto& record=draft.preview.record;
  double width=320,height=240;
  if (creationPreset_==CreationPreset::kTable) {
    const auto columns=std::uint32_t(std::clamp(std::ceil(std::abs(double(dx))/120),2.0,64.0));
    const auto rows=std::uint32_t(std::clamp(std::ceil(std::abs(double(dy))/48),2.0,double(std::min(64U,1024U/columns))));
    semantic::TableContent table;
    table.rows.resize(rows);table.columns.resize(columns);table.cells.resize(rows*columns);
    const auto reserve=[&](auto& ids,std::size_t count) {
      while(ids.size()<count) {
        const auto id=allocateTextIdentity();if(id.isZero())return false;
        ids.push_back(id);
      }
      return true;
    };
    if(!reserve(draft.rows,rows)||!reserve(draft.columns,columns)) {
      cancelPlacementCreation();return false;
    }
    for(std::size_t i=0;i<rows;++i)table.rows[i]={draft.rows[i],48};
    for(std::size_t i=0;i<columns;++i)table.columns[i]={draft.columns[i],120};
    // Keep identities across growth/shrink and commit: hand-drawn shared grid
    // lines derive their sequence from the axis ID as well as the style seed.
    // Nothing is published until the Table owner commits on Up.
    record.content=std::move(table);width=columns*120;height=rows*48;
  } else {
    if(draft.dragged){width=std::max(24.0,double(std::abs(dx)));height=std::max(24.0,double(std::abs(dy)));}
    record.content=semantic::SectionContent{{0,0,width,height},"Section",16,32};
  }
  record.transform.tx=draft.preview.start.x-(draft.dragged&&dx<0?width:0);
  record.transform.ty=draft.preview.start.y-(draft.dragged&&dy<0?height:0);
  draft.preview.end={point.first,point.second};
  canonicalPresentationDirty_=true;
  if (!terminal) {
    (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    return true;
  }
  const auto completed=std::move(draft);
  placementCreation_.reset();transientOverlayDirty_=false;transientOverlayHandoffPending_=true;
  if (creationPreset_==CreationPreset::kTable) {
    const auto& table=std::get<semantic::TableContent>(completed.preview.record.content);
    runtime::TableRequest request;request.rows=table.rows.size();request.columns=table.columns.size();
    request.x=completed.preview.record.transform.tx;request.y=completed.preview.record.transform.ty;
    request.generation=completed.generation;
    return createTableObject(request,&completed.preview.record).applied;
  }
  runtime::SectionIntent request;request.objectId=completed.preview.record.id;
  request.content=std::get<semantic::SectionContent>(completed.preview.record.content);
  request.transform=completed.preview.record.transform;request.parentId=completed.scope;
  request.expectedSemanticGeneration=completed.generation;request.selectionRevision=completed.selection;
  if (completed.scope) {
    const auto* parent=sceneCoordinator_->runtimeScene().find(*completed.scope);
    semantic::Transform2D inverse;
    if(!parent||!textWritable(*completed.scope)||!scene::inverseTransform(parent->transform,inverse))return false;
    request.transform=scene::composeTransform(inverse,request.transform);
  }
  const auto result=executeSectionCommand(request,nullptr,&completed.preview.record.properties);
  if(result.canonical!=runtime::SectionCanonicalOutcome::kCommitted)return false;
  selectCreatedObject(result.objectId);
  return true;
}
} // namespace canvas::interaction
