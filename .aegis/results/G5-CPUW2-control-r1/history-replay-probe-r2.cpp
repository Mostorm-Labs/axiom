#define main priorUr2dMain
#include "../../../runtime/render/tests/ur2d_selection_up_test.cpp"
#undef main
#include "../../../runtime/interaction/src/partial_erase_geometry.hpp"
#include "../../../runtime/render/src/cpu_worker_protocol.hpp"
#include "../../../runtime/render/src/cpu_exact_path_kernel.hpp"
#include "../../../runtime/render/src/exact_history_path_observer.hpp"
namespace {
std::uint64_t builds=0,construction=0;
void witness(const HistoryPathObservation& o){if(o.historical&&o.event==HistoryPathEvent::kBuild)++builds;if(o.historical&&o.event==HistoryPathEvent::kConstruction)++construction;}
}
int main(int argc,char**argv){try{
 require(argc==2,"output");json rows=json::array();
 for(unsigned n:{1600U,7200U}){
  auto record=source(512,0);record.content.update([](auto& value){auto& st=std::get<semantic::CompactStrokeContent>(value).stroke;st.config.thinning=0;st.knots={{250,230,32768},{450,230,32768}};});std::vector<foundation::WorldPoint> trace;
  for(unsigned i=0;i<=n;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/n)});
  auto geometry=interaction::partialEraseGeometry(trace,record.transform,16);require(bool(geometry),"geometry");
  CpuRequest q{{{1,1,0,1,1,1,1,1}},*geometry};auto request=cpuEncodeRequest(q);CpuExactPathJob job(*geometry);while(!job.ready())require(job.advance(8,2000),"boundedjob");
  const auto size=job.path().writeToMemory(nullptr);std::vector<std::uint8_t> response(128+size);std::copy_n(request.begin(),128,response.begin());
  const auto word=[&](unsigned pos,std::uint32_t value){for(unsigned i=0;i<4;++i)response[pos+i]=value>>(i*8);};word(0,0x31525043);word(96,size);word(100,2);job.path().writeToMemory(response.data()+128);
  SkiaRenderer renderer;RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;renderer.setRenderCacheOptions(options);
  const auto floor=renderer.reserveCpuWorkerGrant(1,64U*1024U*1024U);require(floor!=0,"floor");
  const auto first=renderer.intersectCpuPendingErase(record,*geometry,response,q.identity.words);require(first!=SelectionGeometryHit::kInvalid,"importexact");
  const auto before=renderer.renderCacheWork();record.eraseMasks.push_back({ObjectId::fromUint64(9000),*geometry});
  const auto small=interaction::partialEraseGeometry(std::vector<foundation::WorldPoint>{{300,230},{300,242},{300,266}},record.transform,16);require(bool(small),"small");
  const auto start=std::chrono::steady_clock::now();const auto afterHit=renderer.intersectEraseGeometry(record,*small);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();const auto after=renderer.renderCacheWork();
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});EraseMaskPathCache paths;PreparedGeometryCache body;
  const auto init=paths.query(source(512,0),*small,budget,8U*1024U*1024U,&body,&options);require(init!=SelectionGeometryHit::kInvalid,"initcache");
  builds=construction=0;historyPathObserver=witness;const auto direct=paths.get(*geometry,true);historyPathObserver=nullptr;
  json row{{"segments",n},{"initial_import_decision",unsigned(first)},{"successor_decision",unsigned(afterHit)},{"successor_ms",ms},{"successor_history_builds",after.maskHistory.historyPathBuilds-before.maskHistory.historyPathBuilds},{"successor_history_unions_counted",after.maskHistory.historyUnionOps-before.maskHistory.historyUnionOps},{"get_historical_admitted",bool(direct)},{"get_historical_builds",builds},{"get_historical_constructions",construction},{"local_capacity",8U*1024U*1024U},{"resident_path_bytes",job.path().approximateBytesUsed()},{"raw_geometry_bytes",n*sizeof(semantic::EraseCubicSegment)},{"construction_term_only",n*4096U}};
  rows.push_back(row);std::cout<<row.dump()<<std::endl;require(renderer.releaseCpuWorkerGrant(1,floor),"floorrelease");
 }
 std::ofstream(argv[1])<<rows.dump(2)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
