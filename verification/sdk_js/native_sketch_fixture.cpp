#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace canvas;
int main(int argc, char **argv) {
    if (argc != 2 && argc != 3)
        return 2;
    std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root);
    foundation::ObjectId document;
    document.bytes.fill(0x73);
    runtime::RuntimeComposition owner;
    if (!owner.openDocument(document) || !owner.bindSurface(800, 550))
        return 3;
    std::vector<semantic::ObjectRecord> objects;
    if (argc == 2)
        for (unsigned i = 0; i < 5; ++i) {
            semantic::ObjectRecord o;
            o.id = foundation::ObjectId::fromUint64(100 + i);
            o.kind = i == 4 ? semantic::ObjectKind::kSticky : semantic::ObjectKind::kShape;
            o.kind_version = i == 2 ? 2 : 1;
            o.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(40 + i)});
            o.transform.tx = 30 + (i % 3) * 240;
            o.transform.ty = 30 + (i / 3) * 240;
            const auto kind = i == 1 ? 2U : i == 2 ? 3U : 1U;
            o.content = i == 4 ? semantic::ObjectContent{semantic::StickyContent{180, 150}}
                               : semantic::ObjectContent{semantic::ShapeContent{kind, 180, 150}};
            semantic::SolidStroke stroke{{.1F, .2F, .3F, 1}, double(2 + i)};
            stroke.join = semantic::MiterJoin{4};
            if (i == 3)
                stroke.dash = semantic::DashPattern{{8, 5}, 0};
            o.properties.entries = {{3U, i == 2 ? .5F : 1.F},
                                    {0x100U, semantic::FillStyleValue{semantic::PatternFillV1{
                                                 1,
                                                 {.2F, .5F, .8F, 1},
                                                 i % 2 ? semantic::PatternKind::kCrosshatch
                                                       : semantic::PatternKind::kDiagonal,
                                                 -45,
                                                 8,
                                                 1}}},
                                    {0x101U, semantic::StrokeStyleValue{stroke}},
                                    {0x102U, semantic::SketchStyleV1{1, 9223372036854775809ULL + i,
                                                                     double(i % 4), true}}};
            objects.push_back(o);
        }
    if (argc == 2) {
        semantic::ObjectRecord line;
        line.id = foundation::ObjectId::fromUint64(106);
        line.kind = semantic::ObjectKind::kConnector;
        line.kind_version = 1;
        line.placement.order_key = semantic::OrderKey({50});
        line.content = semantic::ConnectorContent{{semantic::FreePointEndpoint{{520, 300}}},
                                                  {semantic::FreePointEndpoint{{730, 430}}}};
        line.properties.entries = {
            {0x101U, semantic::StrokeStyleValue{semantic::SolidStroke{{.2F, .2F, .2F, 1},
                                                                      3,
                                                                      semantic::StrokeCap::kRound,
                                                                      semantic::RoundJoin{},
                                                                      semantic::SolidDash{}}}},
            {0x102U, semantic::SketchStyleV1{1, 999, 2, true}},
            {0x201U, semantic::ConnectorDecorationValue::kArrow}};
        objects.push_back(line);
    } else {

        for (unsigned i = 0; i < 3; ++i) {
            semantic::ObjectRecord o;
            o.id = foundation::ObjectId::fromUint64(200 + i);
            o.kind_version = 1;
            o.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(40 + i)});
            o.transform.tx = 30 + (i % 3) * 240;
            o.transform.ty = 50;
            if (i == 0) {
                o.kind = semantic::ObjectKind::kVectorPath;
                o.content = semantic::VectorPathContent{
                    {semantic::FillRule::kEvenOdd,
                     {semantic::MoveTo{{0, 0}}, semantic::QuadTo{{90, -25}, {180, 0}},
                      semantic::LineTo{{180, 180}},
                      semantic::CubicTo{{120, 210}, {60, 160}, {0, 180}}, semantic::ClosePath{},
                      semantic::MoveTo{{45, 45}}, semantic::LineTo{{135, 45}},
                      semantic::LineTo{{135, 135}}, semantic::LineTo{{45, 135}},
                      semantic::ClosePath{}}}};
            } else if (i == 1) {
                o.kind = semantic::ObjectKind::kSection;
                o.content = semantic::SectionContent{{0, 0, 180, 180}, "", 8, 24};
            } else {
                o.kind = semantic::ObjectKind::kTable;
                semantic::TableContent t;
                t.rows = {{{foundation::ObjectId::fromUint64(31)}, 60},
                          {{foundation::ObjectId::fromUint64(32)}, 60},
                          {{foundation::ObjectId::fromUint64(33)}, 60}};
                t.columns = {{{foundation::ObjectId::fromUint64(41)}, 90},
                             {{foundation::ObjectId::fromUint64(42)}, 90}};
                t.cells.resize(6);
                for (auto &c : t.cells)
                    c.text.layout = semantic::RichTextLayout{78};
                t.grid.color = {.1F, .2F, .3F, 1};
                t.grid.width = 3;
                o.content = t;
            }
            o.properties.entries = {{3U, .75F}};
            if (i < 2) {
                o.properties.entries.push_back(
                    {0x100U,
                     semantic::FillStyleValue{semantic::PatternFillV1{
                         1, {.2F, .5F, .8F, 1}, semantic::PatternKind::kCrosshatch, -33, 8, 1}}});
                o.properties.entries.push_back(
                    {0x101U,
                     semantic::StrokeStyleValue{semantic::SolidStroke{{.1F, .2F, .3F, 1},
                                                                      3,
                                                                      semantic::StrokeCap::kButt,
                                                                      semantic::MiterJoin{4},
                                                                      semantic::SolidDash{}}}});
            }
            o.properties.entries.push_back(
                {0x102U, semantic::SketchStyleV1{1, 9223372036854775809ULL + i, 2, true}});
            objects.push_back(o);
        }
    }
    semantic::Operation op{semantic::OperationId{foundation::ObjectId::fromUint64(900)},
                           semantic::DocumentId{document}, 1, 1,
                           semantic::InsertObjectsOp{objects}};
    const auto encoded = semantic::SemanticCodec::encodeProtobufOperation(op);
    if (!encoded.ok())
        return 4;
    const auto applied = owner.applyRemoteOperation(op);
    if (applied.disposition != semantic::ApplyDisposition::kApplied)
        return 5;
    runtime::ImageExportOptions options;
    options.padding = 0;
    options.bounds = std::array<double, 4>{0, 0, 800, 550};
    const auto image = owner.controlFacade().exportImage(options);
    if (image.status != runtime::ImageExportStatus::kExported) {
        std::cerr << "export status " << int(image.status) << '\n';
        return 6;
    }
    std::ofstream(root / "fixture.bin", std::ios::binary)
        .write(reinterpret_cast<const char *>(encoded.bytes.data()), encoded.bytes.size());
    std::ofstream(root / "native-render.png", std::ios::binary)
        .write(reinterpret_cast<const char *>(image.bytes.data()), image.bytes.size());
    std::cout << image.width << 'x' << image.height << ' ' << image.bytes.size() << '\n';
}
