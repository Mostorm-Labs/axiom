#include <algorithm>
#include "gpu_resource_retirement.hpp"
#include "webgl_context_liveness.hpp"
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>
using namespace canvas;
using namespace canvas::render;
using internal::GpuResourceRetirement;
namespace {
struct Completion final : GpuCompletionService {
    std::shared_ptr<GpuCompletionState> value=std::make_shared<GpuCompletionState>();
    unsigned polls=0;
    std::function<void()> onPoll;
    void service() noexcept override {++polls;if(onPoll)onPoll();}
    std::shared_ptr<const GpuCompletionState> state()const noexcept override{return value;}
};
// Models the pinned Emscripten registry probe, including a live lost context
// with null attributes/zero drawing-buffer dimensions. The production helper
// is also used by WebGlSurfaceProvider; real browser integration is separate.
struct WebGlCompletion final : GpuCompletionService {
    std::shared_ptr<GpuCompletionState> value=std::make_shared<GpuCompletionState>();
    int handle=7,width=800,height=500;
    bool registered=true,lost=false;
    unsigned registryQueries=0,lossQueries=0;
    void service() noexcept override {
        if(value->abandoned.load())return;
        const auto status=internal::probeWebGlContext(handle,
            [&](int,int* w,int* h){++registryQueries;*w=width;*h=height;return registered;},
            [&](int){++lossQueries;return lost;});
        if(status==internal::WebGlContextLiveness::kAlive)return;
        if(status==internal::WebGlContextLiveness::kPhysicalLost)value->physicalLost=true;
        value->abandoned=true;
    }
    std::shared_ptr<const GpuCompletionState> state()const noexcept override{return value;}
};
template<class T> void submitted(T& t){
    if constexpr(requires{t.submitted(true);}) t.submitted(true);
}
}
int main(){
    unsigned failures=0;
    const auto check=[&](bool value,const char* name){
        std::printf("{\"case\":\"%s\",\"pass\":%s}\n",name,value?"true":"false");
        failures+=!value;
    };
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto s=std::make_shared<WebGlCompletion>();
        auto t=q.begin(b,s);t->end();submitted(*t);
        const auto id=b->reserve(resources::ResourceClass::kScratch,4096,true);
        check(bool(id),"webgl_loss_quota_admitted");resources::ResourceLease lease(b,*id);lease.reset();
        s->lost=true;s->width=s->height=0;q.service();
        check(s->value->physicalLost.load()&&s->value->abandoned.load(),
              "registered_lost_context_with_zero_buffer_is_physical_loss");
        // A loss first observed during polling is retired at the next existing
        // owner opportunity, rather than by reentering/changing the queue.
        q.service();
        check(b->gpuEpochRetired(t->epoch)&&b->used()==0&&!q.blocked(),
              "physical_loss_releases_affected_lifetime_without_owner_close");
    }
    for(const int handle:{0,-1,7}){
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto s=std::make_shared<WebGlCompletion>();auto t=q.begin(b,s);t->end();submitted(*t);
        s->registryQueries=s->lossQueries=0;s->handle=handle;s->registered=false;s->lost=true;q.service();
        check(s->value->abandoned.load()&&!s->value->physicalLost.load()&&s->lossQueries==0,
              "deleted_or_invalid_handle_reported_lost_remains_unknown");
        check(s->registryQueries==(handle>0?1U:0U)&&!b->gpuEpochRetired(t->epoch)&&q.blocked(),
              "unknown_registry_cannot_grant_physical_completion");
        t->finish();q.service();check(!b->gpuEpochRetired(t->epoch),"unknown_old_callback_does_not_release_quota");b->close();
    }
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto s=std::make_shared<WebGlCompletion>();auto t=q.begin(b,s);t->end();submitted(*t);
        s->width=s->height=0;q.service();s->width=1600;s->height=1000;q.service();
        check(!s->value->physicalLost.load()&&!s->value->abandoned.load()&&!b->gpuEpochRetired(t->epoch),
              "logical_resize_and_zero_dimensions_do_not_imply_context_loss");
        t->finish();q.service();check(b->gpuEpochRetired(t->epoch),"healthy_context_keeps_normal_callback_completion");
    }
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto old=std::make_shared<WebGlCompletion>();auto a=q.begin(b,old);a->end();submitted(*a);
        old->lost=true;q.service();
        auto fresh=std::make_shared<WebGlCompletion>();auto n=q.begin(b,fresh);n->end();submitted(*n);
        a->finish();q.service();
        check(old->handle==fresh->handle&&old->value->lifetime!=fresh->value->lifetime,
              "reused_numeric_handle_has_distinct_retained_context_lifetime");
        check(b->gpuEpochRetired(a->epoch)&&!b->gpuEpochRetired(n->epoch)&&
              !fresh->value->physicalLost.load()&&!fresh->value->abandoned.load(),
              "old_loss_and_late_callback_cannot_complete_restored_lifetime");
        n->finish();q.service();check(b->pendingGpuEpochCount()==0,"restored_lifetime_completes_independently");
    }
    {
        GpuResourceRetirement q;
        auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto s=std::make_shared<Completion>();
        std::vector<std::shared_ptr<GpuResourceRetirement::Ticket>> ts;
        for(unsigned i=0;i<8;++i){auto t=q.begin(b,s);check(bool(t),"admit");t->end();submitted(*t);ts.push_back(t);}
        s->polls=0;q.service();check(s->polls==1,"one_poll_per_context_lifetime_owner_pass");
        for(auto& t:ts)t->finish();q.service();
        check(b->pendingGpuEpochCount()==0,"completion_retires_epochs");
    }
    {
        GpuResourceRetirement q;
        auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto s=std::make_shared<Completion>();
        auto first=q.begin(b,s);first->end();submitted(*first);
        auto second=q.begin(b,s);
        const auto id=b->reserve(resources::ResourceClass::kScratch,4096,true,1024);
        check(bool(id),"staging_admission");resources::ResourceLease lease(b,*id);
        check(second->add(lease.completedShrink(1024)),"staging_permit");
        second->end();submitted(*second);second->finish();q.service();
        check(b->snapshot(resources::ResourceClass::kScratch).staging==3072,
              "observed_later_epoch_keeps_staging_until_ordered_retirement");
        check(!b->gpuEpochRetired(second->epoch),"later_done_waits_for_earlier");
        first->finish();q.service();
        check(b->snapshot(resources::ResourceClass::kScratch).staging==0,
              "ordered_retirement_releases_staging");
        lease.reset();check(b->used()==0,"last_release_drains_quota");
    }
    {
        GpuResourceRetirement q;
        auto b=std::make_shared<resources::ResourceBudgetManager>();
        auto old=std::make_shared<Completion>(),fresh=std::make_shared<Completion>();
        auto a=q.begin(b,old);a->end();submitted(*a);
        auto n=q.begin(b,fresh);n->end();submitted(*n);
        old->polls=fresh->polls=0;old->onPoll=[&]{q.service();};
        q.service();check(old->polls==1&&fresh->polls==1,"distinct_lifetimes_polled_once_nonreentrant");
        old->onPoll={};old->value->abandoned=true;a->finish();q.service();
        check(!b->gpuEpochRetired(a->epoch)&&q.blocked(),"unknown_abandon_keeps_epoch_charged");
        n->finish();q.service();check(!q.pending(),"terminal_unknown_not_infinite_retry");
        old->value->physicalLost=true;q.service();
        check(b->pendingGpuEpochCount()==0&&!q.blocked(),"late_physical_loss_retires_ordered_prefix");
        check(old->value->lifetime!=fresh->value->lifetime,"context_lifetime_not_native_handle");
    }
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();auto s=std::make_shared<Completion>();
        auto t=q.begin(b,s);t->expectSubmission();t->end();
        t->finish();check(b->pendingGpuEpochCount()==1,"callback_before_submit_outcome_cannot_grant_completion");
        t->submitted(false);q.service();
        check(!b->gpuEpochRetired(t->epoch)&&!q.pending(),"failed_submit_not_completion_or_busy_retry");
        b->close();q.service();check(b->used()==0,"owner_close_closes_unknown_quota");
    }
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();auto s=std::make_shared<Completion>();
        auto t=q.begin(b,s);t->end();t->unused();q.service();
        check(b->pendingGpuEpochCount()==0&&b->used()==0,"unused_epoch_drains_without_gpu_submit");
        auto first=q.begin(b,s);first->end();submitted(*first);
        const auto p=b->reserve(resources::ResourceClass::kScratch,16,true);resources::ResourceLease lease(b,*p);
        auto last=q.begin(b,s);last->end();submitted(*last);lease.reset();
        q.invalidateDisplays();
        const auto held=b->snapshot(resources::ResourceClass::kScratch);
        check(held.pendingRelease==16&&last->displayCancelled.load(),"cancelled_gpu_lease_stays_charged");
        last->finish();q.service();check(b->snapshot(resources::ResourceClass::kScratch).pendingRelease==16,"later_completion_cannot_release_cancelled_prefix");
        first->finish();q.service();check(b->used()==0,"owner_service_drains_without_new_edit");
    }
    {
        GpuResourceRetirement q;auto b=std::make_shared<resources::ResourceBudgetManager>();auto s=std::make_shared<Completion>();
        std::vector<std::shared_ptr<GpuResourceRetirement::Ticket>> held;
        for(unsigned i=0;i<1024;++i){auto t=q.begin(b,s);if(!t)break;t->end();submitted(*t);held.push_back(t);}
        check(held.size()==1024&&!q.begin(b,s),"existing_1024_structural_guard");
        s->onPoll=[&]{for(auto& t:held)t->finish();};
        auto next=q.begin(b,s);
        check(bool(next),"admission_services_completed_prefix_before_structural_guard");
        s->onPoll={};if(next){next->end();next->unused();}q.service();check(b->used()==0,"saturation_recovery_preserves_quota");
    }
    return failures?1:0;
}
