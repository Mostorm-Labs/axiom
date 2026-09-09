#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/semantic/normalizer.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/operation.pb.h"
#include <array>
#include <string>
#endif

namespace canvas::semantic {
#if defined(CANVAS_SEMANTIC_PROTOBUF)
namespace { namespace p = auditoryworks::axiom::v1;
void id(p::Id128* x, std::uint8_t s) { x->set_value(std::string(16, static_cast<char>(s))); }
void vec(p::Vec2* x, double a, double b) { x->set_x(a); x->set_y(b); }
void tr(p::Transform2D* x) { x->set_a(1); x->set_b(0); x->set_c(0); x->set_d(1); x->set_tx(3); x->set_ty(4); }
void place(p::Placement* x) { x->mutable_order_key()->set_value("A"); }
void obj(p::ObjectRecord* x, std::uint8_t s) { id(x->mutable_id(),s); x->set_kind_id(1); x->set_kind_version(1); place(x->mutable_placement()); tr(x->mutable_transform()); x->mutable_properties(); x->mutable_content()->mutable_shape()->set_shape_kind(2); x->mutable_content()->mutable_shape()->set_width(11); x->mutable_content()->mutable_shape()->set_height(12); }
void wire(p::Operation* o, std::uint8_t v) { id(o->mutable_operation_id(),1); id(o->mutable_document_id(),2); o->set_schema_version(3); o->set_payload_version(4); switch(v) {
case 1: obj(o->mutable_payload()->mutable_insert_objects()->add_objects(),9); break;
case 2: id(o->mutable_payload()->mutable_delete_objects()->add_object_ids(),3); break;
case 3: obj(o->mutable_payload()->mutable_restore_objects()->add_objects(),4); break;
case 4: {auto*x=o->mutable_payload()->mutable_set_placements()->add_items();id(x->mutable_object_id(),5);place(x->mutable_placement());break;}
case 5: {auto*x=o->mutable_payload()->mutable_set_transforms()->add_items();id(x->mutable_object_id(),6);tr(x->mutable_transform());break;}
case 6: {auto*x=o->mutable_payload()->mutable_patch_properties()->add_patches();id(x->mutable_object_id(),7);x->set_field_id(8);x->set_action(p::PROPERTY_PATCH_SET);x->mutable_value()->set_bool_value(true);break;}
case 7: {auto*x=o->mutable_payload()->mutable_set_object_size()->add_items();id(x->mutable_object_id(),8);x->set_width(21);x->set_height(22);break;}
case 8: {auto*x=o->mutable_payload()->mutable_set_vector_path_geometry();id(x->mutable_object_id(),9);auto*g=x->mutable_geometry();g->set_fill_rule(p::FILL_RULE_NON_ZERO);vec(g->add_commands()->mutable_move_to()->mutable_point(),1,2);break;}
case 9: {auto*x=o->mutable_payload()->mutable_set_image_content();id(x->mutable_object_id(),10);auto*c=x->mutable_content();id(c->mutable_resource_id(),11);c->set_intrinsic_width(31);c->set_intrinsic_height(32);c->set_content_mode(p::IMAGE_CONTENT_MODE_FIT);c->set_width(33);c->set_height(34);break;}
case 10: {auto*x=o->mutable_payload()->mutable_add_stroke()->mutable_object();obj(x,12);x->set_kind_id(5);break;}
case 11: {auto*x=o->mutable_payload()->mutable_split_strokes()->add_splits();id(x->mutable_source_stroke_id(),13);obj(x->add_replacements(),14);break;}
case 12: {auto*x=o->mutable_payload()->mutable_add_erase_masks()->add_items();id(x->mutable_object_id(),15);auto*m=x->add_masks();id(m->mutable_mask_id(),16);auto*g=m->mutable_geometry()->mutable_filled_path()->mutable_path();g->set_fill_rule(p::FILL_RULE_NON_ZERO);vec(g->add_commands()->mutable_move_to()->mutable_point(),6,7);break;}
case 13: {auto*x=o->mutable_payload()->mutable_remove_erase_masks()->add_items();id(x->mutable_object_id(),17);id(x->add_mask_ids(),18);break;}
case 14: {auto*x=o->mutable_payload()->mutable_edit_rich_text();id(x->mutable_object_id(),19);auto*s=x->mutable_delta()->add_steps()->mutable_insert_text();id(s->mutable_paragraph_id(),20);s->set_text("hi");s->mutable_style()->set_font_size(12);s->mutable_style()->set_weight(400);s->mutable_style()->set_italic(false);s->mutable_style()->set_underline(false);s->mutable_style()->mutable_color()->set_r(1);s->mutable_style()->mutable_color()->set_g(1);s->mutable_style()->mutable_color()->set_b(1);s->mutable_style()->mutable_color()->set_a(1);break;}
case 15: {auto*x=o->mutable_payload()->mutable_set_connector_content();id(x->mutable_object_id(),21);vec(x->mutable_content()->mutable_start()->mutable_free_point()->mutable_point(),1,1);vec(x->mutable_content()->mutable_end()->mutable_free_point()->mutable_point(),2,2);x->mutable_content()->set_routing(p::CONNECTOR_ROUTING_STRAIGHT);break;}
default: break; }}
TEST(CodecOperationIngress, DecodesIndependentNonEmptyWireRepresentativesForAllFamilies) { constexpr std::array<const char*,15> n{"InsertObjects","DeleteObjects","RestoreObjects","SetPlacements","SetTransforms","PatchProperties","SetObjectSize","SetVectorPathGeometry","SetImageContent","AddStroke","SplitStrokes","AddEraseMasks","RemoveEraseMasks","EditRichText","SetConnectorContent"}; for(std::uint8_t v=1;v<=15;++v){p::Operation w;wire(&w,v);std::string b;ASSERT_TRUE(w.SerializeToString(&b))<<n[v-1];auto d=SemanticCodec::decodeProtobufOperation({b.begin(),b.end()});ASSERT_EQ(d.error,SemanticError::kNone)<<n[v-1];EXPECT_EQ(d.operation.id.value().bytes[0],1);EXPECT_EQ(d.operation.document_id.value().bytes[0],2);EXPECT_EQ(d.operation.schema_version,3U);EXPECT_EQ(d.operation.payload_version,4U);EXPECT_TRUE(d.presence.schema_version);EXPECT_TRUE(d.presence.payload_version);EXPECT_EQ(static_cast<unsigned>(d.operation.kind()),v);switch(v){case 1:EXPECT_EQ(std::get<InsertObjectsOp>(d.operation.payload).objects.front().id.bytes[0],9);break;case 2:EXPECT_EQ(std::get<DeleteObjectsOp>(d.operation.payload).object_ids.front().bytes[0],3);break;case 3:EXPECT_EQ(std::get<RestoreObjectsOp>(d.operation.payload).objects.front().id.bytes[0],4);break;case 4:EXPECT_EQ(std::get<SetPlacementsOp>(d.operation.payload).items.front().object_id.bytes[0],5);break;case 5:EXPECT_DOUBLE_EQ(std::get<SetTransformsOp>(d.operation.payload).items.front().transform.tx,3);break;case 6:EXPECT_EQ(std::get<PatchPropertiesOp>(d.operation.payload).patches.front().field_id,8U);break;case 7:EXPECT_DOUBLE_EQ(std::get<SetObjectSizeOp>(d.operation.payload).items.front().width,21);break;case 8:EXPECT_EQ(std::get<SetVectorPathGeometryOp>(d.operation.payload).geometry.commands.size(),1U);break;case 9:EXPECT_DOUBLE_EQ(std::get<SetImageContentOp>(d.operation.payload).content.width,33);break;case 10:EXPECT_EQ(std::get<AddStrokeOp>(d.operation.payload).object.id.bytes[0],12);break;case 11:EXPECT_EQ(std::get<SplitStrokesOp>(d.operation.payload).splits.front().replacements.front().id.bytes[0],14);break;case 12:EXPECT_EQ(std::get<AddEraseMasksOp>(d.operation.payload).items.front().masks.front().id.bytes[0],16);break;case 13:EXPECT_EQ(std::get<RemoveEraseMasksOp>(d.operation.payload).items.front().mask_ids.front().bytes[0],18);break;case 14:EXPECT_EQ(std::get<EditRichTextOp>(d.operation.payload).delta.steps.size(),1U);break;case 15:EXPECT_EQ(std::get<SetConnectorContentOp>(d.operation.payload).content.routing,ConnectorRouting::kStraight);break;}}}

TEST(CodecOperationIngress, DecodedOperationIsConsumableByOperationEngine) {
    p::Operation wire_operation; wire(&wire_operation, 1); std::string bytes; ASSERT_TRUE(wire_operation.SerializeToString(&bytes));
    const auto decoded = SemanticCodec::decodeProtobufOperation({bytes.begin(), bytes.end()});
    ASSERT_EQ(decoded.error, SemanticError::kNone);
    const auto normalized = normalizeOperation(decoded.operation);
    ASSERT_TRUE(normalized.ok());
    ReferenceObjectStore objects; AppliedOperationLedger ledger; SemanticGenerationState generation;
    CanonicalCommitClock clock(RuntimeEpoch(1)); OperationEngine engine;
    const auto result = engine.apply(normalized.value, ApplySource::kRestoreReplay, objects, ledger, generation, clock);
    EXPECT_EQ(result.disposition, ApplyDisposition::kApplied);
    EXPECT_EQ(objects.size(), 1U); EXPECT_EQ(generation.current(), SemanticGeneration(1));
}
}

#else
TEST(CodecOperationIngress, ProtobufUnavailable) { EXPECT_EQ(SemanticCodec::decodeProtobufOperation({}).error, SemanticError::kRuntimeUnavailable); }
#endif
}
