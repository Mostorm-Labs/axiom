// Native stage comparison only: unchanged canonical construction is the old
// complete transient work, before its discarded bytes/digest were cleared.
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/foundation/internal/frame_timing.hpp"
#include "object_store_mutator.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace canvas;
using nlohmann::json;
namespace timing=foundation::internal::frame_timing;
namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
json stats(std::vector<std::uint64_t> v){
    std::sort(v.begin(),v.end());const auto q=[&](double p){return v[std::size_t(std::ceil(v.size()*p))-1];};
    return {{"count",v.size()},{"p50_ns",q(.5)},{"p95_ns",q(.95)},{"max_ns",v.back()}};
}
json measure(bool canonical,const render::FrameState& frame,const render::VisibilityResult& visible,const RuntimeScene& scene){
    timing::Context observed;const auto previous=timing::active;timing::active=&observed;
    const auto start=std::chrono::steady_clock::now();
    auto result=canonical?render::DirectReferenceSource::build(frame,visible,scene):render::DirectReferenceSource::buildTransient(frame,visible,scene);
    const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
    timing::active=previous;require(bool(result),"reference construction");
    const auto bytes=result.value().canonicalBytes.size();
    require((canonical&&bytes>0&&!result.value().digest.empty())||(!canonical&&bytes==0&&result.value().digest.empty()),"encoding mechanism");
    require(result.value().entries.size()==visible.visibleRecords,"component source dropped entries");
    json row{{"mode",canonical?"old-complete-canonical":"full-transient-no-encoding"},{"wall_ns",elapsed},{"encoded_bytes",bytes}};
    for(auto stage:{timing::Stage::reference_entries,timing::Stage::reference_encode,timing::Stage::reference_digest}){
        const auto i=static_cast<std::size_t>(stage);row[timing::names[i]]=observed.stages[i].inclusive;
    }
    return row;
}
}
int main(int argc,char** argv){try{
    require(argc==2,"usage: pinned literal fixture JSON");std::ifstream stream(argv[1]);json fixture;stream>>fixture;
    require(fixture.at("fixture_id")=="continuous-30s-v1"&&fixture.at("samples").size()==7200,"pinned fixture");
    const auto loaded=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);require(bool(loaded),"v4 catalog");
    ink::BrushSession author{1,ink::resolveBrushState(loaded.package,42)};require(author.begin(),"session begin");
    ink::BrushPreviewDelta preview;std::uint64_t sequence=0;
    for(const auto& s:fixture["samples"]){
        const ink::BrushSample point{double(float(s[0].get<double>()/1000)),double(float(s[1].get<double>()/1000)),.5,false,++sequence,std::stoull(s[2].get<std::string>()),false,s[4]=="up"};
        require(author.append(std::span(&point,1),{},preview)==ink::BrushSessionError::kNone,"fixture append");
    }
    ink::BrushCommitIntent intent;require(author.seal(intent)==ink::BrushSessionError::kNone,"fixture seal");
    json output{{"schema","g5-ur1a-native-component-v1"},{"input_samples",7200},{"knots_per_stroke",intent.knots.size()},
        {"scope","native construction stages, not browser latency or physical present"},{"rows",json::array()}};
    for(unsigned copies:{1U,8U,16U,32U}){
        semantic::ReferenceObjectStore store;std::vector<ObjectId> ids;
        for(unsigned i=0;i<copies;++i){
            semantic::ObjectRecord object;object.id=ObjectId::fromUint64(i+1);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
            semantic::CompactStrokeRecord body;body.config=*loaded.package.compactConfig;body.seed=42;body.knots=intent.knots;
            object.content=semantic::CompactStrokeContent{std::move(body)};object.transform.tx=i;object.transform.ty=i;
            object.placement.order_key=semantic::OrderKey({std::uint8_t(i+1)});ids.push_back(object.id);
            require(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"component insert");
        }
        RuntimeScene scene;require(bool(scene.replace({store,semantic::SemanticGeneration{7}})),"component Scene");
        render::FrameState frame{render::ViewId{1},render::CameraState{{400,250},1,0,render::CameraGeneration{1}},
            {0,0,800,500},{800,500,1600,1000,2,1},semantic::SemanticGeneration{7},SceneRevision{7},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,800,500}};
        render::VisibilityResult visible{.sceneGeneration=frame.sceneGeneration,.sceneReadToken=frame.sceneReadToken,.queryWorldRect=frame.worldViewport,
            .candidatesExamined=copies,.visibleRecords=copies,.backToFront=ids};
        json raw=json::array();std::vector<std::uint64_t> oldTimes,newTimes;
        for(unsigned round=0;round<8;++round)for(const bool old:{true,false,false,true}){
            auto row=measure(old,frame,visible,scene);row["round"]=round;
            (old?oldTimes:newTimes).push_back(row["wall_ns"].get<std::uint64_t>());raw.push_back(std::move(row));
        }
        output["rows"].push_back({{"copies",copies},{"old_complete",stats(oldTimes)},{"full_transient",stats(newTimes)},{"raw",raw}});
    }
    std::cout<<output.dump(2)<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
