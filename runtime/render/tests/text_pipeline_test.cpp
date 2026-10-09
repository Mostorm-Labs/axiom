#include "canvas/text/text_layout.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "../../scene/tests/incremental_runtime_test_access.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/render/skia_headless_backend.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>
#include "../../text/src/sha256.h"

using namespace canvas;
using namespace canvas::semantic;
using namespace canvas::render;
namespace {
foundation::ObjectId id(std::uint64_t n) { return foundation::ObjectId::fromUint64(n); }
std::string sha(std::span<const std::uint8_t> bytes) {
    text::internal::Sha256 hash; hash.Update(bytes); const auto result=hash.Finish();
    constexpr char hex[]="0123456789abcdef"; std::string out;
    for(auto b:result) { out+=hex[b>>4]; out+=hex[b&15]; } return out;
}
SceneRecord sceneRecord(const ObjectRecord& object) {
    return {object.id,SceneOrderKey(object.id.bytes[0]),SceneObjectKind::kRichText,
            SceneRecordFlags::kVisible,computeBounds(object).world,ContentRevision(1),{1,1},{1,1}};
}
class Compiler final : public ISemanticSceneCompiler {
  public:
    bool semanticOnlyBefore = false;
    explicit Compiler(const Scene& scene) : scene_(scene) {}
    foundation::Result<CompiledSceneSnapshot> compileFull(const SemanticReadView& view) const override {
        CompiledSceneSnapshot s{SceneRevision(view.generation().value()),{}};
        for(const auto& object:view.allObjects()) s.records.push_back(sceneRecord(object));
        return foundation::Result<CompiledSceneSnapshot>::success(std::move(s));
    }
    foundation::Result<CompiledSceneDelta> compileDelta(const SemanticReadView& view,const ChangeSet& changes) const override {
        CompiledSceneDelta d{scene_.revision(),SceneRevision(scene_.revision().value()+1),{},std::nullopt};
        const auto published=scene_.read();
        for(const auto& change:changes.objects()) {
            const auto* before=published.find(change.object_id);
            const auto* after=view.find(change.object_id);
            d.mutations.push_back({before?SceneMutationKind::kUpdate:SceneMutationKind::kInsert,
                change.object_id,before?std::optional<SceneRecord>(*before):std::nullopt,
                after?std::optional<SceneRecord>(sceneRecord(*after)):std::nullopt});
            // A semantic compiler cannot know resource-derived text bounds.
            if (semanticOnlyBefore && d.mutations.back().before)
                d.mutations.back().before->worldBounds = computeBounds(*after).world;
        }
        return foundation::Result<CompiledSceneDelta>::success(std::move(d));
    }
  private:
    const Scene& scene_;
};
}
int main(int argc,char** argv) {
    runtime::MemoryResourceProvider resources;
    text::RichTextLayoutService layout(resources,{128,1});
    const ResourceId font{id(700)};
    layout.registerFont({font,std::string(text::kRobotoSha256),0});
    ReferenceObjectStore objects; AppliedOperationLedger ledger;
    SemanticGenerationState generation{SemanticGeneration(1)}; CanonicalCommitClock clock{RuntimeEpoch(1)};
    Scene scene(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>());
    SceneBinding binding(scene); IncrementalRuntimeCoordinator coordinator(binding);
    coordinator.setTextLayoutService(&layout); Compiler compiler(scene);
    TextStyle style{font,16,400,false,false,{0,0,0,1}};
    auto makeObject=[&](std::uint64_t n) {
        ObjectRecord o{}; o.id=id(n); o.kind=ObjectKind::kRichText; o.kind_version=1;
        o.placement={std::nullopt,OrderKey({static_cast<std::uint8_t>(n)})};
        o.transform.tx=10; o.transform.ty=static_cast<double>(n*30);
        o.content=RichTextContent{{{{id(n+10),{ParagraphAlignment::kLeft,1,0,0},{{"iiii WWWW",style}}}}}};
        return o;
    };
    std::uint64_t sequence=100;
    auto apply=[&](OperationPayload payload,bool seed=false) {
        Operation op{}; op.id=OperationId{id(sequence++)}; op.document_id=DocumentId{id(999)};
        op.schema_version=op.payload_version=1; op.payload=std::move(payload);
        const auto result=OperationEngine{}.apply(op,ApplySource::kLocalInteraction,objects,ledger,generation,clock);
        if(result.disposition!=ApplyDisposition::kApplied) {
            std::cerr << "canonical operation rejected disposition=" << static_cast<int>(result.disposition) << '\n';
            std::abort();
        }
        const SemanticReadView view(objects,generation.current());
        if(seed) assert(coordinator.recover(compiler,SceneCommitInput(generation.current(),view)));
        else {
            const auto& changes=result.commit_record->change_set;
            const auto synchronized=coordinator.apply(compiler,SceneCommitInput(changes.beforeGeneration(),changes.afterGeneration(),view,&changes));
            if(!synchronized) { std::cerr << synchronized.error().message << '\n'; std::abort(); }
        }
    };
    apply(InsertObjectsOp{{makeObject(1),makeObject(2)}},true);
    const auto missing=coordinator.runtimeScene().find(id(1))->textLayout;
    assert(missing && missing->runs.empty());
    const auto beforeGeneration=generation.current();
    const auto beforeOrdinal=clock.lastCommittedOrdinal();
    std::ifstream f(G47_LATIN_FONT,std::ios::binary);
    resources.publish(font.value,{std::istreambuf_iterator<char>(f),{}});
    const auto missingRevision=scene.revision();
    const auto missingWorldBounds=scene.read().find(id(1))->worldBounds;
    IncrementalRuntimeTestAccess::failAt(coordinator,RuntimeCheckpoint::kBeforePublication);
    assert(!coordinator.refreshTextResources());
    assert(scene.revision()==missingRevision);
    assert(coordinator.runtimeScene().find(id(1))->textLayout==missing);
    assert(scene.read().find(id(1))->worldBounds==missingWorldBounds);
    IncrementalRuntimeTestAccess::clear(coordinator);
    const auto refreshed=coordinator.refreshTextResources();
    assert(refreshed && refreshed.value().apply.recordsTouched==2);
    assert(generation.current()==beforeGeneration && clock.lastCommittedOrdinal()==beforeOrdinal);
    assert(scene.semanticGeneration()==beforeGeneration && coordinator.runtimeScene().generation()==beforeGeneration);
    assert(coordinator.publicationObservationCoherent());
    const auto ready=coordinator.runtimeScene().find(id(1))->textLayout;
    assert(!ready->runs.empty() && ready->localBounds.right>20);
    const auto* published=scene.read().find(id(1));
    assert(published && published->worldBounds==coordinator.runtimeScene().find(id(1))->worldBounds);
    const auto metric=layout.metrics();
    compiler.semanticOnlyBefore=true;
    apply(EditRichTextOp{id(1),RichTextDelta{1,{InsertTextStep{id(11),0,"A",style}}}});
    assert(layout.metrics().objectLayouts==metric.objectLayouts+1);
    const auto unaffected=coordinator.runtimeScene().find(id(2))->textLayout;
    std::vector<std::string> locality;
    auto edit=[&](std::string name,RichTextStep step,std::uint64_t paragraphs) {
        const auto before=layout.metrics();
        apply(EditRichTextOp{id(1),RichTextDelta{1,{std::move(step)}}});
        assert(layout.metrics().objectLayouts==before.objectLayouts+1);
        assert(layout.metrics().paragraphLayouts==before.paragraphLayouts+paragraphs);
        assert(coordinator.runtimeScene().find(id(2))->textLayout==unaffected);
        locality.push_back("{\"case\":\""+name+"\",\"objects_relaid_out\":1,\"paragraphs_relaid_out\":"+
                           std::to_string(paragraphs)+",\"unrelated_changed\":0}");
    };
    edit("delete",DeleteTextStep{id(11),0,1},1);
    edit("split",SplitParagraphStep{id(11),4,id(30)},2);
    edit("merge",MergeParagraphStep{id(11),id(30)},1);
    auto underline=style; underline.underline=true; underline.color={0.5F,0.2F,0.1F,1};
    edit("inline-style",SetInlineStyleStep{id(11),0,4,underline},1);
    edit("paragraph-style",SetParagraphStyleStep{id(11),{ParagraphAlignment::kCenter,1.2,2,3}},1);
    for(int i=0;i<8;++i) edit("repeated-insert",InsertTextStep{id(11),0,"!",style},1);
    const auto beforeTransform=coordinator.runtimeScene().find(id(1))->textLayout;
    const auto beforeMetric=layout.metrics();
    apply(SetTransformsOp{{{id(1),{1.25,0,0,1.25,12,30}}}});
    assert(coordinator.runtimeScene().find(id(1))->textLayout==beforeTransform);
    assert(layout.metrics().objectLayouts==beforeMetric.objectLayouts);

    std::vector<std::string> matrix;
    const auto cameraLayouts=layout.metrics().objectLayouts;
    for(const float zoom:{0.25F,1.0F,4.0F}) for(const float dpr:{1.0F,2.0F}) {
        RasterSkiaSurfaceProvider surface;
        assert(surface.resize(256,256).code==BackendSubmissionCode::kAccepted);
        FrameState state{.camera={.worldCenter={64,64},.zoom=zoom},
            .worldViewport={64-128/(zoom*dpr),64-128/(zoom*dpr),64+128/(zoom*dpr),64+128/(zoom*dpr)},
            .metrics={256/dpr,256/dpr,256,256,dpr,dpr},.sceneGeneration=generation.current(),
            .sceneReadToken=scene.revision(),.surfaceGeneration=SurfaceGeneration(surface.generation()),
            .viewportClip={0,0,256/dpr,256/dpr}};
        const auto visible=VisibilityResolver::resolve(state,scene); assert(visible);
        const auto source=DirectReferenceSource::build(state,visible.value(),coordinator.runtimeScene()); assert(source);
        const auto replay=DirectReferenceSource::build(state,visible.value(),coordinator.runtimeScene()); assert(replay);
        assert(source.value().canonicalBytes==replay.value().canonicalBytes);
        const FramePlan p{state,source.value()};
        SkiaHeadlessBackend oracle({256,256,0xffffffffU}); SkiaRenderer renderer;
        assert(oracle.submit(p).code==BackendSubmissionCode::kAccepted);
        assert(renderer.renderFrame(surface,p).code==BackendSubmissionCode::kAccepted);
        std::vector<std::uint8_t> actual(256*256*4); assert(surface.readbackRgba(actual).code==BackendSubmissionCode::kAccepted);
        auto expected=oracle.observation()->rgba;
        unsigned maxDiff=0; std::size_t changed=0;
        for(std::size_t i=0;i<actual.size();i+=4) {
            bool different=false;
            for(std::size_t c=0;c<4;++c) {
                const auto difference=static_cast<unsigned>(std::abs(int(actual[i+c])-int(expected[i+c])));
                maxDiff=std::max(maxDiff,difference); different|=difference!=0;
            }
            changed+=different;
        }
        if(maxDiff>8 || changed>256) {
            std::cerr<<"zoom/DPR parity failure zoom="<<zoom<<" dpr="<<dpr<<" max="<<maxDiff<<" changed="<<changed<<'\n'; return 1;
        }
        assert(source.value().diagnostics.semanticObjectIterations==0);
        matrix.push_back("{\"zoom\":"+std::to_string(zoom)+",\"dpr\":"+std::to_string(dpr)+
            ",\"plan_sha256\":\""+sha(source.value().canonicalBytes)+"\",\"reference_sha256\":\""+sha(expected)+
            "\",\"production_sha256\":\""+sha(actual)+"\",\"max_channel_diff\":"+std::to_string(maxDiff)+
            ",\"changed_pixels\":"+std::to_string(changed)+"}");
    }
    assert(layout.metrics().objectLayouts==cameraLayouts);
    if(argc==2) {
        const std::filesystem::path output(argv[1]); std::filesystem::create_directories(output);
        auto array=[&](std::string_view file,const auto& rows) {
            std::ofstream out(output/file); out<<"[";
            for(std::size_t i=0;i<rows.size();++i) out<<(i?",":"")<<rows[i]; out<<"]\n";
        };
        array("reference-production-parity.json",matrix); array("edit-locality.json",locality);
        std::ofstream(output/"negative-controls.json")<<"{\"transform_relayouts\":0,\"camera_relayouts\":0,\"resource_canonical_commits\":0,\"resource_abort_retry\":true,\"publication_coherent\":true}\n";
        std::ofstream(output/"resource-transition.json")<<"{\"semantic_generation_before\":"<<beforeGeneration.value()<<",\"semantic_generation_after_resource\":"<<beforeGeneration.value()<<",\"canonical_ordinal_before\":"<<beforeOrdinal.value()<<",\"canonical_ordinal_after_resource\":"<<beforeOrdinal.value()<<",\"affected_objects\":2}\n";
    }

    RasterSkiaSurfaceProvider provider; assert(provider.resize(256,256).code==BackendSubmissionCode::kAccepted);
    SkiaRenderer production; SkiaHeadlessBackend reference({256,256,0xffffffffU});
    // A 2x physical surface must map the 128x128 logical viewport onto all
    // 256x256 device pixels; DPR metadata alone is not a rendering contract.
    const FrameState highDpi{.camera={.worldCenter={64,64},.zoom=1},
        .worldViewport={0,0,128,128},.metrics={128,128,256,256,2,2},
        .sceneGeneration=generation.current(),.sceneReadToken=scene.revision(),
        .surfaceGeneration=SurfaceGeneration(provider.generation()),.viewportClip={0,0,128,128}};
    const auto highDpiVisible=VisibilityResolver::resolve(highDpi,scene); assert(highDpiVisible);
    const auto highDpiDraw=DirectReferenceSource::build(highDpi,highDpiVisible.value(),coordinator.runtimeScene()); assert(highDpiDraw);
    assert(production.renderFrame(provider,{highDpi,highDpiDraw.value()}).code==BackendSubmissionCode::kAccepted);
    std::vector<std::uint8_t> dprPixels(256*256*4); assert(provider.readbackRgba(dprPixels).code==BackendSubmissionCode::kAccepted);
    bool deviceInk=false;
    for(std::size_t y=128;y<256;++y) for(std::size_t x=0;x<128;++x) {
        const auto p=(y*256+x)*4;
        deviceInk|=dprPixels[p]<250 || dprPixels[p+1]<250 || dprPixels[p+2]<250;
    }
    if(!deviceInk) { std::cerr<<"G47 DPR RED: logical glyphs were not scaled to physical pixels\n"; return 1; }
    FrameState frame{.camera={.worldCenter={128,128},.zoom=1},.worldViewport={0,0,256,256},
        .metrics={256,256,256,256,1,1},.sceneGeneration=generation.current(),
        .sceneReadToken=scene.revision(),.surfaceGeneration=SurfaceGeneration(provider.generation()),.viewportClip={0,0,256,256}};
    auto visibility=VisibilityResolver::resolve(frame,scene); assert(visibility);
    auto draw=DirectReferenceSource::build(frame,visibility.value(),coordinator.runtimeScene()); assert(draw);
    const FramePlan plan{frame,draw.value()};
    if(reference.submit(plan).code!=BackendSubmissionCode::kAccepted) {
        std::cerr << "G47-RENDER-004 RED: shaped fractional text bounds rejected\n"; return 1;
    }
    assert(production.renderFrame(provider,plan).code==BackendSubmissionCode::kAccepted);
    std::vector<std::uint8_t> pixels(256*256*4); assert(provider.readbackRgba(pixels).code==BackendSubmissionCode::kAccepted);
    auto expected=reference.observation()->rgba;
    assert(pixels==expected);
    SkiaHeadlessBackend transparent({256,256}); assert(transparent.submit(plan).code==BackendSubmissionCode::kAccepted);
    std::size_t antialiased=0;
    for(std::size_t p=3;p<transparent.observation()->rgba.size();p+=4) {
        const auto a=transparent.observation()->rgba[p]; if(a>0 && a<255) ++antialiased;
    }
    if(antialiased==0) { std::cerr << "G47-RENDER-004 RED: rectangle placeholders still used instead of shaped glyphs\n"; return 1; }
    assert(draw.value().diagnostics.semanticObjectIterations==0);
    assert(layout.metrics().objectLayouts==beforeMetric.objectLayouts);
    std::cout << "G47 scene/resource/edit/transform/render pipeline PASS\n";
}
