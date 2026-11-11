// Actual SkiaInkLayers owns the draw-preparation pins in this regression.
#define main ownerExistingFixtureMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "canvas/semantic/snapshot.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "../src/skia_mask_history.hpp"
namespace {
std::vector<RuntimeSceneRecord> captured(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(d.ok(),"captured actual source");std::vector<RuntimeSceneRecord>out;for(const auto&o:d.snapshot->objects){RuntimeSceneRecord r;r.objectId=o.id;r.kind=o.kind;r.kindVersion=o.kind_version;r.content=o.content;r.transform=o.transform;r.eraseMasks=o.erase_masks;r.properties=o.properties;out.push_back(r);}return out;}
semantic::EraseMaskGeometry queryFor(const RuntimeSceneRecord&r,unsigned n,unsigned round){std::vector<foundation::WorldPoint> trace{{float(420-round*18),160}};for(unsigned i=1;i<=n;++i)trace.push_back({float(420-round*18+7*std::sin(i*.17)),float(160+170.*i/n)});return *interaction::partialEraseGeometry(trace,r.transform,16);}
json workDelta(const EraseMaskPathCache::Work&a,const EraseMaskPathCache::Work&b){return{{"memo_hits",b.equivalentQueryHits-a.equivalentQueryHits},{"history_denied",b.history.denied-a.history.denied},{"path_builds",b.builds-a.builds},{"query_unions",b.history.queryUnionOps-a.history.queryUnionOps},{"history_unions",b.history.historyUnionOps-a.history.historyUnionOps},{"local_bytes",b.bytes}};}
}
#include "../src/skia_ink_layers.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
int main(int argc,char** argv){try{
 require(argc==4,"usage initial history output");
 auto records=captured(argv[2]);
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
 auto pool=std::make_shared<SourceRetentionPool>();auto geometry=std::make_shared<PreparedGeometryCache>();geometry->setRetentionPool(pool);
 auto histories=std::make_shared<EraseMaskPathCache>();histories->setRetentionPool(pool);
 RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,128));require(bool(surface),"owner raster target");
 SkiaInkLayers layers;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork work;
 // Empty clip still enters the real source/history admission owner, while
 // avoiding an unrelated full forty-object pixel workload for pressure RED.
 surface->getCanvas()->save();surface->getCanvas()->clipRect(SkRect::MakeEmpty());
 unsigned admitted=0;for(const auto& record:records){ReferenceTraversalEntry e;e.record=record;e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)};
  bool painted=false,pending=false;const auto body=geometry->get(e,options,budget,true);require(body&&body->ready(),"actual body warm");
  if(layers.draw(*surface->getCanvas(),*surface,e,SkMatrix::I(),SkIRect::MakeWH(256,128),nullptr,options,budget,pool,targets,work,painted,pending,geometry,body,histories))++admitted;
 }
 surface->getCanvas()->restore();require(admitted>=16,"real consumer did not retain history pressure");
 const auto localBefore=histories->work();
#if defined(AXIOM_EXACT_OWNER_RESIDENCY)
 std::size_t retired=0,callbacks=0;
 histories->beginCohort([&]{++callbacks;retired=layers.retireIdleHistoryPreparation();});
#else
 const auto retired=0U;
 const auto callbacks=0U;
 histories->beginCohort();
#endif
 json decisions=json::array();const auto start=std::chrono::steady_clock::now();
 for(const auto& record:records){auto query=queryFor(record,160,5);decisions.push_back(unsigned(histories->query(record,query,budget,8U*1024U*1024U,geometry.get(),&options)));}
 histories->endCohort();const auto after=histories->work();
#if defined(AXIOM_EXACT_OWNER_RESIDENCY)
 require(callbacks==1,"pressure retirement was not once per cohort");
#endif
 json result{{"admitted_draw_owners",admitted},{"retired_owner_refs",retired},{"pressure_callbacks",callbacks},{"local_before",localBefore.bytes},{"local_after",after.bytes},
  {"query_work",workDelta(localBefore,after)},{"decisions",decisions},{"query_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}};
 layers.clear();histories->clear();geometry->clear();targets.clear();require(budget->used()==0,"real owner last lease leak");result["last_release_bytes"]=budget->used();
 std::ofstream(argv[3])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';
 // The captured final history has13 complete repeated bases. The old actual
 // owner measured6hits/11denials; this is the same observable target on both
 // versions, independent of whether a retirement method exists.
 require(after.equivalentQueryHits-localBefore.equivalentQueryHits>=13,"real owner pressure still loses equivalent exact results");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
