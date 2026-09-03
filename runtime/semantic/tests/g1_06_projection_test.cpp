#include "g1_06_projection.hpp"

#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <locale>
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

void expectKeys(const Json& value, std::initializer_list<const char*> expected) {
    ASSERT_TRUE(value.is_object());
    std::vector<std::string> actual;
    for (const auto& [key, item] : value.items()) {
        static_cast<void>(item);
        actual.push_back(key);
    }
    std::vector<std::string> wanted;
    for (const char* key : expected) wanted.emplace_back(key);
    EXPECT_EQ(actual, wanted);
}

class GroupingFacet final : public std::numpunct<char> {
  protected:
    char do_thousands_sep() const override { return '_'; }
    std::string do_grouping() const override { return "\3"; }
};

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

TEST(G106Projection, ScalarTagsIgnoreProcessGlobalLocale) {
    ReferenceObjectStore store;
    auto record = baseRecord(1U, ObjectKind::kVectorStroke, VectorStrokeContent{
        StrokeRecord{BrushDescriptor{}, 0x8000000000000001ULL, VectorStrokeData{}}});
    insert(store, std::move(record));
    const std::locale previous = std::locale();
    std::locale::global(std::locale(previous, new GroupingFacet));
    const std::string rendered = writeCanonicalProjectionJson(projectDocument(documentId(1U), 1U, store));
    std::locale::global(previous);
    const Json json = Json::parse(rendered);

    EXPECT_EQ(json["value"]["objects"][0]["transform"]["tx"], "f64:4029000000000000");
    EXPECT_EQ(json["value"]["objects"][0]["content"]["vector_stroke"]["stroke"]["deterministic_seed"],
              "u64:8000000000000001");
}

TEST(G106Projection, OmitsEveryAbsentOptionalWithoutEmittingNull) {
    ReferenceObjectStore store;
    insert(store, baseRecord(1U, ObjectKind::kImage, ImageContent{
        ResourceId{id(10U)}, 1.0, 1.0, std::nullopt, ImageContentMode::kStretch, 1.0, 1.0}));
    insert(store, baseRecord(2U, ObjectKind::kConnector, ConnectorContent{
        ConnectorEndpoint{AttachedEndpoint{id(1U), AutoPerimeterAnchor{std::nullopt}}},
        ConnectorEndpoint{FreePointEndpoint{Vec2{}}}, ConnectorRouting::kStraight}));
    const Json root = Json::parse(writeCanonicalProjectionJson(projectDocument(documentId(1U), 1U, store)));
    const auto& objects = root["value"]["objects"];

    EXPECT_FALSE(objects[0]["placement"].contains("parent_id"));
    EXPECT_FALSE(objects[0]["content"]["image"].contains("source_rect"));
    EXPECT_FALSE(objects[1]["content"]["connector"]["start"]["attached"]["anchor"]["auto_perimeter"].contains("hint"));
    EXPECT_EQ(root.dump().find(":null"), std::string::npos);
    EXPECT_EQ(root.dump().find(": null"), std::string::npos);
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
                  {TextRun{"axiom", text_style}, TextRun{"plain", TextStyle{}}}}}}}));

    BrushDescriptor vector_brush;
    vector_brush.brush_family_id = 51U;
    vector_brush.brush_version = 2U;
    vector_brush.color = ColorValue{0.1F, 0.2F, 0.3F, 0.4F};
    vector_brush.nominal_size = 5.0;
    vector_brush.opacity = 0.75F;
    vector_brush.pressure = PressureMapping{
        true,
        PiecewiseLinearCurve01{{{0.0F, 0.25F}, {1.0F, 1.0F}}},
        PiecewiseLinearCurve01{{{0.0F, 0.5F}, {1.0F, 0.75F}}}};
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
    expectKeys(root, {"format", "formatVersion", "semanticSchemaVersion", "rootType", "form", "value"});
    expectKeys(root["value"], {"document_id", "schema_version", "objects"});
    for (const auto& object : objects) {
        expectKeys(object, {"id", "kind_id", "kind_version", "placement", "transform", "properties", "content", "erase_masks"});
        expectKeys(object["transform"], {"a", "b", "c", "d", "tx", "ty"});
        expectKeys(object["properties"], {"entries"});
    }
    EXPECT_EQ(objects[0]["id"], "id128:01000000000000000000000000000000");
    EXPECT_EQ(objects[0]["transform"]["b"], "f64:8000000000000000");
    expectKeys(objects[0]["placement"], {"order_key"});
    EXPECT_FALSE(objects[0]["placement"].contains("parent_id"));
    expectKeys(objects[1]["placement"], {"parent_id", "order_key"});
    EXPECT_EQ(objects[1]["placement"]["parent_id"], "id128:09000000000000000000000000000000");
    EXPECT_EQ(objects[1]["placement"]["order_key"], "hex:0102");
    expectKeys(objects[0]["content"], {"shape"});
    expectKeys(objects[0]["content"]["shape"], {"shape_kind", "width", "height"});
    EXPECT_EQ(objects[0]["content"]["shape"]["width"], "f64:8000000000000000");
    expectKeys(objects[1]["content"], {"image"});
    expectKeys(objects[1]["content"]["image"],
               {"resource_id", "intrinsic_width", "intrinsic_height", "source_rect", "content_mode", "width", "height"});
    EXPECT_TRUE(objects[1]["content"]["image"].contains("source_rect"));
    expectKeys(objects[1]["content"]["image"]["source_rect"], {"x", "y", "width", "height"});
    EXPECT_EQ(objects[1]["content"]["image"]["source_rect"]["x"], "f64:0000000000000000");
    EXPECT_EQ(objects[1]["content"]["image"]["content_mode"], 1);
    for (const auto& entry : objects[0]["properties"]["entries"]) {
        expectKeys(entry, {"field_id", "value"});
        EXPECT_EQ(entry["value"].size(), 1U);
    }
    expectKeys(objects[0]["properties"]["entries"][0]["value"], {"bool_value"});
    expectKeys(objects[0]["properties"]["entries"][1]["value"], {"f32_value"});
    expectKeys(objects[0]["properties"]["entries"][2]["value"], {"color_value"});
    expectKeys(objects[0]["properties"]["entries"][2]["value"]["color_value"], {"r", "g", "b", "a"});
    expectKeys(objects[0]["properties"]["entries"][3]["value"], {"fill_style"});
    expectKeys(objects[0]["properties"]["entries"][5]["value"], {"stroke_style"});
    expectKeys(objects[0]["properties"]["entries"][7]["value"], {"blend_mode"});
    expectKeys(objects[0]["properties"]["entries"][8]["value"], {"connector_decoration"});
    expectKeys(objects[0]["properties"]["entries"][3]["value"]["fill_style"], {"none"});
    expectKeys(objects[0]["properties"]["entries"][4]["value"]["fill_style"], {"solid"});
    expectKeys(objects[0]["properties"]["entries"][4]["value"]["fill_style"]["solid"], {"color"});
    expectKeys(objects[0]["properties"]["entries"][4]["value"]["fill_style"]["solid"]["color"],
               {"r", "g", "b", "a"});
    expectKeys(objects[0]["properties"]["entries"][5]["value"]["stroke_style"], {"none"});
    EXPECT_TRUE(objects[0]["properties"]["entries"][5]["value"]["stroke_style"]["none"].empty());
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"], {"solid"});
    EXPECT_EQ(objects[0]["properties"]["entries"][0]["value"]["bool_value"], true);
    EXPECT_EQ(objects[0]["properties"]["entries"][1]["value"]["f32_value"], "f32:80000000");
    EXPECT_EQ(objects[0]["properties"]["entries"][2]["value"]["color_value"]["r"], "f32:3e800000");
    EXPECT_EQ(objects[0]["properties"]["entries"][3]["value"]["fill_style"]["none"], Json::object());
    EXPECT_TRUE(objects[0]["properties"]["entries"][4]["value"]["fill_style"].contains("solid"));
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"],
               {"color", "width", "cap", "join", "dash"});
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["join"], {"miter"});
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["join"]["miter"], {"limit"});
    EXPECT_EQ(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["cap"], 2);
    EXPECT_EQ(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["join"]["miter"]["limit"],
              "f64:4010000000000000");
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["dash"]["pattern"],
               {"segments", "offset"});
    expectKeys(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["dash"], {"pattern"});
    ASSERT_EQ(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["dash"]["pattern"]["segments"].size(), 2U);
    EXPECT_EQ(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["dash"]["pattern"]["segments"][0],
              "f64:3ff0000000000000");
    EXPECT_EQ(objects[0]["properties"]["entries"][6]["value"]["stroke_style"]["solid"]["dash"]["pattern"]["offset"],
              "f64:8000000000000000");
    expectKeys(objects[0]["properties"]["entries"][9]["value"]["stroke_style"], {"solid"});
    expectKeys(objects[0]["properties"]["entries"][9]["value"]["stroke_style"]["solid"]["join"], {"round"});
    EXPECT_TRUE(objects[0]["properties"]["entries"][9]["value"]["stroke_style"]["solid"]["join"]["round"].empty());
    expectKeys(objects[0]["properties"]["entries"][9]["value"]["stroke_style"]["solid"]["dash"], {"solid"});
    EXPECT_TRUE(objects[0]["properties"]["entries"][9]["value"]["stroke_style"]["solid"]["dash"]["solid"].empty());
    expectKeys(objects[0]["properties"]["entries"][10]["value"]["stroke_style"], {"solid"});
    expectKeys(objects[0]["properties"]["entries"][10]["value"]["stroke_style"]["solid"]["join"], {"bevel"});
    EXPECT_TRUE(objects[0]["properties"]["entries"][10]["value"]["stroke_style"]["solid"]["join"]["bevel"].empty());
    EXPECT_EQ(objects[0]["properties"]["entries"][7]["value"]["blend_mode"], 1);
    EXPECT_EQ(objects[0]["properties"]["entries"][8]["value"]["connector_decoration"], 2);
    expectKeys(objects[0]["erase_masks"][0], {"mask_id", "geometry"});
    expectKeys(objects[0]["erase_masks"][0]["geometry"], {"swept_circle"});
    expectKeys(objects[0]["erase_masks"][0]["geometry"]["swept_circle"], {"segments"});
    expectKeys(objects[0]["erase_masks"][0]["geometry"]["swept_circle"]["segments"][0],
               {"p0", "p1", "control1", "control2"});
    expectKeys(objects[0]["erase_masks"][0]["geometry"]["swept_circle"]["segments"][0]["p0"],
               {"position", "radius"});
    expectKeys(objects[0]["erase_masks"][0]["geometry"]["swept_circle"]["segments"][0]["p0"]["position"],
               {"x", "y"});
    EXPECT_EQ(objects[0]["erase_masks"][0]["mask_id"], "id128:65000000000000000000000000000000");
    EXPECT_EQ(objects[0]["erase_masks"][0]["geometry"]["swept_circle"]["segments"][0]["p0"]["radius"],
              "f64:4008000000000000");
    expectKeys(objects[0]["erase_masks"][1]["geometry"], {"filled_path"});
    expectKeys(objects[0]["erase_masks"][1]["geometry"]["filled_path"], {"path"});
    expectKeys(objects[0]["erase_masks"][1]["geometry"]["filled_path"]["path"], {"fill_rule", "commands"});
    expectKeys(objects[2]["content"], {"vector_path"});
    expectKeys(objects[2]["content"]["vector_path"], {"geometry"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"], {"fill_rule", "commands"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][0], {"move_to"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][1], {"line_to"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][2], {"quad_to"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][3], {"cubic_to"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][4], {"close_path"});
    EXPECT_EQ(objects[2]["content"]["vector_path"]["geometry"]["fill_rule"], 1);
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][0]["move_to"], {"point"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][0]["move_to"]["point"], {"x", "y"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][1]["line_to"], {"end"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][2]["quad_to"], {"control", "end"});
    expectKeys(objects[2]["content"]["vector_path"]["geometry"]["commands"][3]["cubic_to"],
               {"control1", "control2", "end"});
    EXPECT_TRUE(objects[2]["content"]["vector_path"]["geometry"]["commands"][4]["close_path"].empty());
    expectKeys(objects[3]["content"], {"rich_text"});
    expectKeys(objects[3]["content"]["rich_text"], {"document"});
    expectKeys(objects[3]["content"]["rich_text"]["document"], {"paragraphs"});
    expectKeys(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0],
               {"paragraph_id", "style", "runs"});
    expectKeys(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["style"],
               {"alignment", "line_height", "spacing_before", "spacing_after"});
    expectKeys(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][0],
               {"text", "style"});
    expectKeys(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][0]["style"],
               {"font_resource_id", "font_size", "weight", "italic", "underline", "color"});
    expectKeys(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][1]["style"],
               {"font_size", "weight", "italic", "underline", "color"});
    EXPECT_FALSE(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][1]["style"].contains("font_resource_id"));
    EXPECT_EQ(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["paragraph_id"],
              "id128:92010000000000000000000000000000");
    EXPECT_EQ(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][0]["text"], "axiom");
    EXPECT_EQ(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][0]["style"]["font_resource_id"],
              "id128:91010000000000000000000000000000");
    EXPECT_EQ(objects[3]["content"]["rich_text"]["document"]["paragraphs"][0]["runs"][0]["style"]["italic"], true);
    expectKeys(objects[4]["content"], {"vector_stroke"});
    expectKeys(objects[4]["content"]["vector_stroke"], {"stroke"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"], {"brush", "deterministic_seed", "vector"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["brush"],
               {"brush_family_id", "brush_version", "color", "nominal_size", "opacity", "pressure", "tilt", "smoothing", "spacing", "texture_resource_id", "blend_mode"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["pressure"],
               {"enabled", "size_curve", "opacity_curve"});
    const auto& pressure = objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["pressure"];
    expectKeys(pressure["size_curve"], {"points"});
    expectKeys(pressure["opacity_curve"], {"points"});
    ASSERT_EQ(pressure["size_curve"]["points"].size(), 2U);
    ASSERT_EQ(pressure["opacity_curve"]["points"].size(), 2U);
    expectKeys(pressure["size_curve"]["points"][0], {"x", "y"});
    expectKeys(pressure["opacity_curve"]["points"][1], {"x", "y"});
    EXPECT_EQ(pressure["size_curve"]["points"][0]["y"], "f32:3e800000");
    EXPECT_EQ(pressure["opacity_curve"]["points"][1]["y"], "f32:3f400000");
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["tilt"],
               {"enabled", "size_influence", "angle_influence"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["smoothing"], {"amount"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["spacing"], {"normalized_spacing"});
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["brush_family_id"], 51);
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["nominal_size"], "f64:4014000000000000");
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["opacity"], "f32:3f400000");
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["texture_resource_id"],
              "id128:f5010000000000000000000000000000");
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["brush"]["blend_mode"], 2);
    EXPECT_EQ(objects[4]["content"]["vector_stroke"]["stroke"]["deterministic_seed"], "u64:8000000000000001");
    EXPECT_TRUE(objects[4]["content"]["vector_stroke"]["stroke"].contains("vector"));
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["vector"], {"samples"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["vector"]["samples"][0],
               {"position", "pressure", "tilt"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["vector"]["samples"][0]["position"],
               {"x", "y"});
    expectKeys(objects[4]["content"]["vector_stroke"]["stroke"]["vector"]["samples"][0]["tilt"],
               {"x", "y"});
    expectKeys(objects[5]["content"], {"dab_stroke"});
    expectKeys(objects[5]["content"]["dab_stroke"]["stroke"], {"brush", "deterministic_seed", "dab"});
    expectKeys(objects[5]["content"]["dab_stroke"]["stroke"]["dab"], {"dabs"});
    expectKeys(objects[5]["content"]["dab_stroke"]["stroke"]["dab"]["dabs"][0],
               {"center", "size", "rotation", "opacity"});
    expectKeys(objects[5]["content"]["dab_stroke"]["stroke"]["dab"]["dabs"][0]["center"], {"x", "y"});
    EXPECT_EQ(objects[5]["content"]["dab_stroke"]["stroke"]["dab"]["dabs"][0]["rotation"], "f32:80000000");
    expectKeys(objects[5]["content"]["dab_stroke"]["stroke"]["brush"]["pressure"], {"enabled"});
    EXPECT_FALSE(objects[5]["content"]["dab_stroke"]["stroke"]["brush"].contains("texture_resource_id"));
    EXPECT_TRUE(objects[5]["content"]["dab_stroke"]["stroke"].contains("dab"));
    expectKeys(objects[6]["content"], {"connector"});
    expectKeys(objects[6]["content"]["connector"], {"start", "end", "routing"});
    expectKeys(objects[6]["content"]["connector"]["start"], {"free_point"});
    expectKeys(objects[6]["content"]["connector"]["start"]["free_point"], {"point"});
    expectKeys(objects[6]["content"]["connector"]["start"]["free_point"]["point"], {"x", "y"});
    expectKeys(objects[6]["content"]["connector"]["end"], {"attached"});
    expectKeys(objects[6]["content"]["connector"]["end"]["attached"], {"target_object_id", "anchor"});
    expectKeys(objects[6]["content"]["connector"]["end"]["attached"]["anchor"], {"auto_perimeter"});
    expectKeys(objects[6]["content"]["connector"]["end"]["attached"]["anchor"]["auto_perimeter"], {"hint"});
    expectKeys(objects[6]["content"]["connector"]["end"]["attached"]["anchor"]["auto_perimeter"]["hint"],
               {"x", "y"});
    EXPECT_EQ(objects[6]["content"]["connector"]["routing"], 2);
    EXPECT_EQ(objects[6]["content"]["connector"]["end"]["attached"]["target_object_id"],
              "id128:01000000000000000000000000000000");
    expectKeys(objects[9]["content"], {"connector"});
    expectKeys(objects[9]["content"]["connector"]["start"], {"attached"});
    expectKeys(objects[9]["content"]["connector"]["start"]["attached"]["anchor"], {"stable_port"});
    expectKeys(objects[9]["content"]["connector"]["start"]["attached"]["anchor"]["stable_port"], {"port_id"});
    EXPECT_EQ(objects[9]["content"]["connector"]["start"]["attached"]["anchor"]["stable_port"]["port_id"], 42);
    expectKeys(objects[7]["content"], {"sticky"});
    expectKeys(objects[7]["content"]["sticky"], {"width", "height"});
    EXPECT_EQ(objects[7]["content"]["sticky"]["width"], "f64:4054000000000000");
    expectKeys(objects[8]["content"], {"group"});
    EXPECT_TRUE(objects[8]["content"]["group"].empty());
    for (const auto& object : objects) EXPECT_EQ(object["content"].size(), 1U);
    EXPECT_EQ(root.dump().find(":null"), std::string::npos);
    EXPECT_EQ(root.dump().find(": null"), std::string::npos);
}

TEST(G106Projection, HasNoCodecProtobufSnapshotOrEngineDependency) {
    const auto read = [](const char* path) {
        std::ifstream source(path);
        EXPECT_TRUE(source.is_open()) << path;
        return std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    };
    const std::string text = read(G1_06_PROJECTION_SOURCE_PATH) + read(G1_06_PROJECTION_HEADER_PATH);
    std::string dependency_text = text;
    for (std::size_t position = dependency_text.find("DocumentSnapshot"); position != std::string::npos;
         position = dependency_text.find("DocumentSnapshot")) {
        dependency_text.erase(position, std::string("DocumentSnapshot").size());
    }
    EXPECT_EQ(text.find("codec"), std::string::npos);
    EXPECT_EQ(text.find("Codec"), std::string::npos);
    EXPECT_EQ(text.find("protobuf"), std::string::npos);
    EXPECT_EQ(text.find(".pb."), std::string::npos);
    EXPECT_EQ(dependency_text.find("operation"), std::string::npos);
    EXPECT_EQ(dependency_text.find("Operation"), std::string::npos);
    EXPECT_EQ(dependency_text.find("snapshot"), std::string::npos);
    EXPECT_EQ(dependency_text.find("Snapshot"), std::string::npos);
    EXPECT_EQ(text.find("canvas/semantic/document_id.hpp"), std::string::npos);
    EXPECT_EQ(text.find("Mutator"), std::string::npos);
    EXPECT_EQ(text.find("mutat"), std::string::npos);
    EXPECT_EQ(text.find("const_cast"), std::string::npos);
    EXPECT_EQ(text.find("insertFresh"), std::string::npos);
    EXPECT_EQ(text.find("replaceExisting"), std::string::npos);
    EXPECT_EQ(text.find("eraseExisting"), std::string::npos);
    EXPECT_EQ(text.find("sort"), std::string::npos);
    EXPECT_EQ(text.find("Sort"), std::string::npos);
}

} // namespace
} // namespace canvas::verification::g1_06
