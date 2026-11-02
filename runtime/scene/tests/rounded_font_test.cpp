#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/resource_manifest.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/text/text_layout.hpp"
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
using namespace canvas;
TEST(HandwritingFont, ManifestAdmitsVersionedFontMetadataAndRejectsDanglingOrCycles) {
    semantic::ResourceManifest m;
    semantic::ResourceManifestEntry a, b;
    a.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(1)};
    a.resource_kind = semantic::ResourceKind::kFont;
    a.resource_revision = 1;
    a.content_hash = "sha256:" + std::string(64, 'a');
    a.byte_length = 100;
    a.decode_version = 1;
    a.layout_version = 2;
    b = a;
    b.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(2)};
    b.layout_version = 1;
    b.metadata = {8, 1, 16, 0, 24, 1};
    a.metadata = {8, 2, 16, 0, 24, 1, 34, 16};
    a.metadata.insert(a.metadata.end(), b.resource_id.value.bytes.begin(),
                      b.resource_id.value.bytes.end());
    m.entries = {a, b};
    ASSERT_TRUE(semantic::validateResourceManifest(m));
    m.entries.pop_back();
    EXPECT_FALSE(semantic::validateResourceManifest(m));
    m.entries = {a, b};
    m.entries[1].layout_version = 2;
    m.entries[1].metadata = a.metadata;
    EXPECT_FALSE(semantic::validateResourceManifest(m));
}
TEST(HandwritingFont, MixedTextUsesNotoAndExcalifontWithoutSystemFallback) {
    runtime::MemoryResourceProvider resources;
    text::RichTextLayoutService layout(resources, {600, 1});
    const semantic::ResourceId latin{foundation::ObjectId::fromUint64(1)},
        cjk{foundation::ObjectId::fromUint64(2)};
    const auto load = [](const char *path) {
        std::ifstream in(std::string(AXIOM_TEST_FONT_ROOT) + path, std::ios::binary);
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(in), {}};
    };
    resources.publish(latin.value, load("/Excalifont/Excalifont-Regular.ttf"));
    resources.publish(cjk.value, load("/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf"));
    layout.registerFont(
        {latin, "574ab04d4598e048edf2c4f579664ff9865a8db729fd0db01a774cc9e9b38247", 0});
    layout.registerFont(
        {cjk, "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b", 0});
    semantic::RichTextContent content;
    content.document.paragraphs.push_back(
        {foundation::ObjectId::fromUint64(10),
         {semantic::ParagraphAlignment::kLeft, 1.25, 0, 0},
         {{"Canvas 2323 我爱我家 繁體中文", {latin, 24, 400, false, false, {0, 0, 0, 1}}}}});
    const auto snapshot = layout.resolve(foundation::ObjectId::fromUint64(11), content);
    ASSERT_TRUE(snapshot->diagnostics.empty());
    ASSERT_GT(snapshot->runs.size(), 1);
    bool haveLatin = false, haveCJK = false;
    for (const auto &run : snapshot->runs) {
        const auto id = snapshot->fonts[run.font].resourceId;
        haveLatin = haveLatin || id == latin;
        haveCJK = haveCJK || id == cjk;
    }
    EXPECT_TRUE(haveLatin && haveCJK);
    resources.markMissing(cjk.value);
    const auto missing = layout.resolve(foundation::ObjectId::fromUint64(11), content);
    EXPECT_NE(snapshot->digest, missing->digest);
    EXPECT_FALSE(missing->diagnostics.empty());
}

TEST(CornerSemantic, CodecNormalizationAndUnsupportedKindsAreClosed) {
    semantic::ObjectRecord r;
    r.id = foundation::ObjectId::fromUint64(10);
    r.kind = semantic::ObjectKind::kShape;
    r.kind_version = 2;
    r.content = semantic::ShapeContent{3, 120, 80};
    r.placement.order_key = semantic::OrderKey({128});
    r.properties.entries = {{0x103U, semantic::CornerStyleV1{1, 16}}};
    const auto operation = [&]() {
        return semantic::Operation{semantic::OperationId{foundation::ObjectId::fromUint64(2)},
                                   semantic::DocumentId{foundation::ObjectId::fromUint64(1)}, 1, 1,
                                   semantic::InsertObjectsOp{{r}}};
    };
    const auto encoded = semantic::SemanticCodec::encodeProtobufOperation(operation());
    ASSERT_TRUE(encoded.ok());
    const auto decoded = semantic::SemanticCodec::decodeProtobufOperation(encoded.bytes);
    ASSERT_TRUE(decoded.ok());
    EXPECT_EQ(decoded.operation.payload, operation().payload);
    std::get<semantic::CornerStyleV1>(r.properties.entries[0].value).radius = 0;
    const auto zero = semantic::normalizeOperation(operation());
    ASSERT_TRUE(zero.ok());
    EXPECT_TRUE(std::get<semantic::InsertObjectsOp>(zero.value.payload)
                    .objects[0]
                    .properties.entries.empty());
    r.content = semantic::ShapeContent{2, 120, 80};
    EXPECT_FALSE(semantic::normalizeOperation(operation()).ok());
    r.content = semantic::ShapeContent{1, 120, 80};
    std::get<semantic::CornerStyleV1>(r.properties.entries[0].value) = {2, 16};
    EXPECT_FALSE(semantic::SemanticCodec::encodeProtobufOperation(operation()).ok());
}
TEST(CornerSemantic, SnapshotRejectsReaderWithoutCornerCapability) {
    semantic::SemanticSnapshot s;
    s.document_id = semantic::DocumentId{foundation::ObjectId::fromUint64(1)};
    s.schema_version = 3;
    s.manifest_present = true;
    s.manifest.schema_version = 1;
    s.document_revision = 1;
    s.recovery_frontier = {8, 1, 16, 1, 24, 1};
    s.digest_algorithm = 1;
    semantic::ObjectRecord r;
    r.id = foundation::ObjectId::fromUint64(10);
    r.kind = semantic::ObjectKind::kShape;
    r.kind_version = 1;
    r.content = semantic::ShapeContent{1, 120, 80};
    r.placement.order_key = semantic::OrderKey({128});
    r.properties.entries = {{0x103U, semantic::CornerStyleV1{1, 16}}};
    s.objects = {r};
    const auto full = semantic::SnapshotCapability::kAllWithRoundedAndFontFallbackV1;
    ASSERT_TRUE(semantic::SnapshotCodec::populateRequirements(s));
    ASSERT_TRUE(semantic::SnapshotCodec::bindDigests(s, full));
    const auto bytes = semantic::SnapshotCodec::encode(s, full);
    ASSERT_TRUE(bytes.ok());
    EXPECT_FALSE(semantic::SnapshotCodec::decode(
                     bytes.bytes, semantic::SnapshotCapability::kAllWithSketchExtendedV1)
                     .ok());
    ASSERT_TRUE(semantic::SnapshotCodec::decode(bytes.bytes, full).ok());
}

TEST(HandwritingFont, SnapshotRequiresFallbackCapabilityEvenWithoutTextObjects) {
    semantic::SemanticSnapshot s;
    s.document_id = semantic::DocumentId{foundation::ObjectId::fromUint64(1)};
    s.schema_version = 3;
    s.manifest_present = true;
    s.document_revision = 1;
    s.recovery_frontier = {8, 1, 16, 1, 24, 1};
    s.digest_algorithm = 1;
    semantic::ResourceManifestEntry a, b;
    a.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(1)};
    a.resource_kind = semantic::ResourceKind::kFont;
    a.resource_revision = 1;
    a.content_hash = "sha256:" + std::string(64, 'a');
    a.byte_length = 100;
    a.decode_version = 1;
    a.layout_version = 2;
    b = a;
    b.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(2)};
    b.layout_version = 1;
    b.metadata = {8, 1, 16, 0, 24, 1};
    a.metadata = {8, 2, 16, 0, 24, 1, 34, 16};
    a.metadata.insert(a.metadata.end(), b.resource_id.value.bytes.begin(),
                      b.resource_id.value.bytes.end());
    s.manifest.entries = {a, b};
    const auto full = semantic::SnapshotCapability::kAllWithRoundedAndFontFallbackV1;
    ASSERT_TRUE(semantic::SnapshotCodec::populateRequirements(s));
    ASSERT_TRUE(semantic::SnapshotCodec::bindDigests(s, full));
    const auto encoded = semantic::SnapshotCodec::encode(s, full);
    ASSERT_TRUE(encoded.ok());
    EXPECT_FALSE(semantic::SnapshotCodec::decode(encoded.bytes,
                                                 semantic::SnapshotCapability::kAllWithRoundedV1)
                     .ok());
    EXPECT_TRUE(semantic::SnapshotCodec::decode(encoded.bytes, full).ok());
}
