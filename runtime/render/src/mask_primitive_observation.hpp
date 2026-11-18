#pragma once
// Default-OFF, layout-neutral observation. No resource ownership or policy.
#if defined(AXIOM_MASK_PRIMITIVE_OBSERVATION)
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
namespace canvas::render::internal::mps {
enum Metric:unsigned{Lookup,Eligible,Reused,NotResident,NewBuild,NavHit,Transient,Resident,SourceUnit,Command,Page,Candidate,Draw,Clear,Request,Count};
enum Phase:unsigned{RequestPhase,PreparePhase,SelectPhase,LookupPhase,AllocatePhase,BuildPhase,RetirePhase,DrawPhase,PhaseCount};
struct Partition {const void* owner=nullptr;std::array<std::uint64_t,3> bytes{};};
struct Charge {const void* key=nullptr;const void* owner=nullptr;std::uint64_t bytes=0;unsigned kind=0;};
struct State{std::array<std::uint64_t,Count> counts{};std::array<std::uint64_t,PhaseCount> ns{};std::array<std::uint64_t,20> usage{};std::array<Partition,1024> parts{};std::array<Charge,65536> charges{};std::uint64_t sequence=0,dropped=0,overflow=0,last=0;unsigned phase=PhaseCount;};
inline thread_local State state;
inline void add(Metric m){++state.counts[m];}
inline std::uint64_t now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline std::uint64_t mix(std::uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;return x;}
inline Partition* part(const void* key){const auto h=mix(reinterpret_cast<std::uintptr_t>(key))%state.parts.size();for(unsigned n=0;n<64;++n){auto&p=state.parts[(h+n)%state.parts.size()];if(!p.owner||p.owner==key){p.owner=key;return &p;}}++state.overflow;return nullptr;}
inline void charge(const void* key,const void* owner,unsigned kind,std::uint64_t bytes){const auto h=mix(reinterpret_cast<std::uintptr_t>(key))%state.charges.size();for(unsigned n=0;n<128;++n){auto&c=state.charges[(h+n)%state.charges.size()];if(!c.key){c={key,owner,bytes,kind};if(auto*p=part(owner))p->bytes[kind]+=bytes;return;}}++state.overflow;}
inline void uncharge(const void* key){const auto h=mix(reinterpret_cast<std::uintptr_t>(key))%state.charges.size();for(unsigned n=0;n<128;++n){auto&c=state.charges[(h+n)%state.charges.size()];if(c.key==key){if(auto*p=part(c.owner))p->bytes[c.kind]-=c.bytes;c={};return;}}++state.overflow;}
inline void checkpoint(unsigned phase){
#if !defined(__EMSCRIPTEN__)
 const auto t=now();if(state.phase<PhaseCount)state.ns[state.phase]+=t-state.last;state.last=t;state.phase=phase;
#else
 (void)phase;
#endif
}
struct Call{unsigned parent,phase;std::uint64_t start=0;explicit Call(unsigned p):parent(state.phase),phase(p){
#if defined(__EMSCRIPTEN__)
 start=now();
#else
 checkpoint(p);
#endif
}~Call(){
#if defined(__EMSCRIPTEN__)
 state.ns[phase]+=now()-start;
#else
 checkpoint(parent);
#endif
}};
struct Invocation{std::array<std::uint64_t,Count> before=state.counts;std::array<std::uint64_t,PhaseCount> phases=state.ns;std::uint64_t start=now();std::array<std::uint64_t,8> identity;const void* target;Invocation(std::array<std::uint64_t,8> i,const void*t):identity(i),target(t){}~Invocation(){if(state.sequence>=30000){++state.dropped;return;}std::fprintf(stderr,"AXIOM_MPS {\"sequence\":%llu,\"dropped\":%llu,\"overflow\":%llu,\"elapsedNs\":%llu,\"target\":%llu,\"identity\":[",(unsigned long long)++state.sequence,(unsigned long long)state.dropped,(unsigned long long)state.overflow,(unsigned long long)(now()-start),(unsigned long long)reinterpret_cast<std::uintptr_t>(target));for(unsigned i=0;i<8;++i)std::fprintf(stderr,"%s%llu",i?",":"",(unsigned long long)identity[i]);std::fprintf(stderr,"],\"counts\":[");for(unsigned i=0;i<Count;++i)std::fprintf(stderr,"%s%llu",i?",":"",(unsigned long long)(state.counts[i]-before[i]));std::fprintf(stderr,"],\"phaseNs\":[");for(unsigned i=0;i<PhaseCount;++i)std::fprintf(stderr,"%s%llu",i?",":"",(unsigned long long)(state.ns[i]-phases[i]));std::fprintf(stderr,"],\"usage\":[");for(unsigned i=0;i<state.usage.size();++i)std::fprintf(stderr,"%s%llu",i?",":"",(unsigned long long)state.usage[i]);std::fprintf(stderr,"]}\n");}};
}
#define MPS_ADD(n) ::canvas::render::internal::mps::add(::canvas::render::internal::mps::n)
#define MPS_IF(...) do{__VA_ARGS__;}while(false)
#define MPS_CALL(name,p) ::canvas::render::internal::mps::Call name{::canvas::render::internal::mps::p}
#define MPS_PHASE(p) ::canvas::render::internal::mps::checkpoint(::canvas::render::internal::mps::p)
#else
#define MPS_ADD(...) ((void)0)
#define MPS_IF(...) ((void)0)
#define MPS_CALL(...) ((void)0)
#define MPS_PHASE(...) ((void)0)
#endif
