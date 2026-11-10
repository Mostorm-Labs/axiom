#define main g5OriginalCanonicalTestMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/render_memory_budget.hpp"
#include "../src/transient_display_state.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

int main(){try{
 const auto configured=internal::configuredRenderCacheOptions();
 constexpr std::size_t mib=1024U*1024U;
 require(configured.maxBytes==std::size_t(AXIOM_RENDER_CACHE_MIB)*mib&&
     configured.sharedMaxBytes==std::size_t(AXIOM_RENDER_SHARED_MIB)*mib&&
     configured.maxEntryBytes==std::size_t(AXIOM_RENDER_ENTRY_MIB)*mib,"configured limits");
 const RenderCacheOptions conservative;
 require(conservative.maxBytes==32*mib&&conservative.sharedMaxBytes==96*mib&&conservative.maxEntryBytes==8*mib,"explicit conservative profile changed");
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
 auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
 completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"actual budget Metal context");
 {
 NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"actual budget surface");
 auto entry=source();entry.record.kind=semantic::ObjectKind::kVectorStroke;entry.record.kindVersion=3;
 entry.record.content.update([](auto& value){std::get<semantic::CompactStrokeContent>(value).stroke.knots.resize(16);});
 entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
 const auto plan=renderPlan(entry,1,1);EditingOverlay overlay{plan.frame};
 SkiaRenderer renderer;
 for(unsigned i=0;;++i){require(i<512,"default budget preparation progress");
  const auto result=renderer.renderTransientOverlay(target,plan,&overlay);
  completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
  if(result.code==BackendSubmissionCode::kAccepted)break;
  require(result.code==BackendSubmissionCode::kRetry,"default budget render rejected");
 }
 auto budget=internal::transientDisplayBudget.lock();require(bool(budget),"actual renderer budget witness");
 const auto used=budget->used();require(used<configured.maxBytes,"default renderer cache already exhausted");
 auto soft=budget->reserve(resources::ResourceClass::kRecorded,configured.maxBytes-used,false);
 require(bool(soft)&&!budget->reserve(resources::ResourceClass::kRecorded,1,false),"actual renderer optional limit");
 budget->release(*soft);
 auto hard=budget->reserve(resources::ResourceClass::kRecorded,configured.sharedMaxBytes-used,true);
 require(bool(hard)&&!budget->reserve(resources::ResourceClass::kRecorded,1,true),"actual renderer shared limit");
 budget->release(*hard);
 renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
 require(budget->used()==0,"default budget resource retirement");
 }
 message(pool,"drain");
#endif
 std::cout<<nlohmann::json{{"optional_mib",configured.maxBytes/mib},{"shared_mib",configured.sharedMaxBytes/mib},
     {"entry_mib",configured.maxEntryBytes/mib},{"actual_renderer_admission_checked",true},{"leases_after_clear",0}}.dump()<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
