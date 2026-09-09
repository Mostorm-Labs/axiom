#include "g1_07_replay_corpus.hpp"

#include <optional>

namespace canvas::verification::g1_07 {
namespace {

using namespace canvas::semantic;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }
DocumentId document() { return DocumentId{id(0x706U)}; }

ObjectRecord shape(std::uint64_t value, std::optional<ObjectId> parent = std::nullopt,
                   std::uint8_t order = 1U) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kShape;
    record.kind_version = 1U;
    record.placement = Placement{parent, OrderKey({order})};
    record.transform = Transform2D{1.0, 0.0, 0.0, 1.0, static_cast<double>(value),
                                   static_cast<double>(value + 1U)};
    record.properties.entries = {{1U, PropertyValue{false}}};
    record.content = ShapeContent{1U, 10.0 + static_cast<double>(value),
                                  20.0 + static_cast<double>(value)};
    return record;
}

ObjectRecord group(std::uint64_t value) {
    ObjectRecord record{};
    record.id = id(value);
    record.kind = ObjectKind::kGroup;
    record.kind_version = 1U;
    record.placement = Placement{std::nullopt, OrderKey({1U})};
    record.content = GroupContent{};
    return record;
}

ObjectRecord vectorPath(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 2U);
    record.kind = ObjectKind::kVectorPath;
    record.content = VectorPathContent{VectorPathGeometry{
        FillRule::kNonZero, {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 1.0}}}}};
    return record;
}

ObjectRecord image(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 3U);
    record.kind = ObjectKind::kImage;
    record.content = ImageContent{ResourceId{id(800U)}, 10.0, 10.0, std::nullopt,
                                  ImageContentMode::kFit, 10.0, 10.0};
    return record;
}

ObjectRecord vectorStroke(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 4U);
    record.kind = ObjectKind::kVectorStroke;
    StrokeRecord stroke{};
    stroke.brush.brush_family_id = 1U;
    stroke.brush.brush_version = 1U;
    stroke.brush.nominal_size = 6.0;
    stroke.brush.opacity = 0.8F;
    stroke.deterministic_seed = 0x1234U;
    stroke.data = VectorStrokeData{{StrokeSample{Vec2{0.0, 0.0}, 1.0F, Vec2{0.0, 0.0}},
                                    StrokeSample{Vec2{1.0, 2.0}, 1.0F, Vec2{0.0, 0.0}}}};
    record.content = VectorStrokeContent{stroke};
    return record;
}

ObjectRecord richText(std::uint64_t value, ObjectId parent) {
    ObjectRecord record = shape(value, parent, 5U);
    record.kind = ObjectKind::kRichText;
    Paragraph paragraph{};
    paragraph.id = id(value * 10U);
    paragraph.style = ParagraphStyle{ParagraphAlignment::kCenter, 1.2, 0.1, 0.2};
    paragraph.runs = {TextRun{"replay inspector", TextStyle{}}};
    record.content = RichTextContent{RichTextDocument{{paragraph}}};
    return record;
}

ObjectRecord connector(std::uint64_t value, ObjectId target) {
    ObjectRecord record = shape(value, std::nullopt, 6U);
    record.kind = ObjectKind::kConnector;
    record.content = ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{target, StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{9.0, 9.0}}}, ConnectorRouting::kStraight};
    return record;
}

EraseMaskRecord mask(std::uint64_t value) {
    return EraseMaskRecord{id(value), SweptCircleMask{{EraseCubicSegment{
        EraseKnot{Vec2{0.0, 0.0}, 1.0}, EraseKnot{Vec2{1.0, 1.0}, 1.0},
        Vec2{0.5, 0.5}, Vec2{0.5, 0.5}}}}};
}

Operation operation(OperationPayload payload, std::uint64_t operation_id) {
    Operation result{};
    result.id = OperationId{id(operation_id)};
    result.document_id = document();
    result.schema_version = 1U;
    result.payload_version = 1U;
    result.payload = std::move(payload);
    return result;
}

} // namespace

ReplayTrace makeMinimumTrace() {
    ReplayTrace result;
    result.document_id = document();
    result.baseline.runtime_epoch = RuntimeEpoch(42U);
    return result;
}

ReplayTrace makeAllOperationFamiliesTrace() {
    const ObjectId root_id = id(100U);
    const ObjectRecord root = group(100U);
    const ObjectRecord shape_record = shape(101U, root_id);
    const ObjectRecord vector_path_record = vectorPath(102U, root_id);
    const ObjectRecord image_record = image(103U, root_id);
    const ObjectRecord vector_stroke_record = vectorStroke(104U, root_id);
    const ObjectRecord rich_text_record = richText(105U, root_id);
    const ObjectRecord connector_record = connector(106U, shape_record.id);
    const ObjectRecord deleted_record = shape(107U, root_id, 8U);
    const ObjectRecord added_stroke = vectorStroke(108U, root_id);
    const ObjectRecord split_a = vectorStroke(109U, root_id);
    const ObjectRecord split_b = vectorStroke(110U, root_id);
    const EraseMaskRecord added_mask = mask(411U);
    ObjectRecord placed = shape_record;
    placed.placement = Placement{root_id, OrderKey({20U})};
    ObjectRecord transformed = placed;
    transformed.transform = Transform2D{1.0, 0.0, 0.0, 1.0, 101.0, 102.0};
    const VectorPathGeometry changed_geometry{
        FillRule::kEvenOdd, {MoveTo{Vec2{2.0, 3.0}}, LineTo{Vec2{4.0, 5.0}}}};
    const ImageContent changed_image{ResourceId{id(801U)}, 44.0, 55.0, std::nullopt,
                                     ImageContentMode::kFill, 66.0, 77.0};
    const ConnectorContent changed_connector{
        ConnectorEndpoint{AttachedEndpoint{shape_record.id, StablePortAnchor{3U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{11.0, 12.0}}}, ConnectorRouting::kOrthogonal};

    ReplayTrace result = makeMinimumTrace();
    result.operations = {
        operation(InsertObjectsOp{{root, shape_record, vector_path_record, image_record,
                                   vector_stroke_record, rich_text_record, connector_record,
                                   deleted_record}}, 6101U),
        operation(DeleteObjectsOp{{deleted_record.id}}, 6102U),
        operation(RestoreObjectsOp{{deleted_record}}, 6103U),
        operation(SetPlacementsOp{{PlacementItem{shape_record.id, placed.placement}}}, 6104U),
        operation(SetTransformsOp{{TransformItem{shape_record.id, transformed.transform}}}, 6105U),
        operation(PatchPropertiesOp{{PropertyPatch{shape_record.id, 2U,
                                                    PropertyPatchAction::kSet,
                                                    PropertyValue{true}}}}, 6106U),
        operation(SetObjectSizeOp{{ObjectSizeItem{shape_record.id, 44.0, 55.0}}}, 6107U),
        operation(SetVectorPathGeometryOp{vector_path_record.id, changed_geometry}, 6108U),
        operation(SetImageContentOp{image_record.id, changed_image}, 6109U),
        operation(AddStrokeOp{added_stroke}, 6110U),
        operation(SplitStrokesOp{{StrokeSplit{added_stroke.id, {split_a, split_b}}}}, 6111U),
        operation(AddEraseMasksOp{{EraseMaskAddItem{vector_stroke_record.id, {added_mask}}}}, 6112U),
        operation(RemoveEraseMasksOp{{EraseMaskRemoveItem{vector_stroke_record.id, {added_mask.id}}}}, 6113U),
        operation(EditRichTextOp{rich_text_record.id, RichTextDelta{}}, 6114U),
        operation(SetConnectorContentOp{connector_record.id, changed_connector}, 6115U)};
    return result;
}

} // namespace canvas::verification::g1_07
