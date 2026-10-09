#include "canvas/render/skia_headless_backend.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/runtime/resource_provider.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>
#include <array>
#include <filesystem>

// Resource availability is derived: missing contributes no pixels, ready
// decodes once, and warm replay reuses the same generation/digest.
int main(int argc, char** argv) {
    using namespace canvas;
    using namespace canvas::render;
    const FrameState frame{.worldViewport = {0, 0, 256, 256},
                           .sceneGeneration = semantic::SemanticGeneration{7},
                           .surfaceGeneration = SurfaceGeneration{1}};
    semantic::ImageContent image{};
    image.resource_id.value = foundation::ObjectId::fromUint64(42);
    image.width = 32; image.height = 32;
    semantic::ReferenceObjectStore objects;
    semantic::AppliedOperationLedger ledger;
    semantic::SemanticGenerationState generation{semantic::SemanticGeneration{6}};
    semantic::CanonicalCommitClock clock{semantic::RuntimeEpoch{1}};
    semantic::ObjectRecord canonical{};
    canonical.id = foundation::ObjectId::fromUint64(1);
    canonical.kind = semantic::ObjectKind::kImage; canonical.kind_version = 1;
    canonical.placement = {std::nullopt, semantic::OrderKey({1})}; canonical.content = image;
    semantic::Operation operation{};
    operation.id = semantic::OperationId{foundation::ObjectId::fromUint64(100)};
    operation.document_id = semantic::DocumentId{foundation::ObjectId::fromUint64(101)};
    operation.schema_version = operation.payload_version = 1;
    operation.payload = semantic::InsertObjectsOp{{canonical}};
    assert(semantic::OperationEngine{}.apply(operation,semantic::ApplySource::kLocalInteraction,
        objects,ledger,generation,clock).disposition == semantic::ApplyDisposition::kApplied);
    const auto beforeGeneration = generation.current();
    const auto beforeOrdinal = clock.lastCommittedOrdinal();
    const auto beforeObjects = objects.allObjects();
    RuntimeSceneRecord record{};
    record.objectId = foundation::ObjectId::fromUint64(1);
    record.kind = semantic::ObjectKind::kImage;
    record.content = image;
    record.visualBounds = {0, 0, 32, 32};
    ReferenceDrawList list{.frame = frame};
    list.viewportClip = frame.worldViewport;
    list.entries.push_back({record, true, ImageReferenceCommand{image}});
    runtime::MemoryResourceProvider resources;
    ImageResourceResolver resolver(resources);
    SkiaHeadlessBackend backend({256, 256}, &resolver);
    const auto submitted = backend.submit(FramePlan{frame, list});
    assert(submitted.code == BackendSubmissionCode::kAccepted);
    assert(backend.observation());
    const auto& rgba = backend.observation()->rgba;
    if (!std::all_of(rgba.begin(), rgba.end(), [](auto channel) { return channel == 0; })) {
        std::cerr << "G47-IMAGE-000: missing resource fabricated pixels\n";
        return 1;
    }
    assert(frame.sceneGeneration == semantic::SemanticGeneration{7});
    // 1x1 opaque red PNG, deterministic fixture.
    resources.publish(image.resource_id.value, {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
        0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,0x00,0x00,0x00,0x0d,0x49,0x44,0x41,
        0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0xf0,0x1f,0x00,0x05,0x00,0x01,0xff,0x89,0x99,
        0x3d,0x1d,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82});
    const auto first = resolver.resolve(image.resource_id);
    const auto second = resolver.resolve(image.resource_id);
    assert(first == second);
    assert(first->state == ImageResourceState::kReady);
    assert(first->width == 1 && first->height == 1 && first->rgba.size() == 4);
    assert(resolver.decodeCount() == 1);
    std::ifstream fixture(G47_IMAGE_FIXTURE, std::ios::binary);
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(fixture), {}};
    assert(!bytes.empty());
    resources.publish(image.resource_id.value, bytes);
    RasterSkiaSurfaceProvider provider;
    assert(provider.resize(256,256).code == BackendSubmissionCode::kAccepted);
    SkiaRenderer production(&resolver);
    auto render = [&](semantic::ImageContent content) {
        list.entries[0].command = ImageReferenceCommand{content};
        const FramePlan plan{frame, list};
        assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
        assert(production.renderFrame(provider, plan).code == BackendSubmissionCode::kAccepted);
        std::vector<std::uint8_t> pixels(256*256*4);
        assert(provider.readbackRgba(pixels).code == BackendSubmissionCode::kAccepted);
        auto composite = backend.observation()->rgba;
        for(std::size_t p=0;p<composite.size();p+=4) {
            const int alpha=composite[p+3];
            for(std::size_t c=0;c<3;++c) composite[p+c]=static_cast<std::uint8_t>(composite[p+c]+255-alpha);
            composite[p+3]=255;
        }
        assert(pixels == composite);
        return *backend.observation();
    };
    auto pixel = [](const HeadlessRasterObservation& raster, int x, int y) {
        const auto offset = static_cast<std::size_t>((y*256+x)*4);
        return std::array<std::uint8_t,4>{raster.rgba[offset],raster.rgba[offset+1],
                                        raster.rgba[offset+2],raster.rgba[offset+3]};
    };
    const auto cold = render(image), warm = render(image);
    assert(cold == warm && resolver.decodeCount() == 2);
    const std::array<std::uint8_t,4> red{255,0,0,255}, blue{0,0,255,255}, clear{};
    assert(pixel(cold,4,4) == red && pixel(cold,28,4) == blue);
    image.content_mode = semantic::ImageContentMode::kFit;
    const auto fit = render(image);
    assert(pixel(fit,4,4) == clear && pixel(fit,4,16) == red);
    image.content_mode = semantic::ImageContentMode::kFill;
    const auto fill = render(image);
    assert(pixel(fill,4,4) == red && pixel(fill,28,4) == blue);
    image.content_mode = semantic::ImageContentMode::kStretch;
    image.source_rect = semantic::NormalizedRect{0.5,0,0.5,1};
    const auto cropped = render(image);
    assert(pixel(cropped,4,4) == blue && pixel(cropped,28,4) == blue);
    resources.publish(image.resource_id.value, {1,2,3});
    assert(resolver.resolve(image.resource_id)->state == ImageResourceState::kDecodeFailed);
    const auto corrupt = render(image);
    assert(std::all_of(corrupt.rgba.begin(),corrupt.rgba.end(),[](auto c){return c==0;}));
    resources.publish(image.resource_id.value,bytes);
    assert(render(image).rgba == cropped.rgba);
    assert(generation.current() == beforeGeneration && clock.lastCommittedOrdinal() == beforeOrdinal);
    assert(objects.allObjects() == beforeObjects && ledger.find(operation.id).has_value());
    if(argc == 2) {
        const std::filesystem::path output(argv[1]); std::filesystem::create_directories(output);
        std::ofstream(output/"image-resource-fixtures.json") << "{\"fixture\":\"verification/fixtures/g47/image-stripes.png\",\"width\":4,\"height\":2}\n";
        std::ofstream(output/"image-resource-replay.json") << "{\"cold_digest\":\"" << cold.digest << "\",\"warm_digest\":\"" << warm.digest << "\",\"decodes_per_generation\":1,\"semantic_generation\":" << generation.current().value() << ",\"canonical_commits_before\":" << beforeOrdinal.value() << ",\"canonical_commits_after\":" << clock.lastCommittedOrdinal().value() << "}\n";
        std::ofstream(output/"image-render-parity.json") << "{\"headless_production_diff\":0,\"fit\":\"" << fit.digest << "\",\"fill\":\"" << fill.digest << "\",\"stretch\":\"" << cold.digest << "\",\"source_rect\":\"" << cropped.digest << "\"}\n";
    }
    std::cout << "G47-IMAGE-000 image modes/render parity PASS\n";
}
