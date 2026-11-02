#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace canvas;
std::vector<std::uint8_t> load(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root);
    foundation::ObjectId document;
    document.bytes.fill(0x87);
    runtime::RuntimeComposition owner;
    if (!owner.openDocument(document) || !owner.bindSurface(1000, 680))
        return 3;
    semantic::ResourceManifest manifest;
    struct Font {
        const char *file;
        const char *hash;
        std::uint64_t id;
    };
    const Font fonts[] = {
        {"imported/Excalifont/Excalifont-Regular.ttf",
         "574ab04d4598e048edf2c4f579664ff9865a8db729fd0db01a774cc9e9b38247", 0xFC0310}};
    for (const auto &f : fonts) {
        const auto bytes = load(std::filesystem::path(AXIOM_TEST_FONT_ROOT) / f.file);
        semantic::ResourceManifestEntry e;
        e.resource_id = semantic::ResourceId{foundation::ObjectId::fromUint64(f.id)};
        e.resource_kind = semantic::ResourceKind::kFont;
        e.resource_revision = 1;
        e.content_hash = "sha256:" + std::string(f.hash);
        e.byte_length = bytes.size();
        e.decode_version = 1;
        e.layout_version = 1;
        e.metadata = {8, 1, 16, 0, 24, 1};
        manifest.entries.push_back(e);
    }
    if (!owner.bindInitialResourceManifest(manifest, 1))
        return 4;
    for (const auto &f : fonts) {
        const auto token = owner.resourceToken(foundation::ObjectId::fromUint64(f.id));
        if (!token || !owner.publishResourceBytes(
                          *token, load(std::filesystem::path(AXIOM_TEST_FONT_ROOT) / f.file)))
            return 5;
    }
    if (!owner.ownerPump())
        return 6;
    std::vector<semantic::ObjectRecord> objects;
    for (unsigned i = 0; i < 4; ++i) {
        semantic::ObjectRecord r;
        r.id = foundation::ObjectId::fromUint64(100 + i);
        r.kind = semantic::ObjectKind::kShape;
        r.kind_version = i >= 2 ? 2 : 1;
        r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(40 + i)});
        r.transform.tx = 40 + (i % 2) * 270;
        r.transform.ty = 35 + (i / 2) * 220;
        r.content = semantic::ShapeContent{i >= 2 ? 3U : 1U, 220, 140};
        r.properties.entries = {
            {0x100U,
             semantic::FillStyleValue{semantic::PatternFillV1{
                 1,
                 {.2F, .5F, .8F, .8F},
                 i % 2 ? semantic::PatternKind::kCrosshatch : semantic::PatternKind::kDiagonal,
                 -45,
                 10,
                 1}}},
            {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{.1F, .2F, .3F, 1},
                                                                      3,
                                                                      semantic::StrokeCap::kButt,
                                                                      semantic::MiterJoin{4},
                                                                      semantic::SolidDash{}}}},
            {0x102U, semantic::SketchStyleV1{1, 9223372036854775809ULL + i, i % 2 ? 2. : 0., true}},
            {0x103U, semantic::CornerStyleV1{1, 24}}};
        objects.push_back(r);
    }
    semantic::ObjectRecord line;
    line.id = foundation::ObjectId::fromUint64(110);
    line.kind = semantic::ObjectKind::kConnector;
    line.kind_version = 1;
    line.placement.order_key = semantic::OrderKey({60});
    line.content = semantic::ConnectorContent{{semantic::FreePointEndpoint{{650, 90}}},
                                              {semantic::FreePointEndpoint{{900, 350}}},
                                              semantic::ConnectorRouting::kOrthogonal};
    line.properties.entries = {
        {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{.1F, .2F, .3F, 1},
                                                                  3,
                                                                  semantic::StrokeCap::kButt,
                                                                  semantic::MiterJoin{4},
                                                                  semantic::SolidDash{}}}},
        {0x102U, semantic::SketchStyleV1{1, 999, 1.5, true}},
        {0x103U, semantic::CornerStyleV1{1, 36}},
        {0x201U, semantic::ConnectorDecorationValue::kArrow}};
    objects.push_back(line);
    for (unsigned i = 0; i < 2; ++i) {
        semantic::ObjectRecord r;
        r.id = foundation::ObjectId::fromUint64(120 + i);
        r.kind = semantic::ObjectKind::kRichText;
        r.kind_version = 3;
        r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(70 + i)});
        r.transform.tx = 45;
        r.transform.ty = 480 + i * 80;
        semantic::RichTextContent t;
        t.document.paragraphs.push_back(
            {foundation::ObjectId::fromUint64(130 + i),
             {semantic::ParagraphAlignment::kLeft, 1.25, 0, 0},
            {{"Excalifont Canvas 2323 我爱我家",
               {semantic::ResourceId{foundation::ObjectId::fromUint64(0xFC0310)},
                30,
                400,
                false,
                false,
                {.1F, .2F, .3F, 1}}}}});
        t.layout = semantic::RichTextLayout{900};
        r.content = t;
        objects.push_back(r);
    }
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(900)},
                           semantic::DocumentId{document}, 1, 1,
                           semantic::InsertObjectsOp{objects}};
    const auto encoded = semantic::SemanticCodec::encodeProtobufOperation(op);
    if (!encoded.ok()) {
        std::cerr << "encode " << int(encoded.error) << '\n';
        return 7;
    }
    if (owner.applyRemoteOperation(op).disposition != semantic::ApplyDisposition::kApplied)
        return 8;
    for (unsigned i = 0; i < 200 && owner.controlFacade().readConnectorState(line.id).routeStatus !=
                                        runtime::ConnectorRouteStatus::kReady;
         ++i)
        if (!owner.ownerPump())
            return 9;
    runtime::ImageExportOptions options;
    options.padding = 0;
    options.bounds = std::array<double, 4>{0, 0, 1000, 680};
    const auto image = owner.controlFacade().exportImage(options);
    if (image.status != runtime::ImageExportStatus::kExported) {
        std::cerr << "export " << int(image.status) << '\n';
        return 10;
    }
    std::ofstream(root / "fixture.bin", std::ios::binary)
        .write(reinterpret_cast<const char *>(encoded.bytes.data()), encoded.bytes.size());
    std::ofstream(root / "native-render.png", std::ios::binary)
        .write(reinterpret_cast<const char *>(image.bytes.data()), image.bytes.size());
    std::cout << image.width << 'x' << image.height << ' ' << image.bytes.size() << '\n';
}
