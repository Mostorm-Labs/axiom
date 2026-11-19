#pragma once
#if defined(AXIOM_NAV_MASK_DIAGNOSTICS)
#include <cstdint>
#include <chrono>
#include <cstdio>
namespace canvas::render::internal::nm {
struct Counts {std::uint64_t oldBuilds=0,oldReplay=0,upperScans=0,sourceUnits=0,validated=0,accounted=0,indexUnits=0,pathBuilds=0,pathCommands=0,candidates=0,drawn=0,resumes=0,bodyStarts=0,maskNs=0,waits=0,pages=0;};inline thread_local Counts counts{};
struct Invocation {
 Counts before=counts;
 ~Invocation(){const auto&a=counts;std::fprintf(stderr,"AXIOM_NM {\"oldBuilds\":%llu,\"oldReplay\":%llu,\"sourceUnits\":%llu,\"validated\":%llu,\"accounted\":%llu,\"indexUnits\":%llu,\"pathBuilds\":%llu,\"pathCommands\":%llu,\"candidates\":%llu,\"drawn\":%llu,\"resumes\":%llu,\"bodyStarts\":%llu,\"maskNs\":%llu,\"waits\":%llu,\"pages\":%llu}\n",(unsigned long long)(a.oldBuilds-before.oldBuilds),(unsigned long long)(a.oldReplay-before.oldReplay),(unsigned long long)(a.sourceUnits-before.sourceUnits),(unsigned long long)(a.validated-before.validated),(unsigned long long)(a.accounted-before.accounted),(unsigned long long)(a.indexUnits-before.indexUnits),(unsigned long long)(a.pathBuilds-before.pathBuilds),(unsigned long long)(a.pathCommands-before.pathCommands),(unsigned long long)(a.candidates-before.candidates),(unsigned long long)(a.drawn-before.drawn),(unsigned long long)(a.resumes-before.resumes),(unsigned long long)(a.bodyStarts-before.bodyStarts),(unsigned long long)(a.maskNs-before.maskNs),(unsigned long long)(a.waits-before.waits),(unsigned long long)(a.pages-before.pages));}
};
struct Scope{std::uint64_t& counter;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();~Scope(){counter+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();}};
}
#define NM_INVOCATION(name) ::canvas::render::internal::nm::Invocation name
#define NM_ADD(n,v) (::canvas::render::internal::nm::counts.n+=(v))
#define NM_SCOPE(name,n) ::canvas::render::internal::nm::Scope name{::canvas::render::internal::nm::counts.n}
#else
#define NM_INVOCATION(...) ((void)0)
#define NM_ADD(...) ((void)0)
#define NM_SCOPE(...) ((void)0)
#endif
