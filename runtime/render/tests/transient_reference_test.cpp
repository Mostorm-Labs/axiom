#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace canvas;
using namespace canvas::render;
namespace {
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
semantic::EraseMaskRecord mask(std::uint64_t id,double x,double y){
    semantic::VectorPathGeometry path;
    path.commands={semantic::MoveTo{{x,y}},semantic::LineTo{{x+25,y}},
        semantic::LineTo{{x+25,y+40}},semantic::LineTo{{x,y+40}},semantic::ClosePath{}};
    return {ObjectId::fromUint64(id),semantic::FilledPathMask{std::move(path)}};
}
semantic::ObjectRecord compact(std::uint64_t id,double offset=0){
    semantic::ObjectRecord r;r.id=ObjectId::fromUint64(id);r.kind=semantic::ObjectKind::kVectorStroke;r.kind_version=3;
    semantic::CompactStrokeContent c;
    auto loaded=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);require(bool(loaded),"v4 catalog");
    c.stroke.config=*loaded.package.compactConfig;c.stroke.config.opacity=.65;c.stroke.seed=42;
    for(unsigned i=0;i<320;++i)c.stroke.knots.push_back({offset+20.+i*.75,80.+22.*std::sin(i*.045),std::uint16_t(18000+(i%120)*250)});
    r.content=std::move(c);r.placement.order_key=semantic::OrderKey({std::uint8_t(id)});return r;
}
struct Fixture {
    semantic::ReferenceObjectStore store;
    RuntimeScene scene;
    std::vector<ObjectId> ids;
    Fixture(){
        std::vector<semantic::ObjectRecord> objects;
        semantic::ObjectRecord shape;shape.id=ObjectId::fromUint64(1);shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;
        shape.content=semantic::ShapeContent{0,210,120};shape.transform.tx=35;shape.transform.ty=25;
        shape.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.1F,.3F,.8F,.6F}}}}};
        objects.push_back(shape);
        auto stroke=compact(2);stroke.erase_masks={mask(71,90,55)};objects.push_back(stroke);
        semantic::ObjectRecord vector;vector.id=ObjectId::fromUint64(3);vector.kind=semantic::ObjectKind::kVectorStroke;vector.kind_version=2;
        semantic::BrushStrokeContent body;body.stroke.snapshot.paint={.8,.1,.1,.7,.8,1,1};
        body.stroke.vector_output.outline={{40,110},{70,100},{120,130},{200,115},{220,125},{120,145}};
        vector.content=std::move(body);objects.push_back(vector);
        // Runtime forces selected old/offscreen records into transient visibility
        // before applying the temporary transform. Navigation must not cull them.
        objects.push_back(compact(4,10000));
        semantic::ObjectRecord sticky;sticky.id=ObjectId::fromUint64(5);sticky.kind=semantic::ObjectKind::kSticky;sticky.kind_version=1;
        sticky.content=semantic::StickyContent{30,22};sticky.transform.tx=245;sticky.transform.ty=125;objects.push_back(sticky);
        for(auto& r:objects){r.placement.order_key=semantic::OrderKey({std::uint8_t(ids.size()+1)});ids.push_back(r.id);
            require(semantic::internal::ObjectStoreMutator::insertFresh(store,r),"fixture insert");}
        require(bool(scene.replace({store,semantic::SemanticGeneration{7}})),"full Scene projection");
    }
    FrameState frame(unsigned dpr)const{return {ViewId{9},CameraState{{160,90},1,0,CameraGeneration{4}},
        {0,0,320,180},{320,180,320*dpr,180*dpr,float(dpr),1},semantic::SemanticGeneration{7},SceneRevision{7},
        SurfaceGeneration{11},MetricsGeneration{13},FrameId{17},{3,4,317,176}};}
    VisibilityResult visibility(const FrameState& f)const{return {.sceneGeneration=f.sceneGeneration,.sceneReadToken=f.sceneReadToken,
        .queryWorldRect=f.worldViewport,.candidatesExamined=ids.size(),.visibleRecords=ids.size(),.backToFront=ids};}
};

std::vector<std::uint8_t> pixels(const ReferenceDrawList& references){
    const auto plan=FramePlanBuilder::build(references.frame,references,{},true,true);require(bool(plan),"pixel FramePlan");
    const auto& m=references.frame.metrics;
    auto info=SkImageInfo::Make(m.physicalWidth,m.physicalHeight,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB());
    auto surface=SkSurfaces::Raster(info);require(bool(surface),"pixel surface");
    require(internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan.value()).code==BackendSubmissionCode::kAccepted,"actual Skia scene draw");
    std::vector<std::uint8_t> result(std::size_t(m.physicalWidth)*m.physicalHeight*4);
    require(surface->readPixels(info,result.data(),m.physicalWidth*4,0,0),"pixel readback");return result;
}

void completeCommandsAndPixels(){
    Fixture fixture;
    const semantic::Transform2D transforms[]={{1,0,0,1,0,0},{.94,.34,-.34,.94,80,-20},{1.4,.1,.2,.7,10,10}};
    unsigned cases=0;
    for(unsigned dpr:{1U,2U,4U})for(const auto transform:transforms){
        auto f=fixture.frame(dpr);auto v=fixture.visibility(f);
        v.chunkSelections={{fixture.ids[1],{0},false}};
        const auto full=DirectReferenceSource::build(f,v,fixture.scene);
        const auto transient=DirectReferenceSource::buildTransient(f,v,fixture.scene);
        require(bool(full)&&bool(transient),"complete source construction");
        require(!full.value().canonicalBytes.empty()&&!full.value().digest.empty(),"canonical encoding removed");
        require(transient.value().canonicalBytes.empty()&&transient.value().digest.empty(),"discarded transient encoding");
        auto stripped=full.value();stripped.canonicalBytes.clear();stripped.digest.clear();
        require(stripped==transient.value(),"full commands/identities/clip/metadata differ");
        require(stripped.entries.size()==5&&stripped.entries[3].record.objectId==fixture.ids[3],"offscreen selected source dropped");
        require(stripped.diagnostics.payloadCopyBytes==0,"immutable payload copied");
        for(const auto& e:transient.value().entries){
            const auto original=fixture.scene.find(e.record.objectId)->content.shared(),alias=e.record.content.shared();
            require(!original.owner_before(alias)&&!alias.owner_before(original),"payload storage ownership differs");
            require(!e.navigationChunkSpan,"transient incorrectly used navigation filtering");
        }
        auto expected=stripped,actual=transient.value();
        for(auto* list:{&expected,&actual}){
            list->entries[1].record.transform=transform;
            list->entries[1].presentationEraseActive=true;
            list->entries[1].presentationEraseMasks={mask(72,150,70)};
            list->entries[1].presentationOpacity=.55F;
            list->entries[3].record.transform.tx=-10000;
        }
        const auto oldPixels=pixels(expected),newPixels=pixels(actual);
        require(oldPixels==newPixels,"Skia pixels changed by no-encoding construction");
        require(std::any_of(oldPixels.begin(),oldPixels.end(),[](auto b){return b!=0;}),"empty pixel oracle");++cases;
    }
    std::cout<<"{\"complete_command_pixel_cases\":"<<cases<<",\"dpr\":[1,2,4],\"exact_pixels\":true,\"shared_payloads\":true}\n";
}

void rejectsTheSameInvalidInputs(){
    Fixture fixture;const auto f=fixture.frame(1);const auto visible=fixture.visibility(f);
    auto sameFailure=[&](const FrameState& input,const VisibilityResult& v,const RuntimeScene& scene){
        const auto a=DirectReferenceSource::build(input,v,scene),b=DirectReferenceSource::buildTransient(input,v,scene);
        require(!a&&!b&&a.error().code==b.error().code,"transient weakened canonical input rejection");};
    const FrameState stale{f.viewId,f.camera,f.worldViewport,f.metrics,semantic::SemanticGeneration{8},
        f.sceneReadToken,f.surfaceGeneration,f.metricsGeneration,f.frameId,f.viewportClip};
    sameFailure(stale,visible,fixture.scene);
    auto bad=visible;bad.sceneReadToken=SceneRevision{8};sameFailure(f,bad,fixture.scene);
    RuntimeScene other;sameFailure(f,visible,other);
    bad=visible;++bad.visibleRecords;sameFailure(f,bad,fixture.scene);
    bad=visible;bad.backToFront[0]=ObjectId::fromUint64(999);sameFailure(f,bad,fixture.scene);
    auto far=visible;far.queryWorldRect={20000,20000,20100,20100};
    const auto full=DirectReferenceSource::buildTransient(f,far,fixture.scene),nav=DirectReferenceSource::buildNavigation(f,far,fixture.scene);
    require(full&&nav&&full.value().entries.size()==5&&nav.value().entries.size()<5,"full/no-encoding entry used navigation culling");
    std::cout<<"{\"negative_identity_cases\":5,\"navigation_independent\":true}\n";
}
}
int main(){try{completeCommandsAndPixels();rejectsTheSameInvalidInputs();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
