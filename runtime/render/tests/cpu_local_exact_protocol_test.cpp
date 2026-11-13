#define main cpuw6bPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/cpu_local_exact.hpp"
#include "../src/selection_region_geometry_internal.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using json=nlohmann::json;
semantic::EraseMaskGeometry sweep(double x,double y,double endX,double endY,unsigned n,double radius){
 std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=n;++i)trace.push_back({float(x+(endX-x)*i/n),float(y+(endY-y)*i/n)});
 return *interaction::partialEraseGeometry(trace,{},radius);
}
int main(int argc,char** argv){try{
    require(argc==2,"output");json rows=json::array();
    auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{8U*1024U*1024U,16U*1024U*1024U});
    LocalExactCache cache;PreparedGeometryCache bodies;RenderCacheOptions limits;
    limits.maxBytes=8U*1024U*1024U;limits.sharedMaxBytes=16U*1024U*1024U;limits.maxEntryBytes=8U*1024U*1024U;
    auto entry=source();auto record=entry.record;record.kind=semantic::ObjectKind::kVectorStroke;record.kindVersion=3;
    auto query=sweep(100,64,120,64,64,3);auto index=cache.request(query,budget,limits.maxEntryBytes);require(bool(index),"query index");
    const auto remaining=16U*1024U*1024U-budget->used();const auto pressure=budget->reserve(resources::ResourceClass::kRecorded,remaining,true);require(bool(pressure),"pressure floor");
    auto job=std::make_unique<LocalExactJob>(record,index,cache,bodies,limits,budget);
    PreparationAllowance one;require(job->advance(one)==LocalExactStatus::kPending,"construction pressure must remain pending");
    budget->release(*pressure);bool recovered=false,imported=false;std::uint64_t serial=0;
    for(unsigned turn=0;turn<10000;++turn){PreparationAllowance allowance;allowance.limits.maxCoverageSteps=1;
        const auto status=job->advance(allowance);
        if(status==LocalExactStatus::kHit||status==LocalExactStatus::kMiss){recovered=true;break;}
        require(status!=LocalExactStatus::kInvalid,"resource retry became invalid");
        if(status!=LocalExactStatus::kNeedPiece)continue;
        const std::array<std::uint64_t,8> token{1,2,3,4,5,++serial,1,7};const auto request=job->encode(token);
        const auto decoded=cpuDecodeRequest(request);require(decoded&&decoded->piece&&decoded->fullCount==64,"piece full/range type");
        CpuExactPathJob worker(decoded->geometry);while(!worker.ready())require(worker.advance(8,2000),"worker progress");
        std::vector<std::uint8_t> output(128+worker.path().writeToMemory(nullptr));std::copy_n(request.begin(),128,output.begin());
        const auto word=[&](unsigned p,std::uint32_t n){for(unsigned i=0;i<4;++i)output[p+i]=n>>(8*i);};
        word(0,0x31525043);word(4,1);word(96,output.size()-128);word(100,3);worker.path().writeToMemory(output.data()+128);
        bool pending=false;auto wrong=token;++wrong[5];require(!job->import(output,wrong,&pending),"foreign token");
        auto fork=output;fork[108]^=1;require(!job->import(fork,token,&pending),"foreign range");
        fork=output;fork[116]^=1;require(!job->import(fork,token,&pending),"foreign full witness");
        fork=output;fork[12]^=1;require(!job->import(fork,token,&pending),"foreign primitive count");
        fork=output;fork[100]=2;require(!job->import(fork,token,&pending),"full result masquerading as piece");
        require(job->import(output,token,&pending)&&!pending,"valid piece import");
        require(!job->import(output,token,&pending),"duplicate piece import");imported=true;
        EraseMaskPathCache full;require(!full.lookup(query),"piece falsely published full path");
    }
    require(recovered&&imported,"same job did not recover constructor pressure");
    rows.push_back({{"constructor_pressure_same_job_recovered",true},{"typed_range_negatives",5},{"piece_not_full",true}});
    job.reset();index.reset();cache.clear();bodies.clear();require(budget->used()==0,"cancel/release resource leak");
    // A long negative query validates in bounded pieces and proves Miss without
    // ever requesting a whole path. A completely covered repeat is likewise a
    // complete-value no-op proof after validation, rather than budget exhaustion.
    auto generous=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
    LocalExactCache negativeCache;PreparedGeometryCache negativeBodies;
    limits.maxBytes=256U*1024U*1024U;limits.sharedMaxBytes=512U*1024U*1024U;limits.maxEntryBytes=64U*1024U*1024U;
    for(bool covered:{false,true}){
        auto longQuery=sweep(covered?100:50000,64,covered?120:51000,64,7200,3);
        auto target=record;if(covered)target.eraseMasks.push_back({ObjectId::fromUint64(99),longQuery});
        auto input=negativeCache.request(longQuery,generous,limits.maxEntryBytes);require(bool(input),"long negative index");
        LocalExactJob negative(target,input,negativeCache,negativeBodies,limits,generous);bool done=false;unsigned turns=0;
        for(;turns<20000;++turns){PreparationAllowance allowance;allowance.limits.maxCoverageSteps=1;
            const auto before=negative.statistics().validated;const auto status=negative.advance(allowance);
            require(negative.statistics().validated-before<=32,"validation drained complete history in one step");
            require(status!=LocalExactStatus::kNeedPiece&&status!=LocalExactStatus::kNeedFull,"negative eagerly requested union");
            if(status==LocalExactStatus::kMiss){done=true;break;}
            require(status==LocalExactStatus::kPending,"negative query incorrectly hit/invalid");
        }
        require(done&&turns>=225,"long negative progress or result");
        rows.push_back({{"case",covered?"complete-covered-noop":"long-disjoint-miss"},{"turns",turns},{"union_steps",negative.statistics().pieceUnions}});
    }
    negativeCache.clear();negativeBodies.clear();require(generous->used()==0,"negative cleanup");
    std::ofstream(argv[1])<<json{{"rows",rows},{"used_after_cleanup",budget->used()}}.dump(2)<<'\n';
    std::cout<<"local identity, constructor retry, cleanup verified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
