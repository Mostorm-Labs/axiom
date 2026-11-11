#define main hpOwnerFixtureMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "../src/exact_history_path_observer.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "../src/skia_ink_layers.hpp"
#include "include/core/SkSurface.h"
#include <map>
namespace {
std::vector<RuntimeSceneRecord> captured(const char* path){std::ifstream f(path,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(d.ok(),"captured canonical source");std::vector<RuntimeSceneRecord>out;for(const auto&o:d.snapshot->objects){RuntimeSceneRecord r;r.objectId=o.id;r.kind=o.kind;r.kindVersion=o.kind_version;r.content=o.content;r.transform=o.transform;r.eraseMasks=o.erase_masks;r.properties=o.properties;out.push_back(r);}return out;}
json workDelta(const EraseMaskPathCache::Work&a,const EraseMaskPathCache::Work&b){return{{"path_builds",b.builds-a.builds},{"query_unions",b.history.queryUnionOps-a.history.queryUnionOps},{"history_unions",b.history.historyUnionOps-a.history.historyUnionOps},{"local_bytes",b.bytes},{"memo",b.equivalentQueryHits-a.equivalentQueryHits}};}
json observations=json::array();std::vector<semantic::EraseMaskGeometry> observedGeometries;std::size_t observerDropped=0,globalPeak=0,constructionPeak=0,combinedPeak=0;
std::string maskKey(const semantic::EraseMaskGeometry& geometry){
 // Full canonical values remain in the hash-bound snapshot; match by exact
 // geometry comparison below, never use this diagnostic fingerprint as a key.
 std::uint64_t h=1469598103934665603ULL;const auto add=[&](const void*p,std::size_t n){for(std::size_t i=0;i<n;++i)h=(h^static_cast<const unsigned char*>(p)[i])*1099511628211ULL;};
 std::visit([&](const auto& g){if constexpr(requires{g.segments;})for(const auto& s:g.segments){const double a[]={s.p0.position.x,s.p0.position.y,s.p0.radius,s.p1.position.x,s.p1.position.y,s.p1.radius,s.control1.x,s.control1.y,s.control2.x,s.control2.y};add(a,sizeof(a));}else{const auto n=g.path.commands.size();add(&n,sizeof(n));}},geometry);return std::to_string(h);
}
void observe(const HistoryPathObservation&o){if(o.event==HistoryPathEvent::kConstruction){globalPeak=std::max(globalPeak,o.actual);constructionPeak=std::max(constructionPeak,o.estimate);combinedPeak=std::max(combinedPeak,o.local+o.estimate);}if(observations.size()>=4096){++observerDropped;return;}const auto units=o.geometry?std::visit([](const auto& g)->std::size_t{if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},*o.geometry):0;
 std::size_t identity=0;if(o.geometry){auto it=std::find(observedGeometries.begin(),observedGeometries.end(),*o.geometry);if(it==observedGeometries.end()){observedGeometries.push_back(*o.geometry);identity=observedGeometries.size();}else identity=std::size_t(it-observedGeometries.begin())+1;}
 observations.push_back({{"geometry_id",identity},{"kind",unsigned(o.event)},{"entry",reinterpret_cast<std::uintptr_t>(o.entry)},{"geometry",o.geometry?maskKey(*o.geometry):""},{"segments",units},{"local",o.local},{"estimate",o.estimate},{"actual",o.actual},{"cardinality",o.cardinality},{"historical",o.historical}});
}
}
int main(int argc,char**argv){try{
 require(argc==4,"usage capturedhistory output mode");auto records=captured(argv[1]);auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});auto pool=std::make_shared<SourceRetentionPool>();PreparedGeometryCache bodies;bodies.setRetentionPool(pool);auto paths=std::make_shared<EraseMaskPathCache>();paths->setRetentionPool(pool);RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));surface->getCanvas()->save();surface->getCanvas()->clipRect(SkRect::MakeEmpty());SkiaInkLayers layers;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork draw;
 for(const auto&r:records){ReferenceTraversalEntry e;e.record=r;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(r.content)};const auto body=bodies.get(e,options,budget,true);require(body&&body->ready(),"body");bool painted=false,pending=false;(void)layers.draw(*surface->getCanvas(),*surface,e,SkMatrix::I(),SkIRect::MakeWH(256,128),nullptr,options,budget,pool,targets,draw,painted,pending,std::shared_ptr<PreparedGeometryCache>(&bodies,[](auto*){}),body,paths);}
 surface->getCanvas()->restore();json passes=json::array();historyPathObserver=observe;
 for(unsigned pass=0;pass<3;++pass){const auto before=paths->work();paths->beginCohort([&]{(void)layers.retireIdleHistoryPreparation();});json decisions=json::array();
 for(const auto&r:records){std::vector<foundation::WorldPoint> trace{{300+float(pass),230},{300+float(pass),242},{300+float(pass),266}};const auto query=interaction::partialEraseGeometry(trace,r.transform,16);require(bool(query),"local query");decisions.push_back(unsigned(paths->query(r,*query,budget,8U*1024U*1024U,&bodies,&options)));}
 paths->endCohort();const auto after=paths->work();passes.push_back({{"pass",pass},{"work",workDelta(before,after)},{"history_builds",after.history.historyPathBuilds-before.history.historyPathBuilds},{"decisions",decisions},{"global",budget->used()}});}
 historyPathObserver=nullptr;json full=json::array();for(const auto& geometry:observedGeometries){json row=json::array();std::visit([&](const auto&g){if constexpr(requires{g.segments;})for(const auto&s:g.segments)row.push_back({s.p0.position.x,s.p0.position.y,s.p0.radius,s.p1.position.x,s.p1.position.y,s.p1.radius,s.control1.x,s.control1.y,s.control2.x,s.control2.y});},geometry);full.push_back(row);}
 json v{{"complete_geometry_values",full},{"passes",passes},{"events",observations},{"observer_dropped",observerDropped},{"sizeofSegment",sizeof(semantic::EraseCubicSegment)},{"global_peak",globalPeak},{"construction_peak",constructionPeak},{"combined_peak",combinedPeak},{"final_staging",budget->snapshot(resources::ResourceClass::kRecorded).staging}};layers.clear();paths->clear();bodies.clear();targets.clear();v["last_release_bytes"]=budget->used();require(budget->used()==0,"lease leak");std::ofstream(argv[2])<<v.dump(2)<<'\n';std::cout<<passes.dump()<<std::endl;
 if(std::string_view(argv[3])=="--verify")require(passes[2]["history_builds"].get<unsigned>()==0,"RETAINED_EXACT_HISTORY_REBUILT_ON_SUCCESSOR");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
