#include "canvas/semantic/snapshot.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "../src/selection_region_geometry_internal.hpp"
#include "../src/navigation_mask_plan.hpp"
#include "../src/navigation_mask_observation.hpp"
#include "../src/mask_primitive_observation.hpp"
#include "../src/mask_residency_policy.hpp"
#include "include/core/SkSurface.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace canvas;using namespace canvas::render;using namespace canvas::render::internal;using nlohmann::json;
static void require(bool b,const char*m){if(!b)throw std::runtime_error(m);}
using Clock=std::chrono::steady_clock;static std::uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();}
static json counters(nm::Counts c){return {{"builds",c.pathBuilds},{"draws",c.drawn},{"candidates",c.candidates},{"pages",c.pages},{"sourceUnits",c.sourceUnits},{"reused",mps::state.counts[mps::Reused]},{"eligible",mps::state.counts[mps::Eligible]}};}
static std::uint64_t hash(SkSurface& s){SkPixmap p;require(s.peekPixels(&p),"pixels");std::uint64_t h=1469598103934665603ULL;for(std::size_t i=0;i<p.computeByteSize();++i)h=(h^static_cast<const unsigned char*>(p.addr())[i])*1099511628211ULL;return h;}
static SkMatrix matrix(const semantic::Transform2D&t){SkMatrix m;m.setAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1);return m;}
int main(int argc,char**argv)try{
 require(argc==4,"snapshot mode output");std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(file),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"decode actual Fixed40");require(decoded.snapshot->objects.size()==40,"actual40");
 std::vector<RuntimeSceneRecord> records;std::size_t recordCount=0,segments=0;
 for(const auto&o:decoded.snapshot->objects){RuntimeSceneRecord r;r.objectId=o.id;r.kind=o.kind;r.kindVersion=o.kind_version;r.content=o.content;r.transform=o.transform;r.eraseMasks=o.erase_masks;r.properties=o.properties;recordCount+=r.eraseMasks.size();for(const auto&g:r.eraseMasks)segments+=std::visit([](const auto&v)->std::size_t{if constexpr(requires{v.segments;})return v.segments.size();else return v.path.commands.size();},g.geometry);records.push_back(std::move(r));}
 const std::string mode=argv[2];require(mode=="held"||mode=="released"||mode=="resident"||mode=="streaming","mode");
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{512U*1048576U,1024U*1048576U});EraseMaskPathCache cache;RenderCacheOptions options;options.maxBytes=512U*1048576U;options.sharedMaxBytes=1024U*1048576U;options.maxEntryBytes=128U*1048576U;
#if defined(AXIOM_MPS_OLD_BASELINE)
 const std::size_t cap=8U*1048576U;
#else
 const auto policy=maskResidencyPolicy(options);const auto cap=policy.drawing;cache.setDrawingResidency(cap);cache.setResidentUpperBound(policy.exact);
#endif
 std::vector<std::shared_ptr<MaskHistoryPlan>> canonical;
 if(mode=="held"||mode=="released")for(const auto&r:records)if(!r.eraseMasks.empty()){auto h=cache.history(r.eraseMasks,budget,cap);if(h)canonical.push_back(h);}
 const auto canonicalPlans=canonical.size(),canonicalBytes=cache.work().bytes;
 if(mode=="released")canonical.clear();
 nm::counts={};mps::state.counts={};mps::state.ns={};
 std::vector<std::shared_ptr<NavigationMaskPlan>> plans;
 const auto setupStart=now();
 for(const auto&r:records){if(r.eraseMasks.empty()){plans.push_back({});continue;}auto p=cache.requestNavigationHistory(r.eraseMasks,budget,cap);require(bool(p),"plan request");if(mode=="streaming")p->streaming=true;unsigned n=0;while(cache.advanceNavigationHistory(p)!=EraseMaskPathCache::NavigationMaskStatus::kReady)require(++n<5000,"prepare progress");plans.push_back(p);}
 auto setup=counters(nm::counts);setup["elapsedNs"]=now()-setupStart;setup["usage"]=std::array<std::uint64_t,2>{cache.work().bytes,budget->used()};setup["canonicalPlans"]=canonicalPlans;setup["canonicalBytesBeforeClear"]=canonicalBytes;setup["canonicalPlansHeld"]=canonical.size();setup["phaseNs"]=mps::state.ns;
 auto actual=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,256)),oracle=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,256));require(actual&&oracle,"surface");
 const auto camera=SkMatrix::MakeAll(.765F,0,-196.F,0,.765F,-46.25F,0,0,1);
 oracle->getCanvas()->clear(SK_ColorWHITE);
 for(const auto&r:records){oracle->getCanvas()->save();oracle->getCanvas()->setMatrix(camera);oracle->getCanvas()->concat(matrix(r.transform));ReferenceTraversalEntry e;e.record=r;drawEraseMasksToSkCanvas(*oracle->getCanvas(),e);oracle->getCanvas()->restore();}
 const auto oracleHash=hash(*oracle);json rows=json::array();
 for(unsigned repeat=0;repeat<8;++repeat){actual->getCanvas()->clear(SK_ColorWHITE);const auto before=nm::counts;const auto beforeShared=mps::state.counts;const auto beforePhases=mps::state.ns;const auto start=now();std::size_t calls=0;
  for(std::size_t i=0;i<records.size();++i){if(!plans[i])continue;actual->getCanvas()->save();actual->getCanvas()->setMatrix(camera);actual->getCanvas()->concat(matrix(records[i].transform));NavigationMaskReplay replay;unsigned turns=0;
   for(;;){auto s=cache.replayNavigationHistory(plans[i],*actual->getCanvas(),replay,64);++calls;require(s!=EraseMaskPathCache::NavigationMaskStatus::kInvalid&&++turns<10000,"replay");if(s==EraseMaskPathCache::NavigationMaskStatus::kReady)break;}
   actual->getCanvas()->restore();}
  const auto elapsed=now()-start;auto row=counters(nm::counts);row["builds"]=nm::counts.pathBuilds-before.pathBuilds;row["draws"]=nm::counts.drawn-before.drawn;row["candidates"]=nm::counts.candidates-before.candidates;row["reused"]=mps::state.counts[mps::Reused]-beforeShared[mps::Reused];row["eligible"]=mps::state.counts[mps::Eligible]-beforeShared[mps::Eligible];const auto observed=hash(*actual);require(observed==oracleHash,"independent unchanged legacy pixel hash differs");std::array<std::uint64_t,mps::PhaseCount> phases{};std::uint64_t phaseTotal=0;for(unsigned k=0;k<mps::PhaseCount;++k){phases[k]=mps::state.ns[k]-beforePhases[k];phaseTotal+=phases[k];}row["phaseNs"]=phases;row["exclusivePhaseTotalNs"]=phaseTotal;row["unattributedCallNs"]=elapsed-phaseTotal;row["usagePartition"]=mps::state.usage;row["overflow"]=mps::state.overflow;row["repeat"]=repeat;row["elapsedNs"]=elapsed;row["pixelHash"]=std::to_string(observed);row["independentLegacyEqual"]=true;row["calls"]=calls;row["usage"]=std::array<std::uint64_t,2>{cache.work().bytes,budget->used()};rows.push_back(row);
 }
 json out={{"snapshot",argv[1]},{"mode",mode},{"objects",records.size()},{"maskRecords",recordCount},{"sourceUnits",segments},{"setup",setup},{"replays",rows},{"originalLocalCap",cap},{"budgetLimits",{512,1024}},{"unmodifiedLegacyPixelHash",std::to_string(oracleHash)}};
 out["phaseNames"]={"request","prepare","select","lookup","allocate","build","retire","draw"};out["usageNames"]={"cap","localTotal","navTotal","currentIndex","currentPaths","globalUsed","sourceData","sourceIndex","sourceLocal","canonicalPrimitives","canonicalMetadata","exact","query","imported","refusal","navMetadata","navIndex","navPrimitives","recordedStaging","recordedPending"};out["overflow"]=mps::state.overflow;plans.clear();canonical.clear();cache.clear();out["lastReleaseBytes"]=budget->used();require(budget->used()==0,"leak");std::ofstream(argv[3])<<out.dump(2)<<'\n';std::cout<<json{{"mode",mode},{"objects",40},{"maskRecords",recordCount},{"segments",segments},{"replays",8},{"pixelEqual",true},{"lastReleaseBytes",budget->used()}}.dump()<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
