#include "canvas/foundation/internal/g5_observation.hpp"
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <utility>
namespace canvas::foundation::internal::g5 {
namespace {
struct Record {std::uint64_t sequence,time;Context context;unsigned kind,tag,count;bool critical;std::array<std::uint64_t,16> values;};
struct Aggregate {std::uint64_t job=0,parent=0,id=0,count=0,inclusive=0,exclusive=0;unsigned tag=0;};
struct TargetGroup {std::uint64_t job=0,id=0,label=0,width=0,height=0,format=0,uses=0,last=0;unsigned route=0,actual=0;bool requested=false;};
struct GpuSlot {Context origin{};std::uint64_t ticket=0,group=0,contextId=0,epoch=0;bool completed=false;};
struct LeaseSlot {Record reserved{},released{};bool used=false,hasRelease=false;};
constexpr std::size_t pageBytes=65536;
constexpr std::size_t normalCapacity=(15*1024*1024-8*pageBytes)/sizeof(Record);
constexpr std::size_t criticalCapacity=1024*1024/sizeof(Record);
std::shared_ptr<Run> current;
std::shared_ptr<Run> callbackRun; // accessed only by atomic shared_ptr operations
std::atomic<Run*> active{nullptr};std::atomic<std::uint64_t> ids{1};
std::mutex controls;std::thread::id owner;
thread_local Context local;thread_local unsigned scopeDepth=0;
thread_local std::array<std::uint64_t,8> ownerValues{};thread_local bool ownerValid=false;
thread_local std::array<std::uint64_t,8> finished{};
thread_local Scope* activeScope=nullptr;
}
struct Run {
 std::array<Record,normalCapacity> normal;
 std::array<Record,criticalCapacity> critical;
 std::array<char,pageBytes> buffer{};
 std::array<Aggregate,512> aggregates{};std::size_t aggregateHighWater=0;
 std::array<TargetGroup,256> targets{};std::size_t targetHighWater=0;
 std::array<GpuSlot,1024> gpuSlots{};std::size_t gpuHighWater=0;
 std::atomic_flag gpuWriting=ATOMIC_FLAG_INIT;std::atomic<std::uint64_t> gpuLive{0};
 std::array<LeaseSlot,512> leases{};std::size_t leaseHighWater=0;
 std::atomic_flag writing=ATOMIC_FLAG_INIT;
 std::atomic<bool> on{false};
 std::atomic<std::uint64_t> lostNormal{0},lostCritical{0},sequence{0},pending{0};
 std::size_t n=0,c=0,dn=0,dc=0;std::uint32_t length=0,id=0;
 std::size_t evictionCursor=0;bool normalSorted=true;
};
static_assert(sizeof(Run)<=16*1024*1024);
namespace {
void storeRecord(Run& r,const Record& record)noexcept {
 const bool critical=record.critical;auto& lost=critical?r.lostCritical:r.lostNormal;
 Record* row=nullptr;
 if(critical){if(r.c<r.critical.size())row=&r.critical[r.c++];else if(r.n<r.normal.size())row=&r.normal[r.n++];
  else {
   r.evictionCursor=std::max(r.evictionCursor,r.dn);
   while(r.evictionCursor<r.n&&r.normal[r.evictionCursor].critical)++r.evictionCursor;
   if(r.evictionCursor<r.n){row=&r.normal[r.evictionCursor++];r.lostNormal.fetch_add(row->kind==static_cast<unsigned>(Kind::Counter)&&row->tag==static_cast<unsigned>(Tag::resource_lease)&&row->count==16&&row->values[0]==6?3:1);r.normalSorted=false;}
  }
 }
 else if(r.n<r.normal.size())row=&r.normal[r.n++];
 if(row){if(r.n>1&&r.normal[r.n-2].sequence>record.sequence)r.normalSorted=false;*row=record;}
 else lost.fetch_add(record.kind==static_cast<unsigned>(Kind::Counter)&&record.tag==static_cast<unsigned>(Tag::resource_lease)&&record.count==16&&record.values[0]==6?3:1);
}
bool sameContext(const Context& a,const Context& b)noexcept {
 return a.event==b.event&&a.job==b.job&&a.attempt==b.attempt&&a.basis==b.basis&&a.span==b.span&&a.route==b.route&&a.purpose==b.purpose;
}
void flushLease(Run& r,LeaseSlot& s)noexcept {
 if(!s.used)return;storeRecord(r,s.reserved);if(s.hasRelease)storeRecord(r,s.released);s.used=s.hasRelease=false;
}
bool compactLease(Run& r,const Record& row)noexcept {
 if(row.kind!=static_cast<unsigned>(Kind::Counter)||row.tag!=static_cast<unsigned>(Tag::resource_lease)||row.count!=8)return false;
 const auto& v=row.values;std::size_t free=r.leaseHighWater;
 for(std::size_t i=0;i<r.leaseHighWater;++i){auto& s=r.leases[i];if(!s.used){if(free==r.leaseHighWater)free=i;continue;}
  const auto& a=s.reserved.values;if(a[1]!=v[1]||a[2]!=v[2])continue;
  if(!sameContext(s.reserved.context,row.context)||a[3]!=v[3]||a[4]!=v[4]||a[5]!=v[5]||v[6]!=0){flushLease(r,s);return false;}
  if(v[0]==2&&!s.hasRelease){s.released=row;s.hasRelease=true;return true;}
  if(v[0]==3&&s.hasRelease){
   Record packed=s.reserved;packed.count=16;packed.values={6,v[1],v[2],v[3],v[4],v[5],0,a[7],s.reserved.time,s.released.time,row.time,s.released.values[7],v[7],s.reserved.sequence,s.released.sequence,row.sequence};
   storeRecord(r,packed);s.used=s.hasRelease=false;return true;
  }
  flushLease(r,s);return false;
 }
 if(v[0]==1&&v[3]==0&&v[6]==0&&row.context.job&&free<r.leases.size()){
  auto& s=r.leases[free];s.reserved=row;s.used=true;s.hasRelease=false;r.leaseHighWater=std::max(r.leaseHighWater,free+1);return true;
 }
 return false;
}
void flushLeases(Run& r,std::uint64_t job=0)noexcept {
 if(r.writing.test_and_set(std::memory_order_acquire)){r.lostNormal.fetch_add(1);return;}
 for(std::size_t i=0;i<r.leaseHighWater;++i)if(!job||r.leases[i].reserved.context.job==job)flushLease(r,r.leases[i]);
 r.writing.clear(std::memory_order_release);
}
void append(Run& r,Context ctx,Kind kind,unsigned tag,std::initializer_list<std::uint64_t> values,bool critical) noexcept {
 auto& lost=critical?r.lostCritical:r.lostNormal;
 if(values.size()>16||r.writing.test_and_set(std::memory_order_acquire)){lost.fetch_add(1);return;}
 Record row{};row.sequence=r.sequence.fetch_add(1);row.time=nowNs();row.context=ctx;row.kind=static_cast<unsigned>(kind);row.tag=tag;row.count=static_cast<unsigned>(values.size());row.critical=critical;
 std::copy(values.begin(),values.end(),row.values.begin());if(!compactLease(r,row))storeRecord(r,row);
 r.writing.clear(std::memory_order_release);
}
bool criticalScope(unsigned tag,bool root) noexcept {
 if(root)return true;
 switch(static_cast<Tag>(tag)){
  case Tag::exact_prepare:case Tag::publication_prepare:case Tag::semantic_apply:case Tag::observer:
  case Tag::history:case Tag::scene_apply:case Tag::reference_build:case Tag::material:case Tag::provider:
  case Tag::history_prepare:case Tag::history_finalize:case Tag::pathops:return true;
  default:return false;
 }
}
}
bool enabled() noexcept {auto* r=active.load(std::memory_order_acquire);return r&&r->on.load(std::memory_order_relaxed)&&owner==std::this_thread::get_id();}
std::uint64_t nowNs() noexcept{return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
std::uint64_t nextId() noexcept{return ids.fetch_add(1,std::memory_order_relaxed);}
Context context() noexcept{return local;}
void OwnerScope::bind(std::initializer_list<std::uint64_t> values)noexcept {
 if(!enabled()||values.size()!=8||bound_)return;
 previous_=ownerValues;previousValid_=ownerValid;bound_=true;
 std::copy(values.begin(),values.end(),ownerValues.begin());ownerValid=true;
 if(local.job)emit(Kind::Basis,Tag::input,values,true);
}
OwnerScope::~OwnerScope(){if(bound_){ownerValues=previous_;ownerValid=previousValid_;}}
std::uint64_t finishedJob(Purpose p) noexcept{return finished[static_cast<unsigned>(p)];}
void emit(Kind k,unsigned t,std::initializer_list<std::uint64_t> v,bool critical) noexcept {auto* r=active.load(std::memory_order_acquire);if(r&&r->on.load(std::memory_order_relaxed))append(*r,local,k,t,v,critical);}
RouteScope::RouteScope(Route r) noexcept:previous_(local.route){if(enabled())local.route=r;}
RouteScope::~RouteScope(){local.route=previous_;}
void observeTarget(std::uintptr_t target,unsigned actual,std::uint64_t w,std::uint64_t h,std::uint64_t format,bool requested) noexcept {
 if(!enabled())return;
 auto& run=*current;std::size_t available=run.targetHighWater;
 if(local.job){for(std::size_t i=0;i<run.targetHighWater;++i){auto& t=run.targets[i];
  if(t.id&&t.job==local.job&&t.route==static_cast<unsigned>(local.route)&&t.label==target&&t.actual==actual&&t.width==w&&t.height==h&&t.format==format&&t.requested==requested){++t.uses;t.last=nowNs();return;}
  if(!t.id&&available==run.targetHighWater)available=i;
 }}
 const auto decision=nextId();
 if(local.job&&available<run.targets.size()){
  auto& t=run.targets[available];t={};t.job=local.job;t.id=decision;t.label=target;t.actual=actual;t.width=w;t.height=h;t.format=format;t.route=static_cast<unsigned>(local.route);t.requested=requested;t.uses=1;t.last=nowNs();run.targetHighWater=std::max(run.targetHighWater,available+1);
 }
 emit(Kind::Decision,Tag::target,{decision,static_cast<unsigned>(local.route),actual,requested?1ULL:0ULL},true);
 emit(Kind::Leaf,Tag::target,{decision,static_cast<unsigned>(local.route),target,actual,w,h,format},true);
}
Scope::Scope(Tag tag,Purpose purpose) noexcept {
 // Session owns the input root. Duplicate nested "input" wrappers carry no
 // separate frozen phase; admission/identity are recorded at their real seams.
 if(!enabled()||(purpose==Purpose::none&&tag==Tag::input))return;
 ++scopeDepth;previous_=local;tag_=static_cast<unsigned>(tag);root_=purpose!=Purpose::none;
 previousScope_=activeScope;activeScope=this;started_=nowNs();
 if(!root_&&(tag==Tag::coverage||tag==Tag::geometry_replay)){
  auto& run=*current;std::size_t available=run.aggregateHighWater;
  for(std::size_t i=0;i<run.aggregateHighWater;++i){const auto& a=run.aggregates[i];
   if(a.id&&a.job==local.job&&a.parent==previous_.span&&a.tag==tag_){aggregate_=static_cast<int>(i);break;}
   if(!a.id&&available==run.aggregateHighWater)available=i;
  }
  if(aggregate_<0&&available<run.aggregates.size()){
   aggregate_=static_cast<int>(available);run.aggregateHighWater=std::max(run.aggregateHighWater,available+1);
   auto& a=run.aggregates[available];a={};a.id=nextId();a.parent=previous_.span;a.job=local.job;a.tag=tag_;
  }
  if(aggregate_>=0)id_=run.aggregates[static_cast<unsigned>(aggregate_)].id;
 }
 if(!id_)id_=nextId();
 if(root_){local.job=id_;local.attempt=id_;local.basis=id_;local.purpose=purpose;
  if(ownerValid)emit(Kind::Basis,Tag::input,{ownerValues[0],ownerValues[1],ownerValues[2],ownerValues[3],ownerValues[4],ownerValues[5],ownerValues[6],ownerValues[7]},true);
 }
 local.span=id_;
 if(aggregate_<0)emit(Kind::Begin,tag_,{id_,static_cast<unsigned>(purpose),previous_.span,started_},criticalScope(tag_,root_));
}
void Scope::finish() noexcept {
 if(!id_)return;const auto end=nowNs(),duration=end-started_,exclusive=duration>=children_?duration-children_:0;
 if(previousScope_)previousScope_->children_+=duration;
 if(aggregate_>=0){auto& a=current->aggregates[static_cast<unsigned>(aggregate_)];++a.count;a.inclusive+=duration;a.exclusive+=exclusive;}
 else emit(Kind::End,tag_,{id_,end,exclusive},criticalScope(tag_,root_));
 if(root_){
  auto& run=*current;for(std::size_t i=0;i<run.aggregateHighWater;++i){auto& a=run.aggregates[i];if(a.id&&a.job==local.job){
   emit(Kind::Counter,a.tag,{9,a.id,a.parent,a.count,a.inclusive,a.exclusive},false);a={};
  }}
  for(std::size_t i=0;i<run.targetHighWater;++i){auto& t=run.targets[i];if(t.id&&t.job==local.job){
   emit(Kind::Counter,Tag::target,{9,t.id,t.route,t.label,t.actual,t.width,t.height,t.format,t.requested?1ULL:0ULL,t.uses,t.last},false);t={};
  }}
  finished[static_cast<unsigned>(local.purpose)]=id_;
  flushLeases(run,local.job);
 }
 activeScope=previousScope_;local=previous_;id_=0;--scopeDepth;
}
GpuToken::GpuToken(std::shared_ptr<Run> r,Context c,std::uint64_t g,std::uint64_t ci) noexcept:run(std::move(r)),origin(c),group(g),contextId(ci){}
GpuToken::GpuToken(GpuToken&& o) noexcept:run(std::move(o.run)),origin(o.origin),group(o.group),contextId(o.contextId),epoch(o.epoch),completed(o.completed.load()){}
GpuToken& GpuToken::operator=(GpuToken&& o) noexcept {run=std::move(o.run);origin=o.origin;group=o.group;contextId=o.contextId;epoch=o.epoch;completed.store(o.completed.load());return *this;}
GpuToken gpuPrepared(std::uint64_t ci,std::uint64_t epoch) noexcept {
 if(!enabled())return {};
 auto r=current;const auto group=nextId();r->pending.fetch_add(1);
 GpuToken token{std::move(r),local,group,ci};token.epoch=epoch;return token;
}
void gpuSubmissionObserved(GpuToken& t,bool accepted) noexcept {if(t.run)append(*t.run,t.origin,Kind::GpuSubmit,0,{t.group,t.contextId,t.epoch,accepted?1ULL:0ULL},true);}
GpuToken gpuSubmitted(std::uint64_t ci,std::uint64_t epoch) noexcept {auto t=gpuPrepared(ci,epoch);gpuSubmissionObserved(t,true);return t;}
void gpuCompleted(GpuToken& t,bool abandoned,bool lost) noexcept {
 if(!t.run||t.completed.exchange(true))return;
 append(*t.run,t.origin,Kind::GpuComplete,0,{t.group,t.contextId,abandoned?1ULL:0ULL,lost?1ULL:0ULL},true);
 t.run->pending.fetch_sub(1,std::memory_order_release);
}
void gpuRetired(GpuToken& t,std::uint64_t epoch) noexcept {if(t.run){append(*t.run,t.origin,Kind::GpuRetire,0,{t.group,epoch},true);t.run.reset();}}
void gpuTicketPrepared(std::uintptr_t ticket,std::uint64_t ci,std::uint64_t epoch) noexcept {
 if(!enabled())return;auto& r=*current;
 if(r.gpuWriting.test_and_set(std::memory_order_acquire)){r.lostCritical.fetch_add(1);return;}
 std::size_t free=r.gpuHighWater;
 for(std::size_t i=0;i<r.gpuHighWater;++i){if(r.gpuSlots[i].ticket==ticket){r.lostCritical.fetch_add(1);r.gpuWriting.clear(std::memory_order_release);return;}if(!r.gpuSlots[i].ticket&&free==r.gpuHighWater)free=i;}
 if(free<r.gpuSlots.size()){
  r.gpuSlots[free]={local,ticket,nextId(),ci,epoch,false};r.gpuHighWater=std::max(r.gpuHighWater,free+1);r.gpuLive.fetch_add(1);r.pending.fetch_add(1);
 }else r.lostCritical.fetch_add(1);
 r.gpuWriting.clear(std::memory_order_release);
}
namespace {
void ticketEvent(std::uintptr_t ticket,unsigned action,bool a=false,bool b=false,std::uint64_t epoch=0) noexcept {
 auto pin=std::atomic_load_explicit(&callbackRun,std::memory_order_acquire);if(!pin)return;auto& r=*pin;
 if(r.gpuWriting.test_and_set(std::memory_order_acquire)){r.lostCritical.fetch_add(1);return;}
 for(std::size_t i=0;i<r.gpuHighWater;++i){auto& slot=r.gpuSlots[i];if(slot.ticket!=ticket)continue;
  if(action==0)append(r,slot.origin,Kind::GpuSubmit,0,{slot.group,slot.contextId,slot.epoch,a?1ULL:0ULL},true);
  else if(action==1&&!slot.completed){append(r,slot.origin,Kind::GpuComplete,0,{slot.group,slot.contextId,a?1ULL:0ULL,b?1ULL:0ULL},true);slot.completed=true;r.pending.fetch_sub(1,std::memory_order_release);}
  else if(action==2){append(r,slot.origin,Kind::GpuRetire,0,{slot.group,epoch},true);slot={};r.gpuLive.fetch_sub(1,std::memory_order_release);}
  break;
 }
 r.gpuWriting.clear(std::memory_order_release);
}
}
void gpuTicketSubmitted(std::uintptr_t t,bool accepted) noexcept {ticketEvent(t,0,accepted);}
void gpuTicketCompleted(std::uintptr_t t,bool abandoned,bool lost) noexcept {ticketEvent(t,1,abandoned,lost);}
void gpuTicketRetired(std::uintptr_t t,std::uint64_t epoch) noexcept {ticketEvent(t,2,false,false,epoch);}
int control(std::uint32_t opcode,std::uint32_t value) noexcept {
 std::unique_lock lock(controls,std::try_to_lock);if(!lock.owns_lock())return -2;
 if(owner!=std::thread::id{}&&owner!=std::this_thread::get_id())return -2;
 if(opcode>6)return -1;
 if(scopeDepth)return -2;
 if(opcode==2){
  if(current&&(current->pending.load(std::memory_order_acquire)||current->gpuLive.load(std::memory_order_acquire)||current.use_count()!=2))return -3;
  std::atomic_store_explicit(&callbackRun,std::shared_ptr<Run>{},std::memory_order_release);
  if(current&&current.use_count()!=1){std::atomic_store_explicit(&callbackRun,current,std::memory_order_release);return -3;}
  active.store(nullptr,std::memory_order_release);current.reset();
  try {auto r=std::make_shared<Run>();r->id=value;current=std::move(r);std::atomic_store_explicit(&callbackRun,current,std::memory_order_release);if(owner==std::thread::id{})owner=std::this_thread::get_id();active.store(current.get(),std::memory_order_release);local={};finished.fill(0);return 0;}
  catch(...){return -4;}
 }
 if(!current)return -1;auto& r=*current;r.length=0;
 if(opcode<=1){if(opcode==0)flushLeases(r);r.on.store(opcode==1,std::memory_order_release);return 0;}
 if(opcode==5){local.event=value;return 0;}
 if(opcode==6){local.event=0;return 0;}
 if(opcode==3){r.length=static_cast<std::uint32_t>(std::snprintf(r.buffer.data(),r.buffer.size(),"{\"clock_ns\":\"%llu\"}",static_cast<unsigned long long>(nowNs())));return 0;}
 flushLeases(r);
 if(r.writing.test_and_set(std::memory_order_acquire))return -2;
 // Only out-of-band drain sorts records displaced by critical priority. No
 // allocation, JSON or sort occurs on the producer/GPU callback path.
 if(!r.normalSorted){std::sort(r.normal.begin()+static_cast<std::ptrdiff_t>(r.dn),r.normal.begin()+static_cast<std::ptrdiff_t>(r.n),[](const auto& a,const auto& b){return a.sequence<b.sequence;});r.normalSorted=true;r.evictionCursor=r.dn;}
 std::size_t offset=0;
 const auto add=[&](const char* format,auto... args){const int n=std::snprintf(r.buffer.data()+offset,r.buffer.size()-offset,format,args...);if(n>0)offset+=static_cast<std::size_t>(n);};
 add("{\"version\":1,\"run\":%u,\"records\":[",r.id);
 bool first=true;
 while((r.dn<r.n||r.dc<r.c)&&offset+2048<r.buffer.size()){
  // Merge the two lanes by actual append sequence, preserving critical order.
  const bool normal=r.dn<r.n&&(r.dc==r.c||r.normal[r.dn].sequence<r.critical[r.dc].sequence);
  const auto& row=normal?r.normal[r.dn++]:r.critical[r.dc++];
  add("%s{\"seq\":\"%llu\",\"at_ns\":\"%llu\",\"event\":\"%llu\",\"job\":\"%llu\",\"attempt\":\"%llu\",\"basis\":\"%llu\",\"purpose\":%u,\"route\":%u,\"kind\":%u,\"tag\":%u,\"values\":[",first?"":",",static_cast<unsigned long long>(row.sequence),static_cast<unsigned long long>(row.time),static_cast<unsigned long long>(row.context.event),static_cast<unsigned long long>(row.context.job),static_cast<unsigned long long>(row.context.attempt),static_cast<unsigned long long>(row.context.basis),static_cast<unsigned>(row.context.purpose),static_cast<unsigned>(row.context.route),row.kind,row.tag);
  first=false;for(unsigned i=0;i<row.count;++i)add("%s\"%llu\"",i?",":"",static_cast<unsigned long long>(row.values[i]));add("%s","]}");
 }
 add("],\"loss\":{\"statistical\":%llu,\"critical\":%llu},\"pending_gpu\":%llu,\"finished\":%s}",static_cast<unsigned long long>(r.lostNormal.load()),static_cast<unsigned long long>(r.lostCritical.load()),static_cast<unsigned long long>(r.pending.load()),r.dn==r.n&&r.dc==r.c?"true":"false");
 r.length=static_cast<std::uint32_t>(offset);r.writing.clear(std::memory_order_release);return 0;
}
const char* bytes() noexcept {return current?current->buffer.data():nullptr;}
std::uint32_t size() noexcept {return current?current->length:0;}
}
#endif
