#include "canvas/semantic/operation.hpp"
#include <gtest/gtest.h>
#include "canvas/semantic/table_content.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <limits>
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/runtime/runtime_composition.hpp"
namespace {
using namespace canvas::semantic;
ObjectId id(std::uint64_t n){return ObjectId::fromUint64(n);}
ObjectRecord fixture(){
  TableContent table;table.rows={{id(100),48},{id(101),60}};table.columns={{id(200),120},{id(201),160}};
  for(std::size_t i=0;i<4;++i){TableCell c;c.text.layout=RichTextLayout{table.columns[i%2].size-12};table.cells.push_back(c);}
  ObjectRecord object;object.id=id(10);object.kind=ObjectKind::kTable;object.kind_version=1;object.placement.order_key=OrderKey({128});object.content=table;return object;
}
Operation operation(std::uint64_t n,OperationPayload payload){return {OperationId{id(n)},DocumentId{id(1)},1,1,std::move(payload)};}
}
TEST(TableV1, RecognizesAcceptedObjectAndOperation) {
  EXPECT_TRUE(canvas::semantic::isKnownObjectKind(static_cast<canvas::semantic::ObjectKind>(11)));
  EXPECT_TRUE(canvas::semantic::isKnownOperationKind(static_cast<canvas::semantic::OperationKind>(20)));
}
TEST(TableV1, WirePreservesStableAxesStylesAndSizes){
  const auto object=fixture();const auto op=operation(2,InsertObjectsOp{{object}});
  const auto encoded=SemanticCodec::encodeProtobufOperation(op);ASSERT_TRUE(encoded.ok());
  const auto decoded=SemanticCodec::decodeProtobufOperation(encoded.bytes);ASSERT_TRUE(decoded.ok());
  EXPECT_EQ(std::get<InsertObjectsOp>(decoded.operation.payload).objects.front(),object);
  auto next=std::get<TableContent>(object.content);next.rows[0].size=72;
  const auto edit=operation(3,EditTableOp{object.id,TableEditKind::kResize,std::get<TableContent>(object.content),next});
  const auto wire=SemanticCodec::encodeProtobufOperation(edit);ASSERT_TRUE(wire.ok());
  const auto read=SemanticCodec::decodeProtobufOperation(wire.bytes);ASSERT_TRUE(read.ok());EXPECT_EQ(read.operation.payload,edit.payload);
}
TEST(TableV1, InvalidGeometryIdentityAndVersionRejectBeforeEncoding){
  for(int scenario=0;scenario<7;++scenario){auto object=fixture();auto& t=std::get<TableContent>(object.content);
    if(scenario==0)t.rows.clear();if(scenario==1)t.columns[0].size=std::numeric_limits<double>::quiet_NaN();
    if(scenario==2)t.rows[1].id=t.rows[0].id;if(scenario==3)t.cells.pop_back();
    if(scenario==4)t.grid.version=2;if(scenario==5)t.cells[0].fill.a=2;if(scenario==6)t.cells[0].text.layout->wrap_width=99;
    EXPECT_FALSE(SemanticCodec::encodeProtobufOperation(operation(2,InsertObjectsOp{{object}})).ok())<<scenario;
  }
}
TEST(TableV1, ConditionalEditsAreAtomicAndDuplicatesDoNotAdvance){
  IndexedObjectStore store;AppliedOperationLedger ledger;SemanticGenerationState generation;CanonicalCommitClock clock{RuntimeEpoch{1}};OperationEngine engine;
  const auto object=fixture();const auto insert=operation(2,InsertObjectsOp{{object}});
  ASSERT_EQ(engine.apply(insert,ApplySource::kRemoteSync,store,ledger,generation,clock).disposition,ApplyDisposition::kApplied);
  const auto before=std::get<TableContent>(object.content);auto after=before;after.rows[0].size=96;
  const auto edit=operation(3,EditTableOp{object.id,TableEditKind::kResize,before,after});
  ASSERT_EQ(engine.apply(edit,ApplySource::kRemoteSync,store,ledger,generation,clock).disposition,ApplyDisposition::kApplied);
  EXPECT_EQ(engine.apply(edit,ApplySource::kRemoteSync,store,ledger,generation,clock).disposition,ApplyDisposition::kAlreadyApplied);
  auto conflict=edit;conflict.id=OperationId{id(4)};
  EXPECT_EQ(engine.apply(conflict,ApplySource::kRemoteSync,store,ledger,generation,clock).disposition,ApplyDisposition::kRejected);
  EXPECT_EQ(generation.current().value(),2);EXPECT_EQ(std::get<TableContent>(store.find(object.id)->content),after);
}
TEST(TableV1, TextAndPaintOperationsCannotSmuggleStructuralChanges){
  const auto object=fixture();const auto before=std::get<TableContent>(object.content);auto after=before;after.rows[0].size=90;
  for(auto kind:{TableEditKind::kText,TableEditKind::kCellStyle,TableEditKind::kGridStyle})
    EXPECT_FALSE(SemanticCodec::encodeProtobufOperation(operation(4,EditTableOp{object.id,kind,before,after})).ok());
}
TEST(TableRuntime, CommandsHistoryAndSnapshotPreserveIdentities){
  using namespace canvas;
  runtime::RuntimeComposition owner;ASSERT_TRUE(owner.openDocument(id(1)));ASSERT_TRUE(owner.bindSurface(800,600));auto& api=owner.controlFacade();
  runtime::TableRequest request;request.action=runtime::TableAction::kCreate;request.rows=2;request.columns=2;
  const auto created=api.tableCommand(request);ASSERT_TRUE(created.applied);
  const auto initial=api.readTable(created.object);ASSERT_TRUE(initial.available);ASSERT_EQ(initial.content.cells.size(),4);
  request={};request.action=runtime::TableAction::kColumnWidth;request.object=created.object;request.column=initial.content.columns[1].id;request.generation=created.generation;request.size=180;
  ASSERT_TRUE(api.tableCommand(request).applied);const auto resized=api.readTable(created.object);EXPECT_EQ(resized.content.columns[1].size,180);
  const auto history=[&](bool undo,std::uint64_t seq){runtime::CanvasControlRequest r;r.target=api.mountedCanvasTargets().front();r.clientId=76;r.requestId=seq;r.payload.kind=undo?runtime::CanvasControlPayloadKind::kUndo:runtime::CanvasControlPayloadKind::kRedo;EXPECT_EQ(api.submitCanvasControl(r).state,runtime::ProductControlState::kQueued);EXPECT_TRUE(owner.ownerPump());const auto receipt=api.canvasControlReceipt({76,seq});ASSERT_TRUE(receipt);EXPECT_EQ(receipt->state,runtime::CanvasControlReceiptState::kApplied);};
  history(true,1);EXPECT_EQ(api.readTable(created.object).content,initial.content);history(false,2);EXPECT_EQ(api.readTable(created.object).content,resized.content);
  request={};request.action=runtime::TableAction::kInsertRow;request.object=created.object;request.index=0;request.generation=api.readTable(created.object).generation;ASSERT_TRUE(api.tableCommand(request).applied);
  const auto inserted=api.readTable(created.object);history(true,3);EXPECT_EQ(api.readTable(created.object).content,resized.content);history(false,4);EXPECT_EQ(api.readTable(created.object).content,inserted.content);
  auto current=api.readTable(created.object);EXPECT_EQ(current.content.rows[1].id,initial.content.rows[0].id);EXPECT_EQ(current.content.columns[1].id,initial.content.columns[1].id);
  request={};request.action=runtime::TableAction::kCellStyle;request.object=created.object;request.row=initial.content.rows[0].id;request.column=initial.content.columns[1].id;request.fill=std::array<float,4>{1,0,0,1};request.generation=current.generation;ASSERT_TRUE(api.tableCommand(request).applied);
  current=api.readTable(created.object);EXPECT_EQ(current.content.cells[3].fill,(ColorValue{1,0,0,1}));
  const auto stamp=current.generation;request.generation=stamp-1;EXPECT_FALSE(api.tableCommand(request).applied);EXPECT_EQ(api.readTable(created.object).generation,stamp);
  runtime::DocumentSnapshotRequest capture;capture.expectedSemanticGeneration=stamp;capture.documentRevision=7;capture.recoveryFrontier={8,1,16,1,24,7};
  const auto saved=owner.captureDocumentSnapshot(capture);ASSERT_EQ(saved.status,runtime::DocumentSnapshotStatus::kCaptured);
  EXPECT_FALSE(SnapshotCodec::decode(saved.bytes,SnapshotCapability::kAllWithBackgroundV1).ok());
  const auto decoded=SnapshotCodec::decode(saved.bytes,SnapshotCapability::kAllWithTableV1);ASSERT_TRUE(decoded.ok());
  runtime::DocumentRestoreRequest restore;restore.bytes=saved.bytes;restore.expectedDocumentId=id(1);restore.documentRevision=7;restore.recoveryFrontier=capture.recoveryFrontier;restore.checkpointDigest=decoded.snapshot->checkpoint_digest;restore.resourceBinding=97;
  runtime::RuntimeComposition reopened;ASSERT_EQ(reopened.openDocumentFromSnapshot(restore),runtime::DocumentRestoreStatus::kRestored);ASSERT_TRUE(reopened.bindSurface(800,600));
  const auto recovered=reopened.controlFacade().readTable(created.object);EXPECT_EQ(recovered.content,current.content);
  request={};request.action=runtime::TableAction::kInsertColumn;request.object=created.object;request.index=1;request.generation=recovered.generation;
  ASSERT_TRUE(reopened.controlFacade().tableCommand(request).applied);const auto later=reopened.controlFacade().readTable(created.object);EXPECT_EQ(later.content.columns[2].id,current.content.columns[1].id);EXPECT_NE(later.content.columns[1].id,current.content.columns[0].id);
}
TEST(TableRuntime, CreationRespectsCurrentGroupScope){
  using namespace canvas;runtime::RuntimeComposition owner;ASSERT_TRUE(owner.openDocument(id(1)));ASSERT_TRUE(owner.bindSurface(800,600));auto& api=owner.controlFacade();
  runtime::TableRequest create;auto a=api.tableCommand(create);ASSERT_TRUE(a.applied);create.generation=a.generation;create.x=400;auto b=api.tableCommand(create);ASSERT_TRUE(b.applied);
  const auto grouped=api.groupObjects({a.object,b.object},id(900),1,80);ASSERT_EQ(grouped.state,runtime::ProductControlState::kQueued);ASSERT_TRUE(owner.ownerPump());ASSERT_EQ(api.canvasControlReceipt({80,1})->state,runtime::CanvasControlReceiptState::kApplied);
  runtime::SectionIntent enter;enter.action=runtime::SectionAction::kEnterGroupScope;enter.objectId=id(900);ASSERT_EQ(api.submitSectionIntent(enter,2,80).state,runtime::ProductControlState::kQueued);ASSERT_TRUE(owner.ownerPump());ASSERT_EQ(api.readSelectionSummary().groupScope,id(900));
  create.generation=api.readTable(a.object).generation;create.x=80;create.y=90;const auto c=api.tableCommand(create);ASSERT_TRUE(c.applied);
  runtime::DocumentSnapshotRequest capture;capture.expectedSemanticGeneration=c.generation;capture.documentRevision=7;capture.recoveryFrontier={8,1,16,1,24,7};const auto bytes=owner.captureDocumentSnapshot(capture);ASSERT_EQ(bytes.status,runtime::DocumentSnapshotStatus::kCaptured);const auto decoded=SnapshotCodec::decode(bytes.bytes,SnapshotCapability::kAllWithTableV1);ASSERT_TRUE(decoded.ok());const auto& objects=decoded.snapshot->objects;const auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==c.object;});ASSERT_NE(found,objects.end());EXPECT_EQ(found->placement.parent_id,id(900));
}
