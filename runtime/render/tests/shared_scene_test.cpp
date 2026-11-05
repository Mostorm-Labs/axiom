#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "../src/skia_navigation_tiles.hpp"
#include "../src/skia_render_cache_internal.hpp"
#include "../src/chunk_query.hpp"
#endif

using namespace canvas;
using namespace canvas::render;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T> decltype(auto) value(const T& v){if constexpr(requires{v.get();})return v.get();else return (v);}
semantic::ObjectRecord line(std::uint64_t id,unsigned count=65536){
    semantic::ObjectRecord o;o.id=ObjectId::fromUint64(id);o.kind=semantic::ObjectKind::kVectorStroke;o.kind_version=3;
    semantic::CompactStrokeRecord s;s.config.size=8;s.seed=42;s.knots.reserve(count);
    for(unsigned i=0;i<count;++i)s.knots.push_back({double(i),8.+double(i%3),32768});
    o.content=semantic::CompactStrokeContent{std::move(s)};return o;
}
const auto& knots(const RuntimeSceneRecord& r){return std::get<semantic::CompactStrokeContent>(value(r.content)).stroke.knots;}
FrameState frame(){return {ViewId{7},CameraState{{0,0},1,0,CameraGeneration{1}},
    {300,-1,310,20},{800,600,1600,1200,2,2},semantic::SemanticGeneration{9},SceneRevision{9},SurfaceGeneration{3},MetricsGeneration{4},FrameId{1}};}
void payloadSharing(){
    auto object=line(1);semantic::ReferenceObjectStore store;
    check(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"insert source");
    RuntimeScene runtime;auto initial=runtime.replace({store,semantic::SemanticGeneration{9}});check(bool(initial),"scene source");
    VisibilityResult visible{.sceneGeneration=semantic::SemanticGeneration{9},.sceneReadToken=SceneRevision{9},
        .queryWorldRect={300,-1,310,20},.visibleRecords=1,.backToFront={object.id}};
    auto a=DirectReferenceSource::buildNavigation(frame(),visible,runtime);check(bool(a),"source A");
    auto b=DirectReferenceSource::buildNavigation(frame(),visible,runtime);check(bool(b),"source B");
    const auto& canonical=knots(*runtime.find(object.id));
    check(knots(a.value().entries.front().record).data()==canonical.data(),"reference record duplicated immutable Scene knots");
    const auto& command=std::get<CompactStrokeReferenceCommand>(a.value().entries.front().command);
    check(value(command.content).stroke.knots.data()==canonical.data(),"reference command duplicated immutable Scene knots");
    check(knots(b.value().entries.front().record).data()==canonical.data(),"second tile source duplicated immutable Scene knots");
    auto copy=a.value();check(knots(copy.entries.front().record).data()==canonical.data(),"frame source copy duplicated immutable Scene knots");
}
void snapshotSharing(){
    const auto object=line(1);auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    const auto snapshot=projection;
    check(snapshot.records.data()==std::as_const(projection.records).data(),"read-only Scene snapshot copied the record table");
    const auto* old=knots(snapshot.records.front()).data();
    auto changed=object;std::get<semantic::CompactStrokeContent>(changed.content).stroke.knots.front().x=77;
    projection.records.front().content=changed.content;
    check(knots(snapshot.records.front()).front().x==0&&knots(snapshot.records.front()).data()==old,"snapshot mutation corrupted retained reader");
    check(knots(projection.records.front()).front().x==77,"new snapshot lost changed content");
}
void indexedSelection(){
    for(unsigned count:{65536U,262144U}){
        auto object=line(1,count);semantic::ReferenceObjectStore store;
        check(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"indexed source insert");
        RuntimeScene runtime;check(bool(runtime.replace({store,semantic::SemanticGeneration{9}})),"indexed source projection");
        Scene scene(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>(256));
        SceneRecord r{.objectId=object.id,.orderKey=SceneOrderKey{1},.kind=SceneObjectKind::kVectorStroke,
            .flags=SceneRecordFlags::kVisible,.worldBounds={0,0,float(count),20},.contentRevision=ContentRevision{1},
            .renderPayload={1,1},.hitGeometry={1,1}};
        check(bool(scene.replace({SceneRevision{9},{r}})),"indexed bounds source");
        const auto result=VisibilityResolver::resolve(frame(),scene,runtime);check(bool(result),"indexed visibility");
        check(result.value().chunkSelections.size()==1&&result.value().chunkSelections.front().chunkIndices==std::vector<std::uint32_t>{1},
            "indexed sparse query changed chunk selection");
        check(result.value().chunkCandidatesExamined<=16,"sparse viewport still scans every source chunk");
        std::cout<<"{\"knots\":"<<count<<",\"chunk_candidates\":"<<result.value().chunkCandidatesExamined<<",\"index_nodes\":"<<result.value().chunkIndexNodesVisited<<"}\n";
    }
}

void retainedReadersAndVersions(){
    auto object=line(1);semantic::ReferenceObjectStore store;
    check(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"reader source insert");
    RuntimeScene runtime;check(bool(runtime.replace({store,semantic::SemanticGeneration{1}})),"reader projection");
    const auto reader=runtime.records();const auto* storage=knots(reader.front()).data();const auto index=reader.front().geometryIndex;
    object.transform.tx=12;check(semantic::internal::ObjectStoreMutator::replaceExisting(store,object),"transform source");
    check(bool(runtime.apply({store,semantic::SemanticGeneration{2}})),"transform projection");
    check(knots(*runtime.find(object.id)).data()==storage&&runtime.find(object.id)->geometryIndex==index,"transform invalidated unchanged immutable geometry");
    std::get<semantic::CompactStrokeContent>(object.content).stroke.knots.front().pressure=123;
    check(semantic::internal::ObjectStoreMutator::replaceExisting(store,object),"changed source");
    check(bool(runtime.apply({store,semantic::SemanticGeneration{3}})),"changed projection");
    check(!index->matches(*runtime.find(object.id))&&runtime.find(object.id)->geometryIndex->key(0)==index->key(0),
        "derived key/source lifetime invalidation mismatch");
    check(reader.size()==1&&knots(reader.front()).data()==storage&&knots(reader.front()).front().pressure==32768,
        "successive publications changed a retained RuntimeScene reader");
    Scene scene(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>(256));
    SceneRecord r{.objectId=object.id,.orderKey=SceneOrderKey{1},.flags=SceneRecordFlags::kVisible,
        .worldBounds={0,0,100,20},.contentRevision=ContentRevision{1},.renderPayload={1,1},.hitGeometry={1,1}};
    check(bool(scene.replace({SceneRevision{1},{r}})),"reader Scene");const auto old=scene.read();
    for(unsigned generation=2;generation<5;++generation){auto next=r;next.objectId=ObjectId::fromUint64(generation);
        check(bool(scene.replace({SceneRevision{generation},{next}})),"reader Scene successor");}
    check(old.revision()==SceneRevision{1}&&old.find(r.objectId)&&*old.find(r.objectId)==r,
        "successive Scene publications invalidated retained reader");
}
#if defined(CANVAS_RENDER_HAS_SKIA)
WorldRect independentBounds(WorldRect b,const semantic::Transform2D& t){
    double l=INFINITY,top=INFINITY,r=-INFINITY,bottom=-INFINITY;
    for(double x:{double(b.left),double(b.right)})for(double y:{double(b.top),double(b.bottom)}){
        const float px=float(t.a*x+t.c*y+t.tx),py=float(t.b*x+t.d*y+t.ty);
        l=std::min(l,double(px));r=std::max(r,double(px));top=std::min(top,double(py));bottom=std::max(bottom,double(py));}
    return {float(l),float(top),float(r),float(bottom)};
}
void indexLinearParity(){
    auto object=line(1);semantic::ReferenceObjectStore store;check(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"parity source");
    RuntimeScene runtime;check(bool(runtime.replace({store,semantic::SemanticGeneration{1}})),"parity projection");
    auto record=*runtime.find(object.id);
    const semantic::Transform2D poses[]={{},{.8,.2,-.15,1.1,100,-40},{-1,0,.25,.75,65536,-9},
        {1e-6,0,0,1e-6,10,20},{1,0,0,1,1e9,-1e9},{1,1,1,1,0,0}};
    for(const auto& pose:poses)for(unsigned i=0;i<100;++i)for(float margin:{0.F,3.F}){
        record.transform=pose;const float x=float((i*977)%65536);
        const auto region=independentBounds({x-5,4,x+5,12},pose);
        std::vector<std::uint32_t> expected,actual;
        for(const auto& chunk:std::as_const(record.geometryChunks)){
            const auto b=independentBounds(chunk.bounds,pose);
            if(b.right+margin>region.left&&b.left-margin<region.right&&b.bottom+margin>region.top&&b.top-margin<region.bottom)expected.push_back(chunk.index);
        }
        internal::ChunkQueryWork work;internal::queryChunks(record,region,margin,work,[&](const auto& c){actual.push_back(c.index);});
        check(actual==expected,"indexed query differs from independent linear affine oracle");
    }
    record.transform={};std::vector<GeometryChunk> overlap;
    for(unsigned i=0;i<128;++i)overlap.push_back({i,i*256,256,{0,0,10,10},"overlap"});
    record.geometryChunks=std::move(overlap);record.geometryIndex=scene::buildGeometryChunkIndex(record);
    internal::ChunkQueryWork work;unsigned selected=0;
    internal::queryChunks(record,{4,4,6,6},0,work,[&](const auto&){++selected;});
    check(selected==128&&work.candidates==128,"overlap query dropped valid chunks");
    const auto stale=record.geometryIndex;record.content=object.content;
    check(!stale->matches(record),"source replacement retained stale index binding");
    auto unique=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();
    const auto uniqueIndex=scene::buildGeometryChunkIndex(unique);
    unique.content.update([](auto& c){std::get<semantic::CompactStrokeContent>(c).stroke.knots.front().pressure=123;});
    check(!uniqueIndex->matches(unique),"unique staged content edit reused a stale immutable source version");
    record.geometryChunks.front().bounds.left=NAN;
    internal::ChunkQueryWork invalid;internal::queryChunks(record,{4,4,6,6},0,invalid,[&](const auto&){});
    check(invalid.fallbacks>0&&invalid.fullObject,"invalid chunk/index state did not use safe fallback");
}
#endif

#if defined(CANVAS_RENDER_HAS_SKIA)
void idleWorkBufferReclaim(){
    auto object=line(71,256);
    std::get<semantic::CompactStrokeContent>(object.content).stroke.config.algorithmVersion=4;
    auto record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{9}).records.front();
    record.geometryIndex=scene::buildGeometryChunkIndex(record);
    ReferenceTraversalEntry entry{record,true,CompactStrokeReferenceCommand{
        scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)}};
    RasterSkiaSurfaceProvider surface;check(surface.resize(256,128).code==BackendSubmissionCode::kAccepted,"scratch target");
    FrameState state{ViewId{7},CameraState{{0,0},1,0,CameraGeneration{1}}, {0,0,256,128},{256,128,256,128,1,1},
        semantic::SemanticGeneration{9},SceneRevision{9},SurfaceGeneration{surface.generation()},MetricsGeneration{4},FrameId{1}};
    ReferenceDrawList list{state};list.viewportClip={0,0,256,128};list.entries={entry};FramePlan plan{state,std::move(list)};
    constexpr std::size_t soft=1024*1024;
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{soft,3*soft});
    auto pool=std::make_shared<internal::SourceRetentionPool>();internal::SkiaRenderCache cache;
    cache.budget=budget;cache.sourcePool=pool;
    auto acquired=surface.acquire();cache.begin(plan,&surface,*acquired.frame.surface,0);
    RenderPreparationBudget slice;slice.maxCoverageSteps=1;slice.maxMicroseconds=1000000;
    using Progress=internal::SkiaRenderCache::Preparation;
    auto progress=cache.prepare(*acquired.frame.surface->getCanvas(),plan,slice,nullptr,nullptr);
    check(progress==Progress::kPending,"scratch guard did not retain an active raster job");
    const auto busy=budget->used();check(!cache.releaseIdleWorkBuffers(state,{},&surface,0)&&budget->used()==busy,"active raster buffers released before its checkpoint retired");
    for(unsigned i=0;progress==Progress::kPending&&i<256;++i)
        progress=cache.prepare(*acquired.frame.surface->getCanvas(),plan,slice,nullptr,nullptr);
    check(progress==Progress::kReady,"scratch fixture did not complete canonical preparation");
    check(budget->used()<soft,"scratch fixture exceeded its soft limit before pressure");
    const auto token=budget->reserve(resources::ResourceClass::kRaster,soft-budget->used()-128*1024,true);
    check(bool(token),"scratch pressure admission");resources::ResourceLease pressure(budget,*token);
    internal::PreparedGeometryCache geometry;geometry.setRetentionPool(pool);
    check(!geometry.get(entry,RenderCacheOptions{},budget),"scratch fixture did not deny optional prepared paths");
    const auto before=budget->used();check(cache.releaseIdleWorkBuffers(state,{},&surface,0)&&budget->used()<before,"idle canonical scratch kept navigation behind the soft cap");
    const auto reclaimed=before-budget->used();
    auto prepared=geometry.get(entry,RenderCacheOptions{},budget);
    std::cout<<"{\"reclaimed_idle_bytes\":"<<reclaimed<<",\"soft_limit\":"<<soft<<"}\n";
    check(prepared&&prepared->isContour()&&budget->used()<=soft,"idle buffer reclamation did not recover prepared paths within the same budget");
    prepared.reset();geometry.clear();pressure.reset();cache.clear();surface.release();
    check(budget->used()==0,"idle-buffer reclaim leaked retained sources or work buffers");
}
void sourceRetention(){
    auto object=line(1);semantic::ReferenceObjectStore store;
    check(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"retention source insert");
    RuntimeScene runtime;check(bool(runtime.replace({store,semantic::SemanticGeneration{9}})),"retention projection");
    RasterSkiaSurfaceProvider surface;check(surface.resize(256,128).code==BackendSubmissionCode::kAccepted,"retention surface");
    FrameState state{ViewId{7},CameraState{{0,0},1,0,CameraGeneration{1}}, {0,0,256,128},{256,128,256,128,1,1},
        semantic::SemanticGeneration{9},SceneRevision{9},SurfaceGeneration{surface.generation()},MetricsGeneration{4},FrameId{1}};
    NavigationSourceQuery query=[&](const WorldRect& region,std::vector<ReferenceTraversalEntry>& output){
        VisibilityResult visible{.sceneGeneration=state.sceneGeneration,.sceneReadToken=state.sceneReadToken,
            .queryWorldRect=region,.visibleRecords=1,.backToFront={object.id}};
        auto source=DirectReferenceSource::buildNavigation(state,visible,runtime);if(!source)return false;
        output=std::move(source.value().entries);return true;
    };
    auto budget=std::make_shared<resources::ResourceBudgetManager>();
    {
        internal::SkiaNavigationTiles tiles;RenderCacheOptions options;options.navigationMaxQueries=1;
        options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
        check(tiles.render(surface,{1,0,0},state,query,0,options,budget).code==BackendSubmissionCode::kAccepted,"first retention query");
        const auto before=budget->used(resources::ResourceClass::kRecorded);const auto queries=tiles.work.queries;
        check(tiles.render(surface,{1,-512,0},state,query,0,options,budget).code==BackendSubmissionCode::kAccepted,"second retention query");
        const auto after=budget->used(resources::ResourceClass::kRecorded);
        check(tiles.work.queries>queries,"retention oracle did not query a second tile");
        check(after<=before+65536,"second tile charged another whole immutable stroke payload");
        std::cout<<"{\"first_recorded_bytes\":"<<before<<",\"second_recorded_bytes\":"<<after<<"}\n";
    }
    check(budget->used()==0,"retired navigation sources leaked their shared budget lease");
    std::vector<ReferenceTraversalEntry> entries;check(query({0,0,512,128},entries)&&entries.size()==1,"pool source");
    {
        auto sharedPool=std::make_shared<internal::SourceRetentionPool>();internal::SkiaRenderCache canonical;
        canonical.budget=budget;canonical.sourcePool=sharedPool;
        VisibilityResult visible{.sceneGeneration=state.sceneGeneration,.sceneReadToken=state.sceneReadToken,
            .queryWorldRect={0,0,256,128},.visibleRecords=1,.backToFront={object.id}};
        auto draw=DirectReferenceSource::buildNavigation(state,visible,runtime);check(bool(draw),"canonical retention source");
        draw.value().viewportClip={0,0,256,128};FramePlan plan{state,std::move(draw.value())};
        auto acquired=surface.acquire();check(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"canonical retention acquire");
        canonical.begin(plan,&surface,*acquired.frame.surface,0);surface.release();
        const auto sourceBytes=sharedPool->statistics().bytes;check(sourceBytes>0,"canonical source was not admitted");
        {
            internal::SkiaNavigationTiles tiles;RenderCacheOptions options;options.navigationMaxQueries=1;
            options.navigationMaxSteps=1;options.navigationMicroseconds=1000000;
            check(tiles.render(surface,{1,0,0},state,query,0,options,budget,false,nullptr,nullptr,nullptr,{},sharedPool).code==BackendSubmissionCode::kAccepted,"shared canonical/navigation query");
            check(sharedPool->statistics().bytes==sourceBytes,"canonical and navigation charged the same source twice");
            canonical.clear();check(sharedPool->statistics().bytes==sourceBytes,"canonical retirement released in-flight navigation source");
        }
        check(budget->used()==0,"shared canonical/navigation source leaked budget");
    }
    internal::SourceRetentionPool pool;auto a=pool.get(entries.front(),budget,true);check(a.has_value(),"source pool admission");
    const auto charged=budget->used();auto same=pool.get(entries.front(),budget,true);
    check(same.has_value()&&budget->used()==charged,"same immutable source double-charged its owner");
    auto other=std::make_shared<resources::ResourceBudgetManager>();auto b=pool.get(entries.front(),other,true);
    check(b.has_value()&&other->used()==charged&&budget->used()==charged,"source lease crossed budget owner");
    a.reset();check(budget->used()==charged,"in-flight source lease was released early");same.reset();
    check(budget->used()==0,"last old-owner source retained its lease");b.reset();check(other->used()==0,"new-owner source retained its lease");
    auto denied=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
    check(!pool.get(entries.front(),denied,true)&&denied->used()==0,"denied shared source bypassed admission");
}
#endif

}
int main(int argc,char** argv){try{
    if(argc<2||std::string_view(argv[1])=="--payload")payloadSharing();
    if(argc<2||std::string_view(argv[1])=="--snapshot")snapshotSharing();
    if(argc<2||std::string_view(argv[1])=="--index")indexedSelection();
#if defined(CANVAS_RENDER_HAS_SKIA)
    if(argc<2||std::string_view(argv[1])=="--retention"){sourceRetention();idleWorkBufferReclaim();}
#endif
    if(argc<2||std::string_view(argv[1])=="--readers")retainedReadersAndVersions();
#if defined(CANVAS_RENDER_HAS_SKIA)
    if(argc<2||std::string_view(argv[1])=="--parity")indexLinearParity();
#endif
    std::cout<<"shared Scene/source PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
