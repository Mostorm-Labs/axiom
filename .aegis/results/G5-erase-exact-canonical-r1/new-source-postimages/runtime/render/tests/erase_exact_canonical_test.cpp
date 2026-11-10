#define main ecExistingSelectionMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "../src/skia_mask_history.hpp"

int main(int argc,char**argv){try{
 const bool verify=argc>1&&std::string_view(argv[1])=="--verify";
 const auto out=std::filesystem::path(argc>2?argv[2]:"out/erase-exact-canonical");std::filesystem::create_directories(out);
 if(verify){
  auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;
  semantic::SweptCircleMask sweep;
  for(unsigned i=0;i<160;++i)sweep.segments.push_back({{{double(i),100},5},{{double(i+1),100},5},{i+.3,100},{i+.6,100}});
  for(unsigned i=0;i<21;++i)cache.prepareQueryPrefix(sweep,budget,8U*1024U*1024U,8);
  const auto before=cache.work();require(bool(cache.get(sweep)),"prepared sequential query");
  const auto after=cache.work();require(after.history.queryUnionOps==before.history.queryUnionOps,"Up rebuilt complete prepared query prefix");
  require(before.history.queryUnionOps==160,"query prefix changed sequential union count");cache.clear();require(budget->used()==0,"query prefix quota leaked");
 }
 if(argc>1&&std::string_view(argv[1])=="--prefix-cost"){
  json samples=json::array();
  for(unsigned length:{800U,1600U}){
   auto budget=std::make_shared<resources::ResourceBudgetManager>();EraseMaskPathCache cache;semantic::SweptCircleMask sweep;
   std::size_t peak=0;std::uint64_t total=0,copied=0;double maxMs=0;
   for(unsigned i=1;i<=length;++i){const double y=160+170.*i/length,oldy=160+170.*(i-1)/length;
    sweep.segments.push_back({{{420+7*std::sin((i-1)*.17),oldy},16},{{420+7*std::sin(i*.17),y},16},{420,oldy},{420,y}});
    if(i%8)continue;const auto start=std::chrono::steady_clock::now();cache.prepareQueryPrefix(sweep,budget,8U*1024U*1024U,8);
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();total+=ns;maxMs=std::max(maxMs,ns/1.e6);copied+=sweep.segments.size()*sizeof(semantic::EraseCubicSegment);peak=std::max(peak,budget->used());
   }
   auto record=source(7199,0);PreparedGeometryCache bodies;RenderCacheOptions options;const auto t=std::chrono::steady_clock::now();const auto hit=cache.query(record,sweep,budget,8U*1024U*1024U,&bodies,&options);const auto end=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();
   const auto expected=intersectEraseGeometryComponentOracle(record,sweep);require(hit==expected,"long prefix changed independent exact result");cache.clear();bodies.clear();require(budget->used()==0,"long prefix leases");
   samples.push_back({{"moves",length},{"frame_total_ns",total},{"frame_max_ms",maxMs},{"geometry_copy_bytes",copied},{"retained_peak_bytes",peak},{"final_exact_ms",end},{"decision",unsigned(hit)}});
  }
  std::ofstream(out/"prefix-cost.json")<<samples.dump(2)<<'\n';for(const auto& row:samples)std::cout<<row.dump()<<'\n';return 0;
 }
 json rows=json::array();
 for(unsigned copies:{1U,16U,40U})for(unsigned length:{80U,160U,400U}){
  if(copies!=40&&length!=160)continue;
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
  PreparedGeometryCache bodies;EraseMaskPathCache cache;RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
  auto original=source(7199,2);
  for(unsigned round=0;round<3;++round){
   semantic::SweptCircleMask query;
   for(unsigned i=1;i<=length;++i){const double x=395-round*18;
    semantic::Vec2 a{x+4*std::sin((i-1)*.19),150+80.*(i-1)/length},b{x+4*std::sin(i*.19),150+80.*i/length};
    query.segments.push_back({{a,6},{b,6},{a.x+(b.x-a.x)/3,a.y+(b.y-a.y)/3},{a.x+2*(b.x-a.x)/3,a.y+2*(b.y-a.y)/3}});}
   const auto before=cache.work();const auto started=std::chrono::steady_clock::now();json decisions=json::array();
   for(unsigned copy=0;copy<copies;++copy){auto record=original;record.objectId=ObjectId::fromUint64(100+copy);
    // Match the canonical identity divergence produced by a real per-object Up.
    for(unsigned j=2;j<record.eraseMasks.size();++j)record.eraseMasks[j].id=ObjectId::fromUint64(20000+copy*100+j);
    decisions.push_back(unsigned(cache.query(record,query,budget,8U*1024U*1024U,&bodies,&options)));
   }
   const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();const auto after=cache.work();
   if(verify&&round>0&&copies>1)require(after.equivalentQueryHits-before.equivalentQueryHits>=copies-1,"geometry-equivalent canonical-ID forks repeated exact decision");
   rows.push_back({{"copies",copies},{"moves",length},{"round",round},{"decisions",decisions},{"ms",elapsed},
    {"memo_hits",after.equivalentQueryHits-before.equivalentQueryHits},{"history_builds",after.history.primitiveBuilds-before.history.primitiveBuilds},
    {"query_unions",after.history.queryUnionOps-before.history.queryUnionOps},{"history_unions",after.history.historyUnionOps-before.history.historyUnionOps},
    {"query_path_ns",after.history.queryPathNanoseconds-before.history.queryPathNanoseconds},{"history_path_ns",after.history.historyPathNanoseconds-before.history.historyPathNanoseconds},
    {"difference_ns",after.history.differenceNanoseconds-before.history.differenceNanoseconds},{"intersect_ns",after.history.intersectionNanoseconds-before.history.intersectionNanoseconds}});
   original.eraseMasks.push_back({ObjectId::fromUint64(50000+round),query});
  }
  cache.clear();bodies.clear();require(budget->used()==0,"exact history lease release");
 }
 std::ofstream(out/"exact.json")<<rows.dump(2)<<'\n';for(const auto& row:rows)std::cout<<row.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
