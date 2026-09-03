#include "g1_06_projection.hpp"

#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace canvas::verification::g1_06 {
namespace {

using namespace canvas::semantic;
using Json = nlohmann::ordered_json;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

ProjectionDocumentId documentId(std::uint8_t first) {
    ProjectionDocumentId result;
    result.bytes[0] = first;
    return result;
}

ObjectRecord baseRecord(std::uint64_t value, ObjectKind kind, ObjectContent content) {
    ObjectRecord record;
    record.id = id(value);
    record.kind = kind;
    record.kind_version = 7U;
    record.placement = Placement{std::nullopt, OrderKey({0x01U, static_cast<std::uint8_t>(value)})};
    record.transform = Transform2D{1.0, -0.0, 0.25, 1.0, 12.5, -3.0};
    record.content = std::move(content);
    return record;
}

void insert(ReferenceObjectStore& store, ObjectRecord record) {
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(store, std::move(record)));
}

class ObservedStore final : public ObjectStore {
  public:
    explicit ObservedStore(std::vector<ObjectRecord> records) : records_(std::move(records)) {}

    std::size_t size() const noexcept override { return records_.size(); }
    bool contains(const ObjectId& object_id) const noexcept override { return find(object_id) != nullptr; }
    const ObjectRecord* find(const ObjectId& object_id) const noexcept override {
        for (const auto& record : records_) if (record.id == object_id) return &record;
        return nullptr;
    }
    std::vector<ObjectRecord> allObjects() const override { ++all_objects_calls; return records_; }
    std::vector<ObjectRecord> children(const std::optional<ObjectId>&) const override { return {}; }

    mutable std::size_t all_objects_calls = 0U;

  private:
    std::vector<ObjectRecord> records_;
};

TEST(G106Projection, EmitsExactEnvelopeScalarPresenceAndCanonicalText) {
    ReferenceObjectStore store;
    const auto projection = projectDocument(documentId(0xabU), 9U, store);
    const std::string first = writeCanonicalProjectionJson(projection);
    const std::string second = writeCanonicalProjectionJson(projection);
    const Json json = Json::parse(first);

    EXPECT_EQ(first, second);
    ASSERT_FALSE(first.empty());
    EXPECT_EQ(first.back(), '\n');
    EXPECT_NE(first.find("\n  \"format\""), std::string::npos);
    EXPECT_EQ(std::vector<std::string>({"format", "formatVersion", "semanticSchemaVersion", "rootType", "form", "value"}),
              [&] { std::vector<std::string> keys; for (const auto& [key, value] : json.items()) { static_cast<void>(value); keys.push_back(key); } return keys; }());
    EXPECT_EQ(json["format"], "axiom-verification-projection-v1");
    EXPECT_EQ(json["formatVersion"], 1);
    EXPECT_EQ(json["semanticSchemaVersion"], 1);
    EXPECT_EQ(json["rootType"], "auditoryworks.axiom.v1.DocumentSnapshot");
    EXPECT_EQ(json["form"], "CANONICAL");
    EXPECT_EQ(json["value"]["document_id"], "id128:ab000000000000000000000000000000");
    EXPECT_EQ(json["value"]["schema_version"], 9);
    EXPECT_EQ(json["value"]["objects"], Json::array());
}

TEST(G106Projection, ReadsAllObjectsExactlyOnceAndPreservesProviderOrder) {
    const ObjectRecord later = baseRecord(2U, ObjectKind::kShape, ShapeContent{2U, 2.0, 2.0});
    const ObjectRecord earlier = baseRecord(1U, ObjectKind::kShape, ShapeContent{1U, 1.0, 1.0});
    ObservedStore store({later, earlier});

    const auto projection = projectDocument(documentId(1U), 1U, store);

    EXPECT_EQ(store.all_objects_calls, 1U);
    ASSERT_EQ(projection.objects.size(), 2U);
    EXPECT_EQ(projection.objects[0], later);
    EXPECT_EQ(projection.objects[1], earlier);
}

TEST(G106Projection, CoversEveryFrozenV1ContentAndPropertyBranch) {
    ReferenceObjectStore store;

    auto shape = baseRecord(1U, ObjectKind::kShape, ShapeContent{3U, -0.0, 2.5});
    shape.properties.entries = {
        {1U, true},
        {2U, -0.0F},
        {3U, ColorValue{0.25F, 0.5F, 0.75F, 1.0F}},
        {4U, FillStyleValue{NoFill{}}},
        {5U, FillStyleValue{SolidFill{ColorValue{1.0F, 0.0F, 0.5F, 1.0F}}}},
        {6U, StrokeStyleValue{NoStroke{}}},
        {7U, StrokeStyleValue{SolidStroke{ColorValue{1, 1, 1, 1}, 2.0, StrokeCap::kRound,
             StrokeJoin{MiterJoin{4.0}}, StrokeDash{DashPattern{{1.0, 2.0}, -0.0}}}}},
        {8U, BlendModeValue::kNormal},
        {9U, ConnectorDecorationValue::kArrow},
        {10U, StrokeStyleValue{SolidStroke{ColorValue{}, 1.0, StrokeCap::kButt,
              StrokeJoin{RoundJoin{}}, StrokeDash{SolidDash{}}}}},
        {11U, StrokeStyleValue{SolidStroke{ColorValue{}, 1.0, StrokeCap::kSquare,
              StrokeJoin{BevelJoin{}}, StrokeDash{SolidDash{}}}}},
    };
    shape.erase_masks = {
        EraseMaskRecord{id(101U), SweptCircleMask{{EraseCubicSegment{
            EraseKnot{Vec2{1.0, 2.0}, 3.0}, EraseKnot{Vec2{4.0, 5.0}, 6.0},
            Vec2{7.0, 8.0}, Vec2{9.0, 10.0}}}}},
        EraseMaskRecord{id(102U), FilledPathMask{VectorPathGeometry{FillRule::kEvenOdd,
            {MoveTo{Vec2{0.0, 0.0}}, LineTo{Vec2{1.0, 0.0}}, ClosePath{}}}}},
    };
    insert(store, shape);

    auto image = baseRecord(2U, ObjectKind::kImage, ImageContent{
        ResourceId{id(201U)}, 640.0, 480.0, NormalizedRect{},
        ImageContentMode::kFit, 320.0, 240.0});
    image.placement.parent_id = id(9U);
    insert(store, image);

    insert(store, baseRecord(3U, ObjectKind::kVectorPath, VectorPathContent{
        VectorPathGeometry{FillRule::kNonZero,
            {MoveTo{Vec2{1.0, 2.0}}, LineTo{Vec2{3.0, 4.0}},
             QuadTo{Vec2{5.0, 6.0}, Vec2{7.0, 8.0}},
             CubicTo{Vec2{9.0, 10.0}, Vec2{11.0, 12.0}, Vec2{13.0, 14.0}}, ClosePath{}}}}));

    TextStyle text_style{ResourceId{id(401U)}, 12.0, 600U, true, false,
                         ColorValue{0.1F, 0.2F, 0.3F, 1.0F}};
    insert(store, baseRecord(4U, ObjectKind::kRichText, RichTextContent{RichTextDocument{{
        Paragraph{id(402U), ParagraphStyle{ParagraphAlignment::kCenter, 1.2, 2.0, 3.0},
                  {TextRun{"axiom", text_style}}}}}}));

    BrushDescriptor vector_brush;
    vector_brush.brush_family_id = 51U;
    vector_brush.brush_version = 2U;
    vector_brush.color = ColorValue{0.1F, 0.2F, 0.3F, 0.4F};
    vector_brush.nominal_size = 5.0;
    vector_brush.opacity = 0.75F;
    vector_brush.pressure = PressureMapping{true, PiecewiseLinearCurve01{{{0.0F, 0.25F}, {1.0F, 1.0F}}}, std::nullopt};
    vector_brush.tilt = TiltMapping{true, 0.5F, -0.25F};
    vector_brush.smoothing = SmoothingSettings{0.2F};
    vector_brush.spacing = SpacingSettings{0.1F};
    vector_brush.texture_resource_id = ResourceId{id(501U)};
    vector_brush.blend_mode = BrushBlendMode::kHighlighter;
    insert(store, baseRecord(5U, ObjectKind::kVectorStroke, VectorStrokeContent{StrokeRecord{
        vector_brush, 0x8000000000000001ULL,
        VectorStrokeData{{StrokeSample{Vec2{1.0, 2.0}, 0.5F, Vec2{-1.0, 1.0}}}}}}));

    insert(store, baseRecord(6U, ObjectKind::kDabStroke, DabStrokeContent{StrokeRecord{
        BrushDescriptor{}, 6U,
        DabStrokeData{{DabInstance{Vec2{2.0, 3.0}, 4.0, -0.0F, 1.0F}}}}}));

    insert(store, baseRecord(7U, ObjectKind::kConnector, ConnectorContent{
        ConnectorEndpoint{FreePointEndpoint{Vec2{1.0, 2.0}}},
        ConnectorEndpoint{AttachedEndpoint{id(1U), AutoPerimeterAnchor{Vec2{}}}},
        ConnectorRouting::kOrthogonal}));
    insert(store, baseRecord(8U, ObjectKind::kSticky, StickyContent{80.0, 90.0}));
    insert(store, baseRecord(9U, ObjectKind::kGroup, GroupContent{}));
    insert(store, baseRecord(10U, ObjectKind::kConnector, ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{id(9U), StablePortAnchor{42U}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{3.0, 4.0}}}, ConnectorRouting::kStraight}));

    const Json root = Json::parse(writeCanonicalProjectionJson(projectDocument(documentId(0x11U), 1U, store)));
    const auto& objects = root["value"]["objects"];
    ASSERT_EQ(objects.size(), 10U);
    EXPECT_EQ(objects[0]["id"], "id128:01000000000000000000000000000000");
    EXPECT_EQ(objects[0]["transform"]["b"], "f64:8000000000000000");
    EXPECT_FALSE(objects[0]["placement"].contains("parent_id"));
    EXPECT_EQ(objects[1]["placement"]["parent_id"], "id128:09000000000000000000000000000000");
    EXPECT_EQ(objects[1]["placement"]["order_key"], "hex:0102");
    EXPECT_TRUE(objects[1]["content"]["image"].contains("source_rect"));
    EXPECT_EQ(objects[1]["content"]["image"]["source_rect"]["x"], "f64:0000000000000000");
    EXPECT_EQ(objects[0]["properties"]["entries"][1]["value"]["f32_value"], "f32:80000000");
    EXPECT_EQ(objects[0]["properties"]["entries"][3]["value"]["fill_style"]["none"], Json::object());
    EXPECT_TRUE(objects[0]["properties"]["entries"][4]["value"]["fill_style"].contains("solid"));
    EXPECT_TRUE(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["join"].contains("miter"));
    EXPECT_TRUE(objects[0]["properties"]["entries"][9]["value"]["stroke_style"]["solid"]["join"].contains("round"));
    EXPECT_TRUE(objects[0]["properties"]["entries"][10]["value"]["stroke_style"]["solid"]["join"].contains("bevel"));
    EXPECT_TRUE(objects[0]["erase_masks"][0]["geometry"].contains("swept_circle"));
    EXPECT_TRUE(objects[0]["erase_masks"][1]["geometry"].contains("filled_path"));
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][0].contains("move_to"));
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][1].contains("line_to"));
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][2].contains("quad_to"));
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][3].contains("cubic_to"));
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][4].contains("close_path"));
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["deterministic_seed"], "u64:8000000000000001");
    EXPECT_TRUE(objects[4]["content"]["vector_stroke"]["stroke"].contains("vector"));
    EXPECT_TRUE(objects[5]["content"]["dab_stroke"]["stroke"].contains("dab"));
    EXPECT_TRUE(objects[6]["content"]["connector"]["start"].contains("free_point"));
    EXPECT_TRUE(objects[6]["content"]["connector"]["end"]["attached"]["anchor"].contains("auto_perimeter"));
    EXPECT_TRUE(objects[6]["content"]["connector"]["end"]["attached"]["anchor"]["auto_perimeter"].contains("hint"));
    EXPECT_TRUE(objects[9]["content"]["connector"]["start"]["attached"]["anchor"].contains("stable_port"));
    EXPECT_TRUE(objects[8]["content"]["group"].empty());
    for (const auto& object : objects) EXPECT_EQ(object["content"].size(), 1U);
    for (const auto& entry : objects[0]["properties"]["entries"]) EXPECT_EQ(entry["value"].size(), 1U);
    EXPECT_EQ(root.dump().find(":null"), std::string::npos);
    EXPECT_EQ(root.dump().find(": null"), std::string::npos);
}

TEST(G106Projection, HasNoCodecProtobufSnapshotOrEngineDependency) {
    std::ifstream source(G1_06_PROJECTION_SOURCE_PATH);
    ASSERT_TRUE(source.is_open());
    const std::string text((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("canvas/semantic/codec.hpp"), std::string::npos);
    EXPECT_EQ(text.find("snapshot.pb.h"), std::string::npos);
    EXPECT_EQ(text.find("SemanticCodec"), std::string::npos);
    EXPECT_EQ(text.find("SnapshotCodec"), std::string::npos);
    EXPECT_EQ(text.find("OperationEngine"), std::string::npos);
}

} // namespace
} // namespace canvas::verification::g1_06
