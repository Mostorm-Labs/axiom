#pragma once
#if defined(AXIOM_NAV_SHARING_OBSERVATION)
#include <array>
#include <functional>
#include <bit>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace canvas::render::internal::ns {
inline thread_local std::function<void(unsigned)> gateHook;
inline thread_local std::function<std::chrono::steady_clock::time_point()> clock;
inline auto clockNow(){return clock?clock():std::chrono::steady_clock::now();}
enum Phase:unsigned {Service,Planning,Partition,Compose,FlushSubmit,Present,Acquire,Query,Validation,Geometry,Initialization,Preparation,Step,PostCoverage,ProbeOnly,PhaseCount};
enum Reason:unsigned {None,NoDemand,NoEligible,TimeBeforePrepare,TimeAfterDisplay,TimeLoad,QueryCap,InitCap,StepCap,GpuCommandCap,GpuBackpressure,MetadataDenied,PixelDenied,ProtectedOrResident,Unsupported,InvalidBasis,OutsideVisible,TimeAfterSetup,TimeAdvance,PublishCap,ReadyNoDebt,BoundedWorksetYield,Count};
struct Rect{float l=0,t=0,r=0,b=0;};
struct Job{std::uint64_t id=0;std::int64_t x=0,y=0;std::int32_t scale=0;unsigned region=0,role=0;std::uint64_t source=0,primitive=0,knot=0,steps=0;Rect demand;};
struct Candidate{std::int64_t x=0,y=0;std::int32_t scale=0;unsigned region=0,role=0;double missing=0,quality=0,currentDensity=0,targetDensity=0,benefit=0;};
struct Gate{std::uint64_t elapsed=0;unsigned reason=0,queries=0,inits=0,steps=0,gpu=0;};
struct Span{std::uint64_t begin=0,end=0;unsigned phase=0,depth=0;};
struct Row{
 std::uint64_t sequence=0,begin=0,end=0,businessEnd=0,origin=0,prepare=0,view=0,semantic=0,scene=0,resource=0,surface=0,metrics=0,camera=0,frame=0,presentSequence=0;
 std::array<std::uint32_t,3> cameraBits{};std::array<unsigned,2>dimensions{};
 std::array<std::uint64_t,PhaseCount> inclusive{},exclusive{};std::array<unsigned,PhaseCount>phaseCalls{};std::array<Span,64>spans{};unsigned spanCount=0,spanDrop=0;
 std::uint64_t budgetNs=0,firstGateElapsed=0;unsigned firstReason=0,terminalReason=0,gateCount=0,gateDrop=0;std::array<Gate,8>gates{};
 unsigned level=0,missingCount=0,missingCopied=0,qualityCandidates=0,frontierCandidates=0,knownEmptyCandidates=0;std::uint64_t missingHash=0;std::array<Rect,16>holes{};double missingArea=0,visibleArea=0,lowArea=0,severeArea=0,debtArea=0,contentSupportArea=0,contentLowArea=0,contentSevereArea=0,contentDebtArea=0;
 std::array<Job,2>beforeJobs{},afterJobs{};std::array<Candidate,3>candidates{};Candidate selected{};
 unsigned queries=0,inits=0,steps=0,gpu=0,maxQueries=0,maxInits=0,maxSteps=0,maxGpu=0,maxPreparationTickets=2,actualPreparationTickets=0;
 unsigned contentPublications=0,reselected=0,worksetSize=0,worksetRemaining=0;std::array<std::array<std::int64_t,4>,8> publishedContentKeys{};
 unsigned coverage=0,presented=0,earlyPresented=0,returnCode=255,resourceDenied=0,noEvictable=0,transitions=0;std::uint64_t qualityAdvances=0,frontierAdvances=0,proofPublishes=0;unsigned qualityAge=0;double displayReserveUs=0,checkpointEstimateUs=0;
 std::uint64_t built=0,sourceSteps=0,knotSteps=0,contourDrawn=0,cancelled=0,suspended=0,resumed=0,used=0,staging=0,pending=0,navUsed=0;
 std::uint64_t planned=0,actual=0,preparedAt=0;double plannedScale=0;
};
struct State{std::array<Row,512>rows{};unsigned count=0,dropped=0;Row*row=nullptr;std::array<unsigned,16>stack{};std::array<std::uint64_t,16>child{};unsigned depth=0;bool enabled=false;std::uint64_t present=0;};
inline State state;static_assert(sizeof(State)<=4U*1024U*1024U);
inline std::uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline void reset(){state.count=state.dropped=state.depth=0;state.row=nullptr;state.present=0;state.enabled=true;}
inline void begin(std::uint64_t origin){if(!state.enabled)return;if(state.count==state.rows.size()){++state.dropped;state.row=nullptr;return;}auto&r=state.rows[state.count++];std::memset(&r,0,sizeof(r));r.sequence=state.count;r.begin=now();r.origin=origin;r.returnCode=255;state.row=&r;}
inline void finish(){if(state.row){state.row->end=now();state.row=nullptr;}if(state.depth){++state.dropped;state.depth=0;}}
struct End{~End(){finish();}};
struct Scope{unsigned p=0,depth=0;std::uint64_t start=0;bool active=false;explicit Scope(unsigned phase):p(phase){if(!state.row)return;if(state.depth==16){++state.dropped;return;}active=true;depth=state.depth++;state.stack[depth]=phase;state.child[depth]=0;start=now();}~Scope(){if(!active)return;const auto elapsed=now()-start;auto*r=state.row;if(!r)return;if(state.depth!=depth+1){++state.dropped;return;}--state.depth;r->inclusive[p]+=elapsed;r->exclusive[p]+=elapsed-std::min(elapsed,state.child[depth]);++r->phaseCalls[p];if(r->spanCount<r->spans.size())r->spans[r->spanCount++]={start,start+elapsed,p,depth};else ++r->spanDrop;if(depth)state.child[depth-1]+=elapsed;}};
inline void gate(Reason reason,unsigned q=0,unsigned i=0,unsigned s=0,unsigned g=0){auto*r=state.row;if(!r)return;const auto elapsed=now()-r->origin;if(!r->firstReason){r->firstReason=reason;r->firstGateElapsed=elapsed;}r->terminalReason=reason;if(r->gateCount<r->gates.size())r->gates[r->gateCount++]={elapsed,reason,q,i,s,g};else++r->gateDrop;}
inline double intersection(Rect a,Rect b){return std::max(0.,double(std::min(a.r,b.r)-std::max(a.l,b.l)))*std::max(0.,double(std::min(a.b,b.b)-std::max(a.t,b.t)));}
inline std::array<Rect,4>content{};inline unsigned contentCount=0;
inline double contentArea(Rect r){std::array<Rect,32>holes{};unsigned n=1;holes[0]=r;double area=0;for(unsigned k=0;k<contentCount;++k){std::array<Rect,32>next{};unsigned count=0;for(unsigned i=0;i<n;++i){const auto h=holes[i],c=content[k];Rect x{std::max(h.l,c.l),std::max(h.t,c.t),std::min(h.r,c.r),std::min(h.b,c.b)};if(x.r<=x.l||x.b<=x.t){if(count<32)next[count++]=h;continue;}area+=(x.r-x.l)*(x.b-x.t);const auto add=[&](Rect v){if(v.r>v.l&&v.b>v.t&&count<32)next[count++]=v;};add({h.l,h.t,x.l,h.b});add({x.r,h.t,h.r,h.b});add({x.l,h.t,x.r,x.t});add({x.l,x.b,x.r,h.b});}holes=next;n=count;}return area;}
template<class F>struct Exit{F f;~Exit(){f();}};template<class F>Exit(F)->Exit<F>;
template<class T>T result(T value){if(state.row)state.row->returnCode=unsigned(value.code);return value;}
template<class T>Rect rect(const T&r){return {r.left,r.top,r.right,r.bottom};}
template<class T>Job job(const T*j){if(!j)return {};return {j->id,j->key.x,j->key.y,j->key.scale,j->key.region,j->prewarm?8U:0U,j->source,j->primitive,j->knot,j->steps,rect(j->demand)};}
inline std::uint64_t checksum(){std::uint64_t h=1469598103934665603ULL;for(unsigned i=0;i<state.count;++i)for(const auto b:std::bit_cast<std::array<unsigned char,sizeof(Row)>>(state.rows[i]))h=(h^b)*1099511628211ULL;return h;}
}
#define NS_IF(...) do{if(::canvas::render::internal::ns::state.row){__VA_ARGS__;}}while(false)
#define PC_NAV_NOW() ::canvas::render::internal::ns::clockNow()
#define PC_NAV_TICK() do{if(::canvas::render::internal::ns::gateHook)::canvas::render::internal::ns::gateHook(0);}while(false)
#define NS_SCOPE(name,phase) ::canvas::render::internal::ns::Scope name(::canvas::render::internal::ns::phase)
#define NS_GATE(reason,...) ::canvas::render::internal::ns::gate(::canvas::render::internal::ns::reason __VA_OPT__(,) __VA_ARGS__)
#else
namespace canvas::render::internal::ns {
enum Phase:unsigned {Service,Planning,Partition,Compose,FlushSubmit,Present,Acquire,Query,Validation,Geometry,Initialization,Preparation,Step,PostCoverage,ProbeOnly,PhaseCount};
struct Scope{explicit Scope(unsigned){}};
template<class F>struct Exit{F f;~Exit(){f();}};template<class F>Exit(F)->Exit<F>;
template<class T>T result(T value){return value;}
}
#define NS_IF(...) ((void)0)
#define PC_NAV_NOW() std::chrono::steady_clock::now()
#define PC_NAV_TICK() ((void)0)
#define NS_SCOPE(...) ((void)0)
#define NS_GATE(...) ((void)0)
#endif
