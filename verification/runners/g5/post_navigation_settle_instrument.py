from pathlib import Path
R=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom');S=Path(__import__('sys').argv[1])
header=r'''#pragma once
#if defined(AXIOM_POST_NAV_SETTLE_TRACE)
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
namespace canvas::foundation::internal::ps {
enum Stage {Owner,Pump,Resource,Navigation,Canonical,Render,Validation,CacheBegin,Bodies,Materials,SceneDraw,Flush,Present,FullBody,Count};
struct Sample {std::uint64_t total=0,self=0,calls=0;};
struct Context {std::array<Sample,Count>times{};std::array<std::uint64_t,48>facts{};};
inline thread_local Context* active=nullptr;
inline std::uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Scope;inline thread_local Scope* top=nullptr;
struct Scope {Context*ctx=active;Scope*parent=top;unsigned stage;std::uint64_t start=0,child=0;explicit Scope(unsigned s):stage(s){if(ctx){start=now();top=this;}}~Scope(){if(!ctx)return;auto n=now()-start;auto&t=ctx->times[stage];t.total+=n;t.self+=n-child;++t.calls;if(parent)parent->child+=n;top=parent;}};
inline void set(unsigned k,std::uint64_t v){if(active)active->facts[k]=v;}
inline void add(unsigned k,std::uint64_t v=1){if(active)active->facts[k]+=v;}
struct Record {Context context{};Context*previous=active;Scope*oldTop=top;std::uint64_t start=now();Record(){active=&context;top=nullptr;}~Record(){auto end=now();active=previous;top=oldTop;
#if defined(__EMSCRIPTEN__)
 char buffer[6144];std::size_t at=0;auto append=[&](const char*fmt,auto...v){if(at>=sizeof(buffer))return;int n=0;if constexpr(sizeof...(v)==0)n=std::snprintf(buffer+at,sizeof(buffer)-at,"%s",fmt);else n=std::snprintf(buffer+at,sizeof(buffer)-at,fmt,v...);if(n>0)at+=n;};
 append("(function(){if(!globalThis.psTraceAccept)return;globalThis.psTraceAccept({start:'%llu',end:'%llu',times:[",(unsigned long long)start,(unsigned long long)end);
 for(unsigned i=0;i<Count;++i){const auto&t=context.times[i];append("%s['%llu','%llu','%llu']",i?",":"",(unsigned long long)t.total,(unsigned long long)t.self,(unsigned long long)t.calls);}append("],facts:[");
 for(unsigned i=0;i<context.facts.size();++i)append("%s'%llu'",i?",":"",(unsigned long long)context.facts[i]);append("]});})()");if(at<sizeof(buffer))emscripten_run_script(buffer);
#endif
 }};
}
#define PS_OWNER() ::canvas::foundation::internal::ps::Record psRecord;::canvas::foundation::internal::ps::Scope psRoot(::canvas::foundation::internal::ps::Owner)
#define PS_SCOPE(n,s) ::canvas::foundation::internal::ps::Scope n(::canvas::foundation::internal::ps::s)
#define PS_SET(n,v) ::canvas::foundation::internal::ps::set(n,v)
#define PS_ADD(n,v) ::canvas::foundation::internal::ps::add(n,v)
#else
#define PS_OWNER() ((void)0)
#define PS_SCOPE(...) ((void)0)
#define PS_SET(...) ((void)0)
#define PS_ADD(...) ((void)0)
#endif
'''
p=S/'runtime/foundation/include/canvas/foundation/internal/post_navigation_settle_trace.hpp';p.write_text(header)
def edit(n,fn):
 p=S/n;s=p.read_text();s='#include "canvas/foundation/internal/post_navigation_settle_trace.hpp"\n'+s;p.write_text(fn(s))
def composition(s):
 a=s.index('bool RuntimeCompositionCore::serviceFrameOpportunity(');t=s[a:];t=t.replace('  out = {};','  PS_OWNER();\n  out = {};',1).replace('  // A capture may be superseded', '''  PS_SET(0,runtime_->cameraGeneration());PS_SET(1,runtime_->sceneRevision());PS_SET(2,runtime_->semanticGeneration().value());PS_SET(3,runtime_->surface().generation);PS_SET(4,runtime_->surface().metricsGeneration);
  // A capture may be superseded''',1)
 t=t.replace('AXIOM_FRAME_TIMING_SCOPE(ownerPumpTiming,owner_pump);','PS_SCOPE(psPump,Pump);AXIOM_FRAME_TIMING_SCOPE(ownerPumpTiming,owner_pump);',1).replace('AXIOM_FRAME_TIMING_SCOPE(resourceTiming,resource_service);','PS_SCOPE(psResource,Resource);AXIOM_FRAME_TIMING_SCOPE(resourceTiming,resource_service);',1).replace('AXIOM_FRAME_TIMING_SCOPE(navTiming,navigation_frame);','PS_SCOPE(psNav,Navigation);AXIOM_FRAME_TIMING_SCOPE(navTiming,navigation_frame);',1).replace('AXIOM_FRAME_TIMING_SCOPE(canonicalTiming,canonical_frame);','PS_SCOPE(psCanonical,Canonical);AXIOM_FRAME_TIMING_SCOPE(canonicalTiming,canonical_frame);',1)
 t=t.replace('  AXIOM_IF_EVENT("nc_route point=outcome', '''  PS_SET(5,out.navigationPending);PS_SET(6,out.navigationPresented);PS_SET(7,out.canonicalRequested);PS_SET(8,out.needsPaint);PS_SET(9,bool(out.canonicalWork));
  AXIOM_IF_EVENT("nc_route point=outcome''',1);return s[:a]+t
edit('runtime/composition/src/runtime_composition.cpp',composition)
def renderer(s):
 a=s.index('BackendSubmissionResult SkiaRenderer::renderFrame(');b=s.index('\nBackendSubmissionResult SkiaRenderer::renderNavigation',a);t=s[a:b];i=t.index('    G5_')if '    G5_'in t else t.index('    const ');t=t[:i]+'    PS_SCOPE(psRender,Render);\n'+t[i:]
 t=t.replace('const auto validation=internal::validateReferencePlan(plan,validationService(),internal::HistoryValidationPolicy::kPublishedImmutable);','const auto validation=[&]{PS_SCOPE(psValidation,Validation);return internal::validateReferencePlan(plan,validationService(),internal::HistoryValidationPolicy::kPublishedImmutable);}();',1)
 t=t.replace('AXIOM_FRAME_TIMING_SCOPE(beginTiming,cache_begin);','PS_SCOPE(psBegin,CacheBegin);AXIOM_FRAME_TIMING_SCOPE(beginTiming,cache_begin);',1)
 t=t.replace('const auto bodies=cache->prepareBodies(*acquired.frame.surface->getCanvas(),plan,allowance);','const auto bodies=[&]{PS_SCOPE(psBodies,Bodies);return cache->prepareBodies(*acquired.frame.surface->getCanvas(),plan,allowance);}();PS_SET(10,unsigned(bodies));',1)
 t=t.replace('cache->prepareWorkingSet(*acquired.frame.surface->getCanvas(),plan,allowance,images_,&textResources_,false);','[&]{PS_SCOPE(psMaterials,Materials);return cache->prepareWorkingSet(*acquired.frame.surface->getCanvas(),plan,allowance,images_,&textResources_,false);}();',1)
 t=t.replace('prepared=cache->prepare(*target,plan,*preparation,images_,&textResources_,&allowance);','prepared=[&]{PS_SCOPE(psMaterials,Materials);return cache->prepare(*target,plan,*preparation,images_,&textResources_,&allowance);}();PS_SET(11,unsigned(prepared));',1)
 t=t.replace('const auto presented=provider.present();','const auto presented=[&]{PS_SCOPE(psPresent,Present);return provider.present();}();',1)
 t=t.replace('    if(!gpu.finish()){','    if(![&]{PS_SCOPE(psFlush,Flush);return gpu.finish();}()){',1)
 return s[:a]+t+s[b:]
edit('runtime/render/src/skia_renderer.cpp',renderer)
def cache(s):
 s=s.replace('    ++work.preparationSlices;', '''    PS_SET(12,preparationCursor_);++work.preparationSlices;''',1)
 s=s.replace('    const auto yield=[&]{flushPreparation();++work.preparationYields;return Preparation::kPending;};','    const auto yield=[&]{PS_SET(12,preparationCursor_);PS_ADD(13,1);flushPreparation();++work.preparationYields;return Preparation::kPending;};')
 return s
edit('runtime/render/src/skia_render_cache.cpp',cache)
def ink(s):
 a=s.index('SkiaInkLayers::BodyPreparation SkiaInkLayers::prepareNavigationBody(');i=s.index(' const auto*compact',a);s=s[:i]+' PS_SCOPE(psFullBody,FullBody);PS_ADD(14,1);\n'+s[i:];s=s.replace('if(allowance.exhausted())return BodyPreparation::kPending;', 'if(allowance.exhausted()){PS_ADD(15,1);return BodyPreparation::kPending;}',1);return s
edit('runtime/render/src/skia_ink_layers.cpp',ink)
def nav(s):
 needle='    work.presented=false;work.pending=false;work.coverageComplete=false;'
 s=s.replace(needle, '''    const auto psBeforeNav=work;auto psNav=ns::Exit{[&]{PS_SET(32,job_?job_->id:0);PS_SET(33,job_?job_->source:0);PS_SET(34,job_?job_->key.region:0);PS_SET(35,work.sourceSteps-psBeforeNav.sourceSteps);PS_SET(36,work.tilesBuilt-psBeforeNav.tilesBuilt);PS_SET(37,work.queries-psBeforeNav.queries);PS_SET(38,work.budgetYields-psBeforeNav.budgetYields);PS_SET(39,work.coverageComplete);PS_SET(40,job_?job_->key.scale+32:0);PS_SET(41,refinement_?refinement_->id:0);}};
'''+needle,1);return s
edit('runtime/render/src/skia_navigation_tiles.cpp',nav)
# Render aggregate on every return; existing counters, no perprimitive clocks.
p=S/'runtime/render/src/skia_renderer.cpp';s=p.read_text();needle='    PS_SCOPE(psRender,Render);';s=s.replace(needle,needle+'''
    const auto psBefore=renderCacheWork();
    auto psObservation=internal::ns::Exit{[&]{const auto w=renderCacheWork();PS_SET(16,w.preparationCoverageSteps-psBefore.preparationCoverageSteps);PS_SET(17,w.preparedAdvances-psBefore.preparedAdvances);PS_SET(18,w.gpuCoverageTiles-psBefore.gpuCoverageTiles);PS_SET(19,w.cpuCoverageTiles-psBefore.cpuCoverageTiles);PS_SET(20,w.maskPrimitiveDraws-psBefore.maskPrimitiveDraws);PS_SET(21,w.maskTileUpdates-psBefore.maskTileUpdates);PS_SET(22,w.bodyTileBuilds-psBefore.bodyTileBuilds);PS_SET(23,w.bodyTileHits-psBefore.bodyTileHits);PS_SET(24,w.rasterizedGroups-psBefore.rasterizedGroups);PS_SET(25,w.hits-psBefore.hits);PS_SET(26,w.entries);PS_SET(27,w.bodyCompileInputs-psBefore.bodyCompileInputs);if(resourceBudget_){PS_SET(28,resourceBudget_->used());PS_SET(29,resourceBudget_->pendingGpuEpochCount());std::size_t pending=0,staging=0;for(unsigned i=0;i<unsigned(resources::ResourceClass::kCount);++i){auto u=resourceBudget_->snapshot(resources::ResourceClass(i));pending+=u.pendingRelease;staging+=u.staging;}PS_SET(30,pending);PS_SET(31,staging);}}};
''',1);# ns::Exit available skia_navigation_tiles.hpp doesn't include navobserver;use local struct
s=s.replace('auto psObservation=internal::ns::Exit{', 'auto psObservation=internal::ns::Exit{');s='#include "navigation_sharing_observation.hpp"\n'+s;p.write_text(s)
