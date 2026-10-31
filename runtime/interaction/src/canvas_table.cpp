#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/semantic/table_content.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::interaction {
namespace {
std::optional<std::pair<double,double>> localPoint(const semantic::Transform2D& t,double x,double y){
  const auto det=t.a*t.d-t.b*t.c;if(!std::isfinite(det)||std::abs(det)<1e-12)return {};
  x-=t.tx;y-=t.ty;return std::pair{(t.d*x-t.c*y)/det,(t.a*y-t.b*x)/det};
}
void widths(semantic::TableContent& t){for(std::size_t i=0;i<t.cells.size();++i)t.cells[i].text.layout=semantic::RichTextLayout{t.columns[i%t.columns.size()].size-12};}
}
runtime::TableSnapshot CanvasEditingRuntime::tableSnapshot(semantic::ObjectId id) const {
  runtime::TableSnapshot out;if(std::this_thread::get_id()!=ownerThread_||closed_||!historySceneReady())return out;
  if(id.isZero()){if(selectedObjectCount()!=1)return out;id=selection_.primary();}
  const auto* record=semanticObjects_.find(id);if(!record||record->kind!=semantic::ObjectKind::kTable)return out;
  out.available=true;out.editable=textWritable(id);out.object=id;out.generation=semanticGeneration_.current().value();out.content=std::get<semantic::TableContent>(record->content);
  if(id==tableObject_&&semantic::tableCellIndex(out.content,tableRow_,tableColumn_)){out.row=tableRow_;out.column=tableColumn_;}
  else {out.row=out.content.rows.front().id;out.column=out.content.columns.front().id;}
  return out;
}
runtime::TableResult CanvasEditingRuntime::tableCommand(const runtime::TableRequest& request){
  using A=runtime::TableAction;using namespace semantic;
  const auto failure=runtime::TableResult{};
  if(std::this_thread::get_id()!=ownerThread_||closed_||!historySceneReady()||!canonicalWriteAdmission())return failure;
  if(tableResize_||selectionTransformActive()||dispatchRuntime_.activeCount()!=0||creationActive()||request.generation!=semanticGeneration_.current().value())return failure;
  if(auto* editor=interaction_->textSession();editor&&editor->read().phase!=runtime::TextPhase::kClosed){
    const auto finish=editor->finish();if(finish==runtime::TextStatus::kBusy||finish==runtime::TextStatus::kRejected||editor->read().phase!=runtime::TextPhase::kClosed)return failure;
  }
  if(request.action==A::kCreate){
    if(request.rows<1||request.rows>64||request.columns<1||request.columns>64||request.rows*request.columns>1024||!std::isfinite(request.x)||!std::isfinite(request.y))return failure;
    ObjectRecord object;object.id=allocateTextIdentity();object.kind=ObjectKind::kTable;object.kind_version=1;object.transform.tx=request.x;object.transform.ty=request.y;
    object.placement.parent_id=selection_.groupScope();
    if(object.placement.parent_id){const auto* parent=sceneCoordinator_->runtimeScene().find(*object.placement.parent_id);Transform2D inverse;if(!parent||!textWritable(parent->objectId)||!scene::inverseTransform(parent->transform,inverse))return failure;object.transform=scene::composeTransform(inverse,object.transform);}
    std::vector<std::uint8_t> key;for(const auto& existing:semanticObjects_.allObjects())if(existing.placement.parent_id==object.placement.parent_id&&(key.empty()||OrderKey(key)<existing.placement.order_key))key.assign(existing.placement.order_key.bytes().begin(),existing.placement.order_key.bytes().end());
    if(key.size()>=OrderKey::kMaxSize)return failure;key.push_back(128);object.placement.order_key=OrderKey(key);
    TableContent t;for(std::uint32_t i=0;i<request.rows;++i)t.rows.push_back({allocateTextIdentity(),48});for(std::uint32_t i=0;i<request.columns;++i)t.columns.push_back({allocateTextIdentity(),120});t.cells.resize(request.rows*request.columns);widths(t);object.content=t;
    const auto f=allocateOperationId(),u=allocateOperationId();
    if(!applyLocalHistoryOperation({f,documentId_,1,1,InsertObjectsOp{{object}}},{u,documentId_,1,1,DeleteObjectsOp{{object.id}}}))return failure;
    (void)selection_.click(object.id);tableObject_=object.id;tableRow_=t.rows.front().id;tableColumn_=t.columns.front().id;canonicalPresentationDirty_=true;
    return {true,object.id,semanticGeneration_.current().value()};
  }
  const auto* object=semanticObjects_.find(request.object.isZero()?selection_.primary():request.object);if(!object||object->kind!=ObjectKind::kTable||!textWritable(object->id))return failure;
  const auto before=std::get<TableContent>(object->content);auto next=before;
  const auto index=tableCellIndex(before,request.row,request.column);
  if(request.action==A::kSelect){if(!index)return failure;(void)selection_.click(object->id);tableObject_=object->id;tableRow_=request.row;tableColumn_=request.column;canonicalPresentationDirty_=true;return {true,object->id,semanticGeneration_.current().value()};}
  if(request.action==A::kNextCell||request.action==A::kPreviousCell){const auto active=tableSnapshot(object->id);auto i=tableCellIndex(before,active.row,active.column).value_or(0);if(request.action==A::kNextCell&&i+1<before.cells.size())++i;if(request.action==A::kPreviousCell&&i>0)--i;tableObject_=object->id;tableRow_=before.rows[i/before.columns.size()].id;tableColumn_=before.columns[i%before.columns.size()].id;canonicalPresentationDirty_=true;return {true,object->id,semanticGeneration_.current().value()};}
  auto action=TableEditKind::kCellStyle;
  if(request.action==A::kInsertRow||request.action==A::kInsertColumn){
    const bool row=request.action==A::kInsertRow;auto& axes=row?next.rows:next.columns;
    if(request.index>axes.size()||axes.size()>=64||(row?(axes.size()+1)*next.columns.size():next.rows.size()*(axes.size()+1))>1024)return failure;
    const auto newId=allocateTextIdentity();axes.insert(axes.begin()+request.index,{newId,row?48.0:120.0});
    std::vector<TableCell> cells;for(std::size_t r=0;r<next.rows.size();++r)for(std::size_t c=0;c<next.columns.size();++c){if((row&&r==request.index)||(!row&&c==request.index))cells.emplace_back();else cells.push_back(before.cells[(r-(row&&r>request.index))*before.columns.size()+c-(!row&&c>request.index)]);}
    next.cells=std::move(cells);widths(next);action=TableEditKind::kInsert;
  }else if(request.action==A::kRemoveRow||request.action==A::kRemoveColumn){
    const bool row=request.action==A::kRemoveRow;auto& axes=row?next.rows:next.columns;const auto target=row?request.row:request.column;
    const auto found=std::find_if(axes.begin(),axes.end(),[&](const auto& a){return a.id==target;});if(found==axes.end()||axes.size()==1)return failure;const auto removed=static_cast<std::size_t>(found-axes.begin());axes.erase(found);
    std::vector<TableCell> cells;for(std::size_t r=0;r<before.rows.size();++r)for(std::size_t c=0;c<before.columns.size();++c)if(row?r!=removed:c!=removed)cells.push_back(before.cells[r*before.columns.size()+c]);next.cells=std::move(cells);action=TableEditKind::kRemove;
  }else if(request.action==A::kRowHeight||request.action==A::kColumnWidth){
    if(!std::isfinite(request.size)||request.size<24||request.size>16384)return failure;const bool row=request.action==A::kRowHeight;auto& axes=row?next.rows:next.columns;const auto target=row?request.row:request.column;
    const auto found=std::find_if(axes.begin(),axes.end(),[&](const auto& a){return a.id==target;});if(found==axes.end())return failure;found->size=request.size;widths(next);action=TableEditKind::kResize;
  }else if(request.action==A::kGridStyle){
    if(request.gridColor){const auto& c=*request.gridColor;next.grid.color={c[0],c[1],c[2],c[3]};}if(request.gridWidth)next.grid.width=*request.gridWidth;action=TableEditKind::kGridStyle;
  }else if(request.action==A::kCellStyle){
    if(!index)return failure;auto& cell=next.cells[*index];if(request.fill){const auto& c=*request.fill;cell.fill={c[0],c[1],c[2],c[3]};}
    const auto& patch=request.format;if(patch.italic||patch.underline||patch.strikethrough||patch.bulleted)return failure;
    if(patch.color||patch.fontSize||patch.font||patch.bold||patch.alignment){
      if(cell.text.document.paragraphs.empty()){
        const auto* manifest=textManifest();if(!manifest)return failure;const auto font=std::find_if(manifest->entries.begin(),manifest->entries.end(),[](const auto& e){return e.resource_kind==ResourceKind::kFont;});if(font==manifest->entries.end())return failure;
        Paragraph p;p.id=allocateTextIdentity();p.style={ParagraphAlignment::kLeft,1,0,0};p.runs.push_back({"",{font->resource_id,16,400,false,false,{0,0,0,1}}});cell.text.document.paragraphs.push_back(p);
      }
      for(auto& p:cell.text.document.paragraphs){if(patch.alignment)p.style.alignment=static_cast<ParagraphAlignment>(*patch.alignment);for(auto& run:p.runs){auto& style=run.style;if(patch.color){const auto& c=*patch.color;style.color={c[0],c[1],c[2],c[3]};}if(patch.fontSize)style.font_size=*patch.fontSize;if(patch.font)style.font_resource_id=ResourceId{*patch.font};if(patch.bold)style.weight=*patch.bold?700:400;}}
    }
  }else return failure;
  if(next==before)return {true,object->id,semanticGeneration_.current().value()};
  const auto id=object->id;const auto f=allocateOperationId(),u=allocateOperationId();
  if(!applyLocalHistoryOperation({f,documentId_,1,1,EditTableOp{id,action,before,next}},{u,documentId_,1,1,EditTableOp{id,action==TableEditKind::kInsert?TableEditKind::kRemove:action==TableEditKind::kRemove?TableEditKind::kInsert:action,next,before}}))return failure;
  canonicalPresentationDirty_=true;return {true,id,semanticGeneration_.current().value()};
}
bool CanvasEditingRuntime::tableTargetAt(runtime::TextIntent& intent){
  const auto* record=semanticObjects_.find(intent.object);if(!record||record->kind!=semantic::ObjectKind::kTable)return false;
  const auto* world=sceneCoordinator_->runtimeScene().find(record->id);if(!world)return false;const auto p=viewToContent(intent.viewX,intent.viewY);const auto local=localPoint(world->transform,p.first,p.second);if(!local)return false;
  const auto& t=std::get<semantic::TableContent>(record->content);for(std::size_t i=0;i<t.cells.size();++i){const auto r=semantic::tableCellRect(t,i);if(local->first>=r[0]&&local->first<=r[2]&&local->second>=r[1]&&local->second<=r[3]){intent.row=t.rows[i/t.columns.size()].id;intent.column=t.columns[i%t.columns.size()].id;tableObject_=record->id;tableRow_=intent.row;tableColumn_=intent.column;return true;}}return false;
}
bool CanvasEditingRuntime::tablePointer(const input::PointerKey& key,input::PointerPhase phase,float x,float y){
  if(tableResize_){if(tableResize_->key!=key)return true;auto drag=*tableResize_;const auto* world=sceneCoordinator_->runtimeScene().find(drag.object);const auto p=viewToContent(x,y);const auto local=world?localPoint(world->transform,p.first,p.second):std::nullopt;
    if(!local||semanticGeneration_.current().value()!=drag.generation){tableResize_.reset();canonicalPresentationDirty_=true;return true;}
    if(phase==input::PointerPhase::kMove||phase==input::PointerPhase::kUp){tableResize_->preview=std::clamp(drag.size+(drag.column.isZero()?local->second:local->first)-drag.start,24.0,16384.0);canonicalPresentationDirty_=true;}
    if(phase==input::PointerPhase::kUp){const auto size=tableResize_->preview;tableResize_.reset();runtime::TableRequest request;request.action=drag.column.isZero()?runtime::TableAction::kRowHeight:runtime::TableAction::kColumnWidth;request.object=drag.object;request.row=drag.row;request.column=drag.column;request.size=size;request.generation=drag.generation;(void)tableCommand(request);}
    if(phase==input::PointerPhase::kCancel){tableResize_.reset();canonicalPresentationDirty_=true;}return true;
  }
  if(phase!=input::PointerPhase::kDown||!selectionMode_||selectionAcquisitionMode_!=runtime::CanvasSelectionMode::kPoint||selectedObjectCount()!=1||!textWritable(selection_.primary()))return false;
  runtime::TextIntent target;target.object=selection_.primary();target.viewX=x;target.viewY=y;if(!tableTargetAt(target))return false;
  const auto* world=sceneCoordinator_->runtimeScene().find(target.object);const auto p=viewToContent(x,y);const auto local=localPoint(world->transform,p.first,p.second);const auto& t=std::get<semantic::TableContent>(world->content);const double tolerance=5/std::max(0.01F,viewportGesture().scale);
  double edge=0;for(const auto& col:t.columns){edge+=col.size;if(std::abs(local->first-edge)<tolerance){tableResize_=TableResize{key,target.object,{},col.id,local->first,col.size,col.size,semanticGeneration_.current().value()};return true;}}
  edge=0;for(const auto& row:t.rows){edge+=row.size;if(std::abs(local->second-edge)<tolerance){tableResize_=TableResize{key,target.object,row.id,{},local->second,row.size,row.size,semanticGeneration_.current().value()};return true;}}
  canonicalPresentationDirty_=true;return false;
}
}
